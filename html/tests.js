#!/usr/bin/nodejs

var polaris
var vega
var sigauct

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

  stel.observer.longitude = -84.388 * stel.D2R;
  stel.observer.latitude = 33.749 * stel.D2R;
  stel.observer.yaw = 90 * stel.D2R;
  stel.observer.pitch = 0;
  stel.observer.refraction = false;
  assert(JSON.stringify(stel.observer.azalt) === JSON.stringify([0, 1, 0]));

  // Test Sun pos.
  {
    let sun = stel.getObj("NAME Sun");
    let pvo = sun.getInfo('pvo', stel.observer);
    let cirs = stel.convertFrame(stel.observer, 'ICRF', 'CIRS', pvo[0]);
    let ra  = stel.anp(stel.c2s(cirs)[0]);
    let dec = stel.anpm(stel.c2s(cirs)[1]);
    assert(isNear(ra, 165.48 * stel.D2R, 0.01));
    assert(isNear(dec,  6.20 * stel.D2R, 0.01));
  }

  // Test Polaris pos.
  {
    assert(polaris);
    let pvo = polaris.getInfo('pvo', stel.observer);
    let cirs = stel.convertFrame(stel.observer, 'ICRF', 'CIRS', pvo[0]);
    let altaz = stel.convertFrame(stel.observer, 'ICRF', 'OBSERVED', pvo[0]);
    let ra  = stel.anp(stel.c2s(cirs)[0]);
    let dec = stel.anpm(stel.c2s(cirs)[1]);
    let az = stel.anp(stel.c2s(altaz)[0]);
    let alt = stel.anp(stel.c2s(altaz)[1]);
    assert(isNear(ra,  41.07 * stel.D2R, 0.01));
    assert(isNear(dec, 89.30 * stel.D2R, 0.01));
    assert(isNear(az, 359.24 * stel.D2R, 0.01));
    assert(isNear(alt, 33.47 * stel.D2R, 0.01));
  }

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
    let jupiter = stel.getObj('NAME Jupiter');
    assert(jupiter);
    let vmag = jupiter.getInfo('VMAG');
    assert(typeof vmag == 'number');
    let phase = jupiter.getInfo('PHASE');
    assert(typeof phase == 'number');
    let radius = jupiter.getInfo('RADIUS');
    assert(typeof radius == 'number');
  }

  {
    assert(polaris);
    let radius = polaris.getInfo('RADIUS');
  }
}

var testIds = function(stel) {
  assert(polaris);
  assert(polaris.designations().includes('NAME Polaris'));
}

var testSearch = function(stel) {
  var obj;
  // Simple object search.
  obj = stel.getObj('NAME Jupiter');
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
  stel.observer.latitude = 33 * stel.D2R;
  assert(test == 2);
};

