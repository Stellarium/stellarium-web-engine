Simple static test pages
------------------------

Build the main js library at the root of this repo:

    # Setup emscripten path.
    source $PATH_TO_EMSDK/emsdk_env.sh

    # Build stellarium-web-engine.js and stellarium-web-engine.wasm
    # This will also copy the files into html/static/js
    make js

Then from the root directory run a simple web server:

    python3 -m http.server 8000

Browse the files to access stellarium-web-engine.html or debug-page.html
