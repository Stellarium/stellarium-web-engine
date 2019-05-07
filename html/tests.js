#!/usr/bin/nodejs

var assert = function(cond, msg) {
  if (!cond) {
    msg = msg || "Assertion failed";
    throw new Error(msg);
  }
};

var isNear = function(x, y, delta) {
  return Math.abs(x - y) <= delta;
};

var testCore = function(stel) {
  assert(stel.core.observer.path == "core.observer");
}

var testBasic = function(stel) {
  // Set Atlanta, on 2009/09/06 17:00 UTC
  stel.observer.utc = stel.date2MJD(Date.UTC(2009, 8, 6, 17, 0, 0));
  assert(isNear(stel.observer.utc, 55080.71, 0.01));

  stel.observer.longitude = -84.4 * stel.D2R;
  stel.observer.latitude = 33.8 * stel.D2R;
  assert(stel.observer.latitude = 33.8 * stel.D2R);
  stel.observer.azimuth = 90 * stel.D2R;
  stel.observer.altitude = 0;
  assert(JSON.stringify(stel.observer.azalt) === JSON.stringify([0, 1, 0]));
  // Test sun pos.
  var sun = stel.getObj("Sun");
  var pvo = sun.get('pvo', stel.observer);
  var cirs = stel.convertFrame(stel.observer, 'ICRF', 'CIRS', pvo[0]);
  var ra  = stel.anp(stel.c2s(cirs)[0]);
  var dec = stel.anpm(stel.c2s(cirs)[1]);
  assert(isNear(ra, 165.48 * stel.D2R, 0.01));
  assert(isNear(dec,  6.20 * stel.D2R, 0.01));

  // Test different possible values for passing NULL to the core.
  stel.core.selection = null;
  assert(!stel.core.selection);
  stel.core.selection = undefined;
  assert(!stel.core.selection);
  stel.core.selection = 0;
  assert(!stel.core.selection);
};

var testInfo = function(stel) {
  {
    let jupiter = stel.getObj('Jupiter');
    assert(jupiter);
    let vmag = jupiter.get('VMAG');
    assert(typeof vmag == 'number');
    let phase = jupiter.get('PHASE');
    assert(typeof phase == 'number');
    let radius = jupiter.get('RADIUS');
    assert(typeof radius == 'number');
  }

  {
    let polaris = stel.getObj('HIP 11767');
    assert(polaris);
    let radius = polaris.get('RADIUS');
  }
}

var testIds = function(stel) {
  var o1 = stel.getObj('HIP 11767');
  assert(o1);
  assert(o1.names().includes('NAME Polaris'));
}

var testSearch = function(stel) {
  var obj;
  // Simple object search.
  obj = stel.getObj('jupiter');
  assert(obj);
  // Search from multiple idents.
  obj = stel.getObj(['NO RESULT', 'jupiter']);
  assert(obj);
}

var testCloneObserver = function(stel) {
  var obs = stel.observer.clone();
  var jupiter = stel.getObj('jupiter');
  obs.destroy();
};

var testListener = function(stel) {
  var test = 0;
  stel.observer.change('latitude', function() { test++; });
  stel.change(function(obj, attr) {
    if (obj.path == 'core.observer' && attr == 'latitude') test++;
  })
  stel.observer.latitude = 10.0;
  assert(test == 2);
};

var testCalendar = function(stel) {
  var gotMoonMars = false;
  var onEvent = function(ev) {
    if (ev.o2 && ev.o1.names().includes('NAME Moon') &&
                 ev.o2.names().includes('NAME Mars')) {
      assert(ev.time);
      assert(ev.type);
      assert(ev.desc);
      gotMoonMars = true;
    }
  };
  stel.calendar({
    start: new Date(2017, 1, 1),
    end: new Date(2017, 1, 8),
    onEvent: onEvent
  });
  assert(gotMoonMars);

  // Iterator form.
  gotMoonMars = false;
  var cal = stel.calendar({
    start: new Date(2017, 1, 1),
    end: new Date(2017, 1, 8),
    onEvent: onEvent,
    iterator: true
  });
  while (cal()) {}
  assert(gotMoonMars);
};