var testCalendar = function(stel) {
  if (!stel._calendar_create) return;
  var gotMoonMars = false;
  var onEvent = function(ev) {
    if (ev.o2 && ev.o1.designations().includes('NAME Moon') &&
                 ev.o2.designations().includes('NAME Mars')) {
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

var testVisibility = function(stel) {
  stel.observer.utc = stel.date2MJD(Date.UTC(2009, 8, 6, 17, 0, 0));
  stel.observer.longitude = -84.388 * stel.D2R;
  stel.observer.latitude = 33.749 * stel.D2R;
  assert(vega)
  assert(vega.computeVisibility().length === 1);
  assert(polaris)
  assert(JSON.stringify(polaris.computeVisibility()) ==
          '[{"rise":null,"set":null}]');
  assert(sigauct);
  assert(sigauct.computeVisibility().length === 0);
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
  stel.core.selection = polaris;
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
  assert(obj1.designations().includes('NAME 1'));
  assert(obj1.designations().includes('NAME 2'));
  assert(obj1.designations().includes('NAME_SINGLE'));

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
  assert(obj2.jsonData.model_data.norad_number === 19120);

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
  assert(obj3.designations().includes('NAME (1) Ceres'));


  var obj4 = stel.createObj('star', {
    "interest": 2.1444,
    "match": "HR 1234", "model": "star",
    "model_data": {
      "Bmag": 6.39, "Umag": 8.18, "Vmag": 6.39,
      "de": 36.9894734328874, "plx": 6.334, "pm_de": -0.555,
      "pm_ra": -7.449, "ra": 60.3118000083165, "rv": -11.4,
      "spect_t": "A0V"
    }, "names": [ "HD 25152", "HR 1234", "SAO 56899", "HIP 18769",
      "TYC 2369-193-1", "BD+36 805", "Gaia DR2 219547565555375488",
      "Gaia DR1 219547561257131776", "ROT 588", "WEB 3613", "GC 4809",
      "SKY# 6121", "UBV 3900", "UBV M 9865", "AG+36 408", "GCRV 54676",
      "HIC 18769", "PPM 69012", "TD1 2722", "GEN# +1.00025152",
      "GSC 02369-00193", "uvby98 100025152", "2MASS J04011482+3659222" ],
    "short_name": "HD 25152",
    "types": [ "*iC", "*" ]
  });
  assert(obj4.designations().includes('HD 25152'));

  layer.add(obj1);
  layer.add(obj2);

  layer.remove(obj1);
  layer.remove(obj2);
}

var testPositions = function(stel) {
  var obs = stel.observer.clone();
  obs.utc = 55080.7083;
  obs.longitude = -84.39 * stel.D2R;
  obs.latitude = 33.75 * stel.D2R;
  var icrs = polaris.getInfo('pvo', obs)[0];
  var cirs = stel.convertFrame(obs, 'ICRF', 'CIRS', icrs);
  var a_ra  = stel.anp(stel.c2s(icrs)[0]);
  var a_dec = stel.anpm(stel.c2s(icrs)[1]);
  var ra  = stel.anp(stel.c2s(cirs)[0]);
  var dec = stel.anpm(stel.c2s(cirs)[1]);
}

var testGeojson = function(stel) {
  var data = {
    "type": "FeatureCollection",
    "features": []
  };
  var obj = stel.createObj('geojson', {
    data: data
  });
  obj.filter = function(idx) {
    if (idx > 100) return false; // Hide.
    if (idx < 10) return true; // Unchanged.
    return {
      fill: [1, 0, 0, 1],
      stroke: [0, 1, 0, 1]
    };
  }
}

require('../build/stellarium-web-engine.js')({
  wasmFile: '../build/stellarium-web-engine.wasm',
  onReady: function(stel) {

    // Create test stars, re-used in several tests
    polaris = stel.createObj('star', {
      "model": "star",
      "model_data": {
        "Vmag": 2.02,
        "de": 89.26410897, "plx": 7.54, "pm_de": -11.85, "pm_ra": 44.48,
        "ra": 37.95456067, "rv": -16.42, "spect_t": "F8Ib"
      },
      "names": ["NAME Polaris", "* alf UMi", "* 1 UMi", "V* alf UMi"],
      "types": ["cC*", "Pu*", "V*", "*"]
    });

    vega = stel.createObj('star', {
      "model": "star",
      "model_data": {
        "Vmag":0.03,
        "de": 38.783688956, "plx": 130.23, "pm_de": 286.23, "pm_ra": 200.94,
        "ra": 279.234734787, "rv": -20.6, "spect_t": "A0Va"
      },
      "names": ["NAME Vega", "* alf Lyr", "* 3 Lyr", "V* alf Lyr"],
      "types": ["dS*", "Pu*", "V*", "*"]
    });

    sigauct = stel.createObj('star', {
      "match":"HIP 104382",
      "model":"star",
      "model_data": {
        "Vmag": 5.42, "de": -88.9565032486872, "plx": 11.092, "pm_de": 5.612,
        "pm_ra": 26.671, "ra": 317.195180933923, "rv": 11.9,
        "spect_t": "F0IV"
      },
      "names": ["NAME Polaris Australis", "* sig Oct", "V* sig Oct"],
      "types": ["dS*","Pu*","V*","*"]
    });

    testCore(stel);
    testBasic(stel);
    testInfo(stel);
    testIds(stel);
    testSearch(stel);
    testCloneObserver(stel);
    testListener(stel);
    testCalendar(stel);
    testVisibility(stel);
    testTree(stel);
    testCreate(stel);
    testPositions(stel);
    testGeojson(stel);
    console.log('All tests passed');
  }
});
