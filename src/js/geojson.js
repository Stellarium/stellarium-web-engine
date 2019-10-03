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
  function fillColorPtr(color, ptr) {
    for (let i = 0; i < 4; i++) {
      Module._setValue(ptr + i * 4, color[i], 'float');
    }
  }
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

};
