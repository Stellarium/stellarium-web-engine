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
  var module_get_tree = Module.cwrap('module_get_tree', 'number',
    ['number', 'number']);
  var module_get_path = Module.cwrap('module_get_path', 'number',
    ['number', 'number']);
  var obj_create_str = Module.cwrap('obj_create_str', 'number',
    ['string', 'string', 'number', 'string'])
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
        that[name] = function() {
          return that._call(name, arguments);
        };
      } else {
        Object.defineProperty(that, name, {
          configurable: true,
          enumerable: true,
          get: function() {return that._call(name)},
          set: function(v) {return that._call(name, [v])},
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

  SweObj.prototype.get = function(info, obs) {
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
      var ret = obj_create_str(type, id, this.v, args);
      return ret ? new SweObj(ret) : null;
    }
  }

  // XXX: deprecated: use names instead.
  SweObj.prototype.ids = function(f) {
    var callback = Module.addFunction(function(o, u, k, v) {
      k = Module.UTF8ToString(k);
      v = Module.UTF8ToString(v);
      f(k, v);
    }, 'viiii');
    Module._obj_get_designations(this.v, 0, callback);
    Module.removeFunction(callback);
  };

  SweObj.prototype.names = function() {
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
    if (rise === null && set === null) return [];
    return [{'rise': rise, 'set': set}];
  };

  // Add id property
  Object.defineProperty(SweObj.prototype, 'id', {
    get: function() {
      var ret = obj_get_id(this.v);
      if (ret) return ret;
      return this.names()[0];
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

  SweObj.prototype._call = function(attr, args) {
    args = args || []
    args = Array.prototype.slice.call(args)
    // Replace null and undefined to 0.
    args = args.map(function(x) {
      return (x === undefined || x === null) ? 0 : x;
    });
    args = JSON.stringify(args)
    var cret = obj_call_json_str(this.v, attr, args)
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

  // Create a new unparented object.
  Module['createObj'] = function(type, args) {
    var id = args ? args.id : undefined;
    args = JSON.stringify(args)
    var ret = obj_create_str(type, id, 0, args);
    return ret ? new SweObj(ret) : null;
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
