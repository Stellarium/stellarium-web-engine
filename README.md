Stellarium Web Engine
=====================

Version 0.1

About
-----

Stellarium Web Engine is a JavaScript planetarium renderer using
WebGL that can be embedded into a website.


Features
--------

- Atmosphere simulation.
- Gaia stars database access (more than 1 billion stars).
- HiPS surveys rendering.
- High resolution planet textures.
- Constellations.
- Support for adding layers and shapes in the sky view.
- Landscapes.


Build the javascript version
----------------------------

You need to make sure you have both emscripten and sconstruct installed.

    # Setup emscripten path.
    source $PATH_TO_EMSDK/emsdk_env.sh

    # Build stellarium-web-engine.js and stellarium-web-engine.wasm
    # This will also copy the files into html/static/js
    make js

    # Now we can open html/test-page.html in a browser.


Contributing
------------

In order for your contribution to Stellarium Web Engine to be accepted, you have to sign the
[Stellarium Web Contributor License Agreement (CLA)](doc/cla/sign-cla.md).
