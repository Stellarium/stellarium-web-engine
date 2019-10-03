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
  for (let i = 0; i < 4; i++) {
    Module._setValue(ptr + i * 4, color[i], 'float');
  }
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
 *
 * Apply a filter function to all the features of the geojson.
 *
 * The callback function takes as input the index of a feature and can
 * return either:
 *
 *  - false, to hide the feature.
 *  - true, to show the feature unchanged.
 *  - A dict of values to change colors:
 *      fill    - Array of 4 float values.
 *      stroke  - Array of 4 float values.
 *      visible - Boolean (default to true).
 */
function filterAll(obj, callback) {
  const features = obj._features;

  const fn = Module.addFunction(function(idx, fillPtr, strokePtr) {
    const r = callback(idx, features[idx]);
    if (r === false) return 0;
    if (r === true) return 1;
    if (r.fill) fillColorPtr(r.fill, fillPtr);
    if (r.stroke) fillColorPtr(r.stroke, strokePtr);
    return r.visible === false ? 0 : 1;
  }, 'iiii');

  Module._geojson_filter_all(obj.v, fn);
  Module.removeFunction(fn);
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
      filterFn = Module.addFunction(function(id, fillPtr, strokePtr) {
        const r = filter(id);
        if (r === false) return 0;
        if (r === true) return 1;
        if (r.fill) fillColorPtr(r.fill, fillPtr);
        if (r.stroke) fillColorPtr(r.stroke, strokePtr);
        return r.visible === false ? 0 : 1;
      }, 'iiii');
      obj._call('filter', filterFn);
    }
  });

  obj.setData = function(data) { setData(obj, data); }
  obj.filterAll = function(callback) { filterAll(obj, callback); }
};
