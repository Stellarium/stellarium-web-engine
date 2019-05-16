/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

Module.afterInit(function() {
  if (!Module.canvas) return;

  var prevTimestamp;

  // Function called at each frame
  var render = function(timestamp) {

    // Check for canvas resize
    var canvas = Module.canvas;
    var displayWidth  = Math.floor(canvas.clientWidth);
    var displayHeight = Math.floor(canvas.clientHeight);
    var sizeChanged = (canvas.width  !== displayWidth) ||
                      (canvas.height !== displayHeight);

    if (sizeChanged) {
      canvas.width = displayWidth;
      canvas.height = displayHeight;
    }

    if (!prevTimestamp)
      prevTimestamp = timestamp;
    var dt = timestamp - prevTimestamp;
    prevTimestamp = timestamp;

    if (Module.onBeforeRendering)
      Module.onBeforeRendering(timestamp)

    // TODO: combine those calls
    Module._core_update_fov(dt / 1000)
    Module._core_observer_update();

    // TODO: manage paning and flicking here

    Module._core_update(dt / 1000);
    Module._core_render(canvas.width, canvas.height, 1);

    window.requestAnimationFrame(render)
  }

  var fixPageXY = function(e) {
    if (e.pageX == null && e.clientX != null ) {
      var html = document.documentElement
      var body = document.body
      e.pageX = e.clientX + (html.scrollLeft || body && body.scrollLeft || 0)
      e.pageX -= html.clientLeft || 0
      e.pageY = e.clientY + (html.scrollTop || body && body.scrollTop || 0)
      e.pageY -= html.clientTop || 0
    }
  };

  var setupMouse = function() {
    var canvas = Module.canvas;
    var mouseDown = false;
    function getMousePos(evt) {
      var rect = canvas.getBoundingClientRect();
      return {
        x: evt.clientX - rect.left,
        y: evt.clientY - rect.top
      };
    }

    canvas.addEventListener('mousedown', function(e) {
      var that = this;
      e = e || event;
      fixPageXY(e);
      mouseDown = true;
      var pos = getMousePos(e);
      Module._core_on_mouse(0, 1, pos.x, pos.y);

      document.onmouseup = function(e) {
        e = e || event;
        fixPageXY(e);
        mouseDown = false;
        var pos = getMousePos(e);
        Module._core_on_mouse(0, 0, pos.x, pos.y);
      };
      document.onmouseleave = function(e) {
        mouseDown = false;
      };

      document.onmousemove = function(e) {
        e = e || event;
        fixPageXY(e);
        var pos = getMousePos(e);
        Module._core_on_mouse(0, mouseDown ? 1 : 0, pos.x, pos.y);
      }
    });

    canvas.addEventListener('touchstart', function(e) {
      var rect = canvas.getBoundingClientRect();
      for (var i = 0; i < e.changedTouches.length; i++) {
        var id = e.changedTouches[i].identifier;
        var relX = e.changedTouches[i].pageX - rect.left;
        var relY = e.changedTouches[i].pageY - rect.top;
        Module._core_on_mouse(id, 1, relX, relY);
      }
    }, {passive: true});
    canvas.addEventListener('touchmove', function(e) {
      e.preventDefault();
      var rect = canvas.getBoundingClientRect();
      for (var i = 0; i < e.changedTouches.length; i++) {
        var id = e.changedTouches[i].identifier;
        var relX = e.changedTouches[i].pageX - rect.left;
        var relY = e.changedTouches[i].pageY - rect.top;
        Module._core_on_mouse(id, -1, relX, relY);
      }
    }, {passive: false});
    canvas.addEventListener('touchend', function(e) {
      var rect = canvas.getBoundingClientRect();
      for (var i = 0; i < e.changedTouches.length; i++) {
        var id = e.changedTouches[i].identifier;
        var relX = e.changedTouches[i].pageX - rect.left;
        var relY = e.changedTouches[i].pageY - rect.top;
        Module._core_on_mouse(id, 0, relX, relY);
      }
    });

    function getMouseWheelDelta(event) {
      var delta = 0;
      switch (event.type) {
        case 'DOMMouseScroll':
          delta = -event.detail;
          break;
        case 'mousewheel':
          delta = event.wheelDelta / 120;
          break;
        default:
          throw 'unrecognized mouse wheel event: ' + event.type;
      }
      return delta;
    }

    var onWheelEvent = function(e) {
      e.preventDefault();
      fixPageXY(e);
      var pos = getMousePos(e);
      var zoom_factor = 1.05;
      var delta = getMouseWheelDelta(e) * 2;
      Module._core_on_zoom(Math.pow(zoom_factor, delta), pos.x, pos.y);
      return false;
    };
    canvas.addEventListener('mousewheel', onWheelEvent, {passive: false});
    canvas.addEventListener('DOMMouseScroll', onWheelEvent, {passive: false});
  };

  setupMouse();

  // Kickoff rendering at max FPS, normally 60 FPS on a browser.
  window.requestAnimationFrame(render)
})
