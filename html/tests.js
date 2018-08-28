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
    stel.observer.utc = new Date(Date.UTC(2009, 8, 6, 17, 0, 0));
    assert(isNear(stel.observer.utc, 55080.71, 0.01));

    stel.observer.longitude = -84.4 * stel.D2R;
    stel.observer.latitude = 33.8 * stel.D2R;
    assert(stel.observer.latitude = 33.8 * stel.D2R);
    stel.observer.azimuth = 90 * stel.D2R;
    stel.observer.altitude = 0;
    assert(JSON.stringify(stel.observer.azalt) === JSON.stringify([0, 1, 0]));
    // Test sun pos.
    var sun = stel.getObj("Sun");
    sun.update();
    assert(isNear(sun.ra, 165.48 * stel.D2R, 0.01));
    assert(isNear(sun.dec,  6.20 * stel.D2R, 0.01));
};

var testSearch = function(stel) {
    var obj
    // Simple object search.
    obj = stel.getObj('jupiter')
    assert(obj)
    // Search from multiple idents.
    obj = stel.getObj(['NO RESULT', 'jupiter'])
    assert(obj)
}

var testCloneObserver = function(stel) {
    var obs = stel.observer.clone();
    var jupiter = stel.getObj('jupiter');
    obs.city = 'CITY GB LONDON';
    assert(obs.city.name != stel.observer.city.name)
    jupiter.update(obs);
    obs.destroy();
};

var testListener = function(stel) {
    var test = 0;
    stel.observer.change('latitude', function() { test++; });
    stel.change(function(obj, attr) {
      if (obj.path == 'core.observer' && attr == 'latitude') test++
    })
    stel.observer.city = "CITY FR PARIS";
    assert(test == 2);
    assert(stel.observer.city.name == 'Paris (FR)');
};

var testCalendar = function(stel) {
    var gotMoonMars = false;
    var onEvent = function(ev) {
        if (ev.o1.name == 'Moon' && ev.o2.name == 'Mars') {
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
    });
    stel.setValue('observer.longitude', 0.1);
    assert(stel.getValue('observer.longitude') === 0.1);
    assert(test);
    test = undefined; // Prevent future callbacks!

    // Test that null object attributes are indeed 'null'
    assert(stel.getValue('selection') === null)
    // Test that object attributes are returned by id string.
    stel.setValue('selection', 'jupiter');
    assert(stel.getValue('selection').toLowerCase() == 'jupiter')
}

var testCreate = function(stel) {

    var obj = stel.createObj('dso', {
      id: 'my dso',
      nsid: 'beefbeefbeefbeef',
      model_data: {
        ra: 0,
        de: 89,
        vmag: 0,
        dimx: 60,
        dimy: 60,
      }
    })
    assert(obj.nsid == 'beefbeefbeefbeef')

    var layer = stel.createLayer({z: 7, visible: true})
    layer.add(obj)
    assert(stel.getObjByNSID('beefbeefbeefbeef'))
}

var testFormat = function(stel) {
    var angle = 15.0 * stel.D2R;
    var o;
    assert(stel.formatAngle(angle) == '+15°00\'00"');
    assert(stel.formatAngle(angle, 'hms') == '+01h00m00s');
    o = stel.a2tf(angle);
    assert(JSON.stringify(o) === JSON.stringify({
      sign: '+', hours: 1, minutes: 0, seconds: 0, fraction: 0}));
    o = stel.a2af(angle);
    assert(JSON.stringify(o) === JSON.stringify({
      sign: '+', degrees: 15, arcminutes: 0, arcseconds: 0, fraction: 0}));
    angle = 15.0001 * stel.D2R;
    o = stel.a2af(angle, 2);
    assert(JSON.stringify(o) === JSON.stringify({
      sign: '+', degrees: 15, arcminutes: 0, arcseconds: 0, fraction: 36}));
}

var testPositions = function(stel) {
    var obs = stel.observer.clone();
    obs.utc = 55080.7083;
    obs.longitude = -84.39 * stel.D2R;
    obs.latitude = 33.75 * stel.D2R;
    var o = stel.getObj('Polaris');
    o.update(obs);
    var icrs = o.icrs;
    var cirs = stel.convertPosition(obs, 'ICRS', 'CIRS', icrs);
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
        testSearch(stel);
        testCloneObserver(stel);
        testListener(stel);
        testCalendar(stel);
        testTree(stel);
        testCreate(stel);
        testFormat(stel);
        testPositions(stel);
        console.log('All tests passed');
    }
});

