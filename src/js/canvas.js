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

  // Function to be called each time we think the view might have changed.
  var render = function() {
    if (render.called) return;
    render.called = true;
    window.requestAnimationFrame(function() {
      render.called = false;
      var canvas = Module.canvas;
      if (Module._core_render(canvas.width, canvas.height))
        render(); // Render again if needed.
    });
  }

  var checkForCanvasResize = function () {
    window.requestAnimationFrame(function() {
      var canvas = Module.canvas;
      var displayWidth  = Math.floor(canvas.clientWidth);
      var displayHeight = Math.floor(canvas.clientHeight);
      var sizeChanged = (canvas.width  !== displayWidth) ||
                        (canvas.height !== displayHeight);
      checkForCanvasResize(); // Calls itself at each frame
      if (sizeChanged) {
        canvas.width = displayWidth;
        canvas.height = displayHeight;
        render();
      }
    })
  }

  var setup = function() {
    // Permanently monitor canvas size to trigger refresh if it changed
    checkForCanvasResize();
    setupMouse();
  };

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
    var canvas = $(Module.canvas);
    var mouseDown = false;
    canvas.mousedown(function(e) {
      var that = this;
      e = e || event;
      fixPageXY(e);
      var parentOffset = $(this).offset();
      var relX = e.pageX - parentOffset.left;
      var relY = e.pageY - parentOffset.top;
      mouseDown = true;
      Module._core_on_mouse(0, 1, relX, relY);
      render();

      document.onmouseup = function(e) {
        e = e || event;
        fixPageXY(e);
        var parentOffset = $(that).offset();
        var relX = e.pageX - parentOffset.left;
        var relY = e.pageY - parentOffset.top;
        mouseDown = false;
        Module._core_on_mouse(0, 0, relX, relY);
        render();
      };
      document.onmouseleave = function(e) {
        mouseDown = false;
      };

      document.onmousemove = function(e) {
        e = e || event;
        fixPageXY(e);
        var parentOffset = $(that).offset();
        var relX = e.pageX - parentOffset.left;
        var relY = e.pageY - parentOffset.top;
        Module._core_on_mouse(0, mouseDown ? 1 : 0, relX, relY);
        if (mouseDown) render();
      }
    });

    canvas.on('touchstart', function(e) {
      e = e.originalEvent;
      var parentOffset = $(this).offset();
      for (var i = 0; i < e.changedTouches.length; i++) {
        var id = e.changedTouches[i].identifier;
        var relX = e.changedTouches[i].pageX - parentOffset.left;
        var relY = e.changedTouches[i].pageY - parentOffset.top;
        Module._core_on_mouse(id, 1, relX, relY);
      }
      render();
    });
    canvas.on('touchmove', function(e) {
      e.preventDefault();
      e = e.originalEvent;
      var parentOffset = $(this).offset();
      for (var i = 0; i < e.changedTouches.length; i++) {
        var id = e.changedTouches[i].identifier;
        var relX = e.changedTouches[i].pageX - parentOffset.left;
        var relY = e.changedTouches[i].pageY - parentOffset.top;
        Module._core_on_mouse(id, -1, relX, relY);
      }
      render();
    });
    canvas.on('touchend', function(e) {
      e = e.originalEvent;
      var parentOffset = $(this).offset();
      for (var i = 0; i < e.changedTouches.length; i++) {
        var id = e.changedTouches[i].identifier;
        var relX = e.changedTouches[i].pageX - parentOffset.left;
        var relY = e.changedTouches[i].pageY - parentOffset.top;
        Module._core_on_mouse(id, 0, relX, relY);
      }
      render();
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

    canvas.on('DOMMouseScroll mousewheel', function(e) {
      e = e.originalEvent;
      fixPageXY(e);
      var parentOffset = $(this).offset();
      var relX = e.pageX - parentOffset.left;
      var relY = e.pageY - parentOffset.top;
      var zoom_factor = 1.05;
      var delta = getMouseWheelDelta(e);
      Module._core_on_zoom(Math.pow(zoom_factor, delta), relX, relY);
      render();
      return false;
    });
  };

  Module.change(function() {
    render();
  });

  setup();
  render();
  // Render at 20 fps.
  setInterval(render, 50);
})
