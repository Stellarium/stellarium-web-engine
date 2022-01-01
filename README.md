Stellarium Web Engine
=====================

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

    # Now see apps/simple-html/ to try the library in a browser.

Build the Docker version
------------------------

A `Dockerfile` is included that allows you to quickly build a functional
web engine for further development.

    # compile the current source code
    docker build -t webengine .
    
    # run the web engine in a web server
    docker run -it --rm -p 8080:80 webengine

Once running, browse to http://localhost:8080/simple-html/stellarium-web-engine.html
to open the web engine


Contributing
------------

In order for your contribution to Stellarium Web Engine to be accepted, you have to sign the
[Stellarium Web Contributor License Agreement (CLA)](doc/cla/sign-cla.md).
