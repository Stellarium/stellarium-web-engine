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
  var obj_get_by_nsid_str = Module.cwrap('obj_get_by_nsid_str',
    'number', ['number', 'string']);
  var obj_get_id = Module.cwrap('obj_get_id', 'string', ['number']);
  var obj_get_nsid_str = Module.cwrap('obj_get_nsid_str',
    'void', ['number', 'number']);
  var obj_add_res = Module.cwrap('obj_add_res', 'number',
    ['number', 'string', 'string']);
  var obj_add = Module.cwrap('obj_add', null, ['number', 'number']);
  var args_format_json_str = Module.cwrap('args_format_json_str',
    'number', ['string']);
  var obj_get_tree = Module.cwrap('obj_get_tree', 'number',
    ['number', 'number']);
  var obj_get_path = Module.cwrap('obj_get_path', 'number', ['number']);
  var obj_create_str = Module.cwrap('obj_create_str', 'number',
    ['string', 'string', 'number', 'string'])

  // List of {obj, attr, callback}
  var g_listeners = [];

  var SweObj = function(v) {
    assert(typeof(v) === 'number')
    this.v = v
    this.swe_ = 1
    var that = this

    // Create all the dynamic attributes of the object.
    var callback = Module.addFunction(function(attr, isProp, user) {
      var name = Module.Pointer_stringify(attr);
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
      var id = Module.Pointer_stringify(id);
      if (!id) return; // Child with no id?
      Object.defineProperty(that, id, {
        enumerable: true,
        get: function() {
          var obj_get = Module.cwrap('obj_get', 'number',
            ['number', 'string', 'number'])
          var obj = obj_get(that.v, id, 0)
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

  SweObj.prototype.update = function(obs) {
    obs = obs || Module.observer;
    Module._obj_update(this.v, obs.v, 0.0);
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
      obj_add(this.v, obj.v)
      return obj
    } else {
      args = JSON.stringify(args)
      var id = args ? args.id : undefined;
      var ret = obj_create_str(type, id, this.v, args);
      return ret ? new SweObj(ret) : null;
    }
  }

  // XXX: deprecated: use names instead.
  SweObj.prototype.ids = function(f) {
    var callback = Module.addFunction(function(k, v, u) {
      k = Module.Pointer_stringify(k);
      v = Module.Pointer_stringify(v);
      f(k, v);
    }, 'viii');
    Module._obj_get_ids(this.v, callback, 0);
    Module.removeFunction(callback);
  };

  SweObj.prototype.names = function() {
    var ret = [];
    var callback = Module.addFunction(function(cat, v, u) {
      cat = Module.Pointer_stringify(cat);
      v = Module.Pointer_stringify(v);
      ret.push(cat + ' ' + v);
    }, 'viii');
    Module._obj_get_ids(this.v, callback, 0);
    Module.removeFunction(callback);
    return ret;
  };

  // XXX: deprecated.
  SweObj.prototype.getTree = function(detailed) {
    detailed = (detailed !== undefined) ? detailed : false
    var cret = obj_get_tree(this.v, detailed)
    var ret = Module.Pointer_stringify(cret)
    Module._free(cret)
    ret = JSON.parse(ret)
    return ret
  };

  // Add id property
  Object.defineProperty(SweObj.prototype, 'id', {
    get: function() {
      return obj_get_id(this.v);
    }
  })

  // Add nsid property
  Object.defineProperty(SweObj.prototype, 'nsid', {
    get: function() {
      var p = Module._malloc(17)
      obj_get_nsid_str(this.v, p)
      var ret = Module.Pointer_stringify(p)
      Module._free(p)
      return ret
    }
  })

  // Add path property to the objects.
  Object.defineProperty(SweObj.prototype, 'path', {
    get: function() {
      var cret = obj_get_path(this.v)
      var ret = Module.Pointer_stringify(cret)
      Module._free(cret)
      return ret
    }
  });

  // Add icrs, same as radec for the moment!
  Object.defineProperty(SweObj.prototype, 'icrs', {
    get: function() { return this.radec }
  })

  SweObj.prototype._call = function(attr, args) {
    args = args || []
    args = Array.prototype.slice.call(args)
    args = JSON.stringify(args)
    var cret = obj_call_json_str(this.v, attr, args)
    var ret = Module.Pointer_stringify(cret)
    Module._free(cret)
    if (!ret) return null;
    ret = JSON.parse(ret)
    if (!ret.swe_) return ret;
    if (ret.hint === 'obj') return ret.v ? new SweObj(ret.v) : null;
    return ret.v;
  }

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

  // Return a child object by nsid.
  //
  // Inputs:
  //  nsid     NSID as a 16 bytes hex string.
  Module['getObjByNSID'] = function(nsid) {
    var obj = obj_get_by_nsid_str(0, nsid);
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

  // XXX: deprecated
  Module['add'] = function(data, base_path) {
    var obj_add_res = Module.cwrap('obj_add_res', 'number',
      ['number', 'string', 'string']);
    data = JSON.stringify(data);
    var ret = obj_add_res(Module.core.v, data, base_path);
    return ret ? new SweObj(ret) : null;
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
    attr = Module.Pointer_stringify(attr);
    for (var i = 0; i < g_listeners.length; i++) {
      var listener = g_listeners[i];
      if (    (listener.obj === null || listener.obj === objPtr) &&
        (listener.attr === null || listener.attr === attr)) {
        var obj = new SweObj(objPtr);
        listener.callback.apply(listener.ctx, [obj, attr]);
      }
    }
  }, 'vii');
  Module._obj_add_global_listener(onObjChanged);


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
    var obj = Module.getObj(objPath);
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
    var obj = Module.getObj(objPath);
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