var testTree = function(stel) {
  var tree = stel.getTree();
  var test = false;
  stel.onValueChanged(function(path, value) {
    if (test === undefined) return;
    if (path === 'observer.longitude') {
      assert(value === 0.1);
      test = true;
    }
    if (path === 'selection')
      assert(typeof(value) === 'number');
  });
  stel.setValue('observer.longitude', 0.1);
  assert(stel.getValue('observer.longitude') === 0.1);
  assert(test);
  test = undefined; // Prevent future callbacks!

  // Test that null object attributes are indeed 'null'
  assert(stel.getValue('selection') === null);

  // Test that object values are returned as pointer.
  var o = stel.getObj('HIP 11767');
  stel.core.selection = o;
  assert(typeof(stel.getValue('selection')) === 'number');
}

var testCreate = function(stel) {
  var layer = stel.createLayer({z: 7, visible: true});
  var obj1 = stel.createObj('dso', {
    id: 'my dso',
    nsid: '0000000beefbeef1',
    model_data: {
      ra: 0,
      de: 89,
      vmag: 0,
      dimx: 60,
      dimy: 60,
    },
    names: ['NAME 1', 'NAME 2', 'NAME_SINGLE']
  });
  assert(obj1.names().includes('NAME 1'));
  assert(obj1.names().includes('NAME 2'));
  assert(obj1.names().includes('NAME_SINGLE'));

  var obj2 = stel.createObj('tle_satellite', {
    id: 'my sat',
    nsid: '0000000beefbeef2',
    model_data: {
      mag: 2.0,
      norad_number: 19120,
      tle: [
        "1 19120U 88039B   18246.93217385  .00000115  00000-0  77599-4 0  9998",
        "2 19120  71.0141 107.3044 0023686 231.5553 128.3447 14.19191161569077"
      ]
    }
  });

  var obj3 = stel.createObj('mpc_asteroid', {
    "interest": 3.6,
    "match": "NAME 1",
    "model": "mpc_asteroid",
    "model_data": {
      "Aphelion_dist": 2.9760543,
      "Epoch": 2458200.5,
      "G": 0.12,
      "H": 3.34,
      "Hex_flags": "0000",
      "Last_obs": "2018-04-30",
      "M": 352.23052,
      "Node": 80.30992,
      "Orbital_period": 4.6028269,
      "Peri": 73.11528,
      "Perihelion_dist": 2.5580383,
      "Principal_desig": "A899 OF",
      "Semilatus_rectum": 1.3756295,
      "Synodic_period": 1.2775598,
      "Tp": 2458236.78379,
      "U": "0",
      "a": 2.7670463,
      "e": 0.0755347,
      "i": 10.59351,
      "n": 0.21413094,
      "rms": 0.6
    },
    "names": [
      "NAME (1) Ceres",
      "NAME Ceres",
      "NAME A899 OF",
      "NAME 1",
      "NAME (1)",
      "1943 XB"
    ],
    "nsid": "e0000000c593bf28",
    "short_name": "(1) Ceres",
    "types": [
      "MBA",
      "MPl",
      "SSO"
    ]
  });
  assert(obj3.names().includes('MPC (1)'));
  assert(obj3.names().includes('NAME (1) Ceres'));

  layer.add(obj1);
  layer.add(obj2);
}

var testPositions = function(stel) {
  var obs = stel.observer.clone();
  obs.utc = 55080.7083;
  obs.longitude = -84.39 * stel.D2R;
  obs.latitude = 33.75 * stel.D2R;
  var o = stel.getObj('HIP 11767');
  var icrs = o.get('pvo', obs)[0];
  var cirs = stel.convertFrame(obs, 'ICRF', 'CIRS', icrs);
  var a_ra  = stel.anp(stel.c2s(icrs)[0]);
  var a_dec = stel.anpm(stel.c2s(icrs)[1]);
  var ra  = stel.anp(stel.c2s(cirs)[0]);
  var dec = stel.anpm(stel.c2s(cirs)[1]);
}

require('./static/js/stellarium-web-engine.js')({
  wasmFile: './static/js/stellarium-web-engine.wasm',
  onReady: function(stel) {
    testCore(stel);
    testBasic(stel);
    testInfo(stel);
    testIds(stel);
    testSearch(stel);
    testCloneObserver(stel);
    testListener(stel);
    testCalendar(stel);
    testTree(stel);
    testCreate(stel);
    testPositions(stel);
    console.log('All tests passed');
  }
});
