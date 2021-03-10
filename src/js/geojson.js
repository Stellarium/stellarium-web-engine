/*
 * Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

// Some special methods for geojson objects.
//  setData   - fast way to set the geojson data.
//  filterAll - filter and change the properties of the geojson features.

function fillColorPtr(color, ptr) {
  Module._geojson_set_color_ptr_(ptr, color[0], color[1], color[2], color[3])
}

function fillBoolPtr(value, ptr) {
  Module._geojson_set_bool_ptr_(ptr, value);
}

/*
 * Method: setData
 * Set the geojson data of a geojson object.
 *
 * This method is to be used for very fast geojson creation, by skipping
 * passing the geojson to the core as a string, and also skipping parsing
 * the geojson.
 *
 * For the moment we assume a simple geojson with only single ring polygons!
 */
function setData(obj, data) {

  Module._geojson_remove_all_features(obj.v);

  obj._features = data.features; // Keep it for the filter function.
  for (const feature of data.features) {
    const geo = feature.geometry;
    if (geo.type !== 'Polygon') {
      console.error('Only support polygon geometry');
      continue;
    }
    if (geo.coordinates.length != 1) {
      console.error('Only support single ring polygons');
      continue;
    }
    const coordinates = geo.coordinates[0];
    const size = coordinates.length;
    const ptr = Module._malloc(size * 16);
    for (let i = 0; i < size; i++) {
      Module._setValue(ptr + i * 16 + 0, coordinates[i][0], 'double');
      Module._setValue(ptr + i * 16 + 8, coordinates[i][1], 'double');
    }
    Module._geojson_add_poly_feature(obj.v, size, ptr);
    Module._free(ptr);
  }
}

/*
 * Method: filterAll
 * Deprecated.
 *
 * Apply a filter function to all the features of the geojson.
 *
 * The callback function takes as input the index of a feature and
 * return a dict of values:
 *
 *      fill    - Array of 4 float values.
 *      stroke  - Array of 4 float values.
 *      visible - Boolean (default to true).
 *      blink   - Boolean
 *      hidden  - Boolean
 */
function filterAll(obj, callback) {
  const features = obj._features;

  const fn = Module.addFunction(function(idx, fillPtr, strokePtr) {
    const r = callback(idx, features[idx]);
    if (r === false) return 0;
    if (r === true) return 1;
    if (r.fill) fillColorPtr(r.fill, fillPtr);
    if (r.stroke) fillColorPtr(r.stroke, strokePtr);
    let ret = r.visible === false ? 0 : 1;
    if (r.blink === true) ret |= 2;
    return ret;
  }, 'iiii');

  Module._geojson_filter_all(obj.v, fn);
  Module.removeFunction(fn);
}

function queryRenderedFeatureIds(obj, point) {
  if (typeof(point) === 'object') {
    point = [point.x, point.y];
  }
  const pointPtr = Module._malloc(16);
  Module._setValue(pointPtr + 0, point[0], 'double');
  Module._setValue(pointPtr + 8, point[1], 'double');
  const size = 128; // Max number of results.
  const retPtr = Module._malloc(4 * size);
  const nb = Module._geojson_query_rendered_features(obj.v, pointPtr, size, retPtr);
  let ret = []
  for (let i = 0; i < nb; i++) {
    ret.push(Module._getValue(retPtr + i * 4, 'i32'));
  }
  Module._free(pointPtr);
  Module._free(retPtr);
  return ret;
}

