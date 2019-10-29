/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

Module.afterInit(function() {
  // Init C function wrappers.
  var obj_call_json_str = Module.cwrap('obj_call_json_str',
    'number', ['number', 'string', 'string']);
  var obj_get = Module.cwrap('obj_get',
    'number', ['number', 'string', 'number']);
  var obj_get_id = Module.cwrap('obj_get_id', 'string', ['number']);
  var module_add = Module.cwrap('module_add', null, ['number', 'number']);
  var module_remove = Module.cwrap('module_remove', null, ['number', 'number']);
  var module_get_tree = Module.cwrap('module_get_tree', 'number',
    ['number', 'number']);
  var module_get_path = Module.cwrap('module_get_path', 'number',
    ['number', 'number']);
  var obj_create_str = Module.cwrap('obj_create_str', 'number',
    ['string', 'string', 'string'])
  var module_get_child = Module.cwrap('module_get_child', 'number',
    ['number', 'string']);
  var core_get_module = Module.cwrap('core_get_module', 'number', ['string']);
  var obj_get_info_json = Module.cwrap('obj_get_info_json', 'number',
    ['number', 'number', 'string']);

  // List of {obj, attr, callback}
  var g_listeners = [];

  var SweObj = function(v) {
    assert(typeof(v) === 'number')
    this.v = v
    this.swe_ = 1
    var that = this

    // Create all the dynamic attributes of the object.
    var callback = Module.addFunction(function(attr, isProp, user) {
      var name = Module.UTF8ToString(attr);
      if (!isProp) {
        that[name] = function(args) {
          return that._call(name, args);
        };
      } else {
        Object.defineProperty(that, name, {
          configurable: true,
          enumerable: true,
          get: function() {return that._call(name)},
          set: function(v) {return that._call(name, v)},
        })
      }
    }, 'vii')
    Module._obj_foreach_attr(this.v, 0, callback);
    Module.removeFunction(callback);

    // Also add the children as properties
    var callback = Module.addFunction(function(id) {
      id = Module.UTF8ToString(id);
      if (!id) return; // Child with no id?
      Object.defineProperty(that, id, {
        enumerable: true,
        get: function() {
          var obj = module_get_child(that.v, id)
          return obj ? new SweObj(obj) : null
        }
      })
    }, 'vi')
    Module._obj_foreach_child(this.v, callback)
    Module.removeFunction(callback)
  }

  // Swe objects return their values as id string.
  SweObj.prototype.valueOf = function() {
    return this.id
  }

  SweObj.prototype.update = function() {
    Module._module_update(this.v, 0.0);
  }

  SweObj.prototype.getInfo = function(info, obs) {
    if (obs === undefined)
      obs = Module.observer
    var cret = obj_get_info_json(this.v, obs.v, info)
    if (cret === 0)
      return undefined
    var ret = Module.UTF8ToString(cret)
    Module._free(cret)
    ret = JSON.parse(ret)
    if (!ret.swe_) return ret;
    return ret.v;
  }


  SweObj.prototype.clone = function() {
    return new SweObj(Module._obj_clone(this.v));
  }

  SweObj.prototype.destroy = function() {
    Module._obj_release(this.v);
  }

  SweObj.prototype.retain = function() {
    Module._obj_retain(this.v);
  }

  SweObj.prototype.change = function(attr, callback, context) {
    g_listeners.push({
      'obj': this.v,
      'ctx': context ? context : this,
      'attr': attr,
      'callback': callback
    });
  };

  // Add an object as a child to an object.
  // Can be called in two ways:
  // - obj.add(type, args): create an object of type 'type' with data
  //                        'args', add it to the parent object.
  // - obj.add(child): add an already created object as a child.
  SweObj.prototype.add = function(type, args) {
    if (args === undefined) {
      var obj = type
      module_add(this.v, obj.v)
      return obj
    } else {
      var id = args ? args.id : undefined;
      args = JSON.stringify(args)
      var ret = obj_create_str(type, id, args);
      if (ret) {
        module_add(this.v, ret);
        return new SweObj(ret);
      }
      return null;
    }
  }

  /*
   * Methode: remove
   * Remove a previously added child from a layer
   */
  SweObj.prototype.remove = function(obj) {
    module_remove(this.v, obj.v);
  }

  SweObj.prototype.designations = function() {
    var ret = [];
    var callback = Module.addFunction(function(o, u, cat, v) {
      cat = Module.UTF8ToString(cat);
      v = Module.UTF8ToString(v);
      if (cat) ret.push(cat + ' ' + v);
      else ret.push(v);
    }, 'viiii');
    Module._obj_get_designations(this.v, 0, callback);
    Module.removeFunction(callback);
    // Remove duplicates.
    // This should be done in the C code, but for the moment it's simpler
    // here.
    ret = ret.filter(function(item, pos, self) {
      return self.indexOf(item) == pos;
    });
    return ret;
  };

  /*
   * Function: listObjs
   * Return a list of SweObject for a given module.
   *
   * Arguments:
   *   obs      - An observer.
   *   maxMag   - The maximum magnitude above which objects are discarded.
   *   filter   - a function called for each object returning false if the
   *              object must be filtered out.
   *
   * Return:
   *   An array SweObject. It is the responsibility of the caller to properly
   *   destroy all the objects of the list when they are not needed, by calling
  *    obj.destroy() on each of them.
   *
   */
  SweObj.prototype.listObjs = function(obs, maxMag, filter) {
    var ret = [];
    var callback = Module.addFunction(function(user, objv) {
      var obj = new SweObj(objv);
      if (filter(obj)) {
        obj.retain();
        ret.push(obj);
      }
      return 0;
    }, 'iii');
    Module._module_list_objs2(this.v, obs.v, maxMag, 0, callback);
    Module.removeFunction(callback);
    return ret;
  };

  // XXX: deprecated.
  SweObj.prototype.getTree = function(detailed) {
    detailed = (detailed !== undefined) ? detailed : false
    var cret = module_get_tree(this.v, detailed)
    var ret = Module.UTF8ToString(cret)
    Module._free(cret)
    ret = JSON.parse(ret)
    return ret
  };

  /*
   * Function: computeVisibility
   *
   * Arguments:
   *   obs      - An observer.  If not set use current core observer.
   *   starTime - TT MJD starting time.  If not set use 12 hours before
   *              observer time.
   *   endTime  - TT MJD end time.  If not set use 12 hours after
   *              observer time.
   *
   * Return:
   *   A an array of dicts of the form:
   *   [{rise: <riseTime>, set: <setTime>}]
   *
   */
  SweObj.prototype.computeVisibility = function(args) {
    args = args || {};
    var obs = args.obs || Module.core.observer;
    var startTime = args.startTime || obs.tt - 1 / 2;
    var endTime = args.endTime || obs.tt + 1 / 2;
    var precision = 1 / 24 / 60 / 2;
    var rise = Module._compute_event(obs.v, this.v, 1, startTime,
                                     endTime, precision) || null;
    var set = Module._compute_event(obs.v, this.v, 2, startTime,
                                    endTime, precision) || null;
    // Check if the object is never visible:
    if (rise === null && set === null) {
      var p = this.getInfo('radec', obs);
      p = Module.convertFrame(obs, 'ICRF', 'OBSERVED', p);
      if (p[2] < 0) return [];
    }
    return [{'rise': rise, 'set': set}];
  };

  // Add id property
  Object.defineProperty(SweObj.prototype, 'id', {
    get: function() {
      var ret = obj_get_id(this.v);
      if (ret) return ret;
      // XXX: fallback to first designation.  Should probably be removed!
      return this.designations()[0];
    }
  })

  // Add path property to the objects.
  Object.defineProperty(SweObj.prototype, 'path', {
    get: function() {
      if (this.v === Module.core.v) return 'core';
      var cret = module_get_path(this.v, Module.core.v)
      var ret = Module.UTF8ToString(cret)
      Module._free(cret)
      return 'core.' + ret
    }
  });

  // Add icrs, same as radec for the moment!
  Object.defineProperty(SweObj.prototype, 'icrs', {
    get: function() { return this.radec }
  })

  SweObj.prototype._call = function(attr, arg) {
    // Replace null and undefined to 0.
    if (arg === undefined || arg === null)
      arg = 0
    else
      arg = JSON.stringify(arg)
    var cret = obj_call_json_str(this.v, attr, arg)
    var ret = Module.UTF8ToString(cret)
    Module._free(cret)
    if (!ret) return null;
    ret = JSON.parse(ret)
    if (!ret.swe_) return ret;
    if (ret.type === 'obj') return ret.v ? new SweObj(ret.v) : null;
    return ret.v;
  }

  Module['getModule'] = function(name) {
    var obj = core_get_module(name);
    return obj ? new SweObj(obj) : null;
  };

  // Return a child object by identifier.
  //
  // Inputs:
  //  name      String or list of string.  If this is a list swe
  //            search for the first object matching any of those names.
  Module['getObj'] = function(name) {
    if (name instanceof Array) name = name.join('|');
    var obj = obj_get(0, name, 0);
    return obj ? new SweObj(obj) : null;
  };

  Module['change'] = function(callback, context) {
    g_listeners.push({
      'obj': null,
      'ctx': context ? context : null,
      'attr': null,
      'callback': callback
    });
  };

  Module['getTree'] = function(detailed) {
    return Module.core.getTree(detailed)
  }

  // Create a new layer.
  Module['createLayer'] = function(data) {
    return Module.core.add('layer', data);
  }

  // Conveniance function to convert a js string into an allocated C string.
  function stringToC(str) {
    let size = Module.lengthBytesUTF8(str);
    let ptr = Module._malloc(size + 1);
    // For speed, we only support ascii strings for the moment.
    Module.writeAsciiToMemory(str, ptr, false);
    return ptr;
  }

  // Create a new unparented object.
  Module['createObj'] = function(type, args) {
    // Don't use the emscripten wrapped version of obj_create_str, since
    // it seems to crash with large strings!
    let id = args ? args.id : 0;
    args = args ? stringToC(JSON.stringify(args)) : 0;
    if (id) id = stringToC(id);
    const ctype = stringToC(type);
    let ret = Module._obj_create_str(ctype, id, args);
    Module._free(type);
    Module._free(args);
    if (id) Module._free(id);
    ret = ret ? new SweObj(ret) : null;
    // Add special geojson object methods.
    if (type === 'geojson') Module.onGeojsonObj(ret);
    return ret;
  }

  var onObjChanged = Module.addFunction(function(objPtr, attr) {
    attr = Module.UTF8ToString(attr);
    for (var i = 0; i < g_listeners.length; i++) {
      var listener = g_listeners[i];
      if (    (listener.obj === null || listener.obj === objPtr) &&
        (listener.attr === null || listener.attr === attr)) {
        var obj = new SweObj(objPtr);
        listener.callback.apply(listener.ctx, [obj, attr]);
      }
    }
  }, 'vii');
  Module._module_add_global_listener(onObjChanged);


  // Add some convenience functions to access swe values directly as a
  // tree of attributes.

  Module['getTree'] = function() {
    // TODO: remove obj.getTree and only do it here.
    return Module.core.getTree();
  }

  Module['getValue'] = function(path) {
    path = "core." + path;
    var elems = path.split('.');
    var attr = elems.pop();
    var objPath = elems.join('.');
    var obj = Module.getModule(objPath);
    var value = obj[attr];
    if (value && typeof(value) === 'object' && value.swe_)
      value = value.v;
    return value;
  }

  Module['_setValue'] = Module.setValue; // So that we can still use it!
  Module['setValue'] = function(path, value) {
    path = "core." + path;
    var elems = path.split('.');
    var attr = elems.pop();
    var objPath = elems.join('.');
    var obj = Module.getModule(objPath);
    obj[attr] = value;
  }

  Module['onValueChanged'] = function(callback) {
    Module.change(function(obj, attr) {
      var path = obj.path + "." + attr;
      var value = obj[attr];
      if (value && typeof(value) === 'object' && value.swe_)
        value = value.v;
      path = path.substr(5); // Remove the initial 'core.'
      callback(path, value);
    });
  }

  // Just so that we can use SweObj in pre.js.
  Module['SweObj'] = SweObj;
})