Module['onGeojsonObj'] = function(obj) {

  /*
   * Experimental support for the 'filter' attribute of geojson objects.
   *
   * The special filter attribute can be set to a function that takes as
   * input the index of a feature and can return either:
   *
   *  - false, to hide the feature.
   *  - true, to show the feature unchanged.
   *  - A dict of values to change colors:
   *      fill    - Array of 4 float values.
   *      stroke  - Array of 4 float values.
   *      visible - Boolean (default to true).
   */
  let filterFn = null;
  Object.defineProperty(obj, 'filter', {
    set: function(filter) {
      if (filterFn) Module.removeFunction(filterFn);
      filterFn = Module.addFunction(
          function(img, id, fillPtr, strokePtr, blinkPtr, hiddenPtr) {
        const r = filter(id);
        if (r.fill) fillColorPtr(r.fill, fillPtr);
        if (r.stroke) fillColorPtr(r.stroke, strokePtr);
        if (r.stroke) fillColorPtr(r.stroke, strokePtr);
        if (r.blink !== undefined) fillBoolPtr(r.blink, blinkPtr);
        if (r.hidden !== undefined) fillBoolPtr(r.hidden, hiddenPtr);
      }, 'viiiiii');
      obj._call('filter', filterFn);
    }
  });

  obj.setData = function(data) { setData(obj, data); }
  obj.filterAll = function(callback) { filterAll(obj, callback); }
  obj.queryRenderedFeatureIds = function(point) {
    return queryRenderedFeatureIds(obj, point);
  };
};

// Map of survey tile -> features list.
let g_tiles = {}

function asBox(box) {
  if (!(box instanceof Array)) {
    return [[box.x, box.y], [box.x, box.y]]
  }
  assert(box instanceof Array)
  return box.map(function(v) {
    if (!(v instanceof Array)) {
      return [v.x, v.y]
    }
    return v
  })
}

function surveyQueryRenderedFeatures(obj, box) {
  box = asBox(box)
  const boxPtr = Module._malloc(32);
  Module._setValue(boxPtr +  0, box[0][0], 'double');
  Module._setValue(boxPtr +  8, box[0][1], 'double');
  Module._setValue(boxPtr + 16, box[1][0], 'double');
  Module._setValue(boxPtr + 24, box[1][1], 'double');
  const size = 1024; // Max number of results.
  const tilesPtr = Module._malloc(4 * size);
  const indexPtr = Module._malloc(4 * size);
  const nb = Module._geojson_survey_query_rendered_features(
    obj.v, boxPtr, size, tilesPtr, indexPtr);
  let ret = []
  for (let i = 0; i < nb; i++) {
    const tile = Module._getValue(tilesPtr + i * 4, 'i32*');
    const idx = Module._getValue(indexPtr + i * 4, 'i32');
    ret.push(g_tiles[tile][idx]);
  }
  Module._free(boxPtr);
  Module._free(indexPtr);
  Module._free(tilesPtr);
  return ret;
}

// Called each time a new geojson tile of a survey is loaded.
let onNewTile = function(img, json) {
  json = Module.UTF8ToString(json);
  json = JSON.parse(json);
  g_tiles[img] = json.features;
}
let onNewTileSet = false;

/*
 * Same for Geojson surveys
 */
Module['onGeojsonSurveyObj'] = function(obj) {

  if (!onNewTileSet) {
    Module._geojson_set_on_new_tile_callback(
      Module.addFunction(onNewTile, 'vii'));
    onNewTileSet = true;
  }

  Object.defineProperty(obj, 'filter', {
    set: function(filter) {
      if (obj._filterFn) Module.removeFunction(obj._filterFn);
      obj._filterFn = Module.addFunction(
          function(img, id, fillPtr, strokePtr, blinkPtr, hiddenPtr) {
        let features = g_tiles[img];
        const r = filter(features[id]);
        if (r.fill) fillColorPtr(r.fill, fillPtr);
        if (r.stroke) fillColorPtr(r.stroke, strokePtr);
        if (r.blink !== undefined) fillBoolPtr(r.blink, blinkPtr);
        if (r.hidden !== undefined) fillBoolPtr(r.hidden, hiddenPtr);
      }, 'viiiiii');
      obj._call('filter', obj._filterFn);
    }
  });
  obj.queryRenderedFeatures = function(point) {
    return surveyQueryRenderedFeatures(obj, point);
  };

};
