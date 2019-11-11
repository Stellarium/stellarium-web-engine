
all:
	./tools/checkstyle.py && scons -j8

release:
	scons -j8 debug=0

debug:
	scons -j8 debug=1

clean:
	scons -c

run:
	./stellarium-web-engine

profile:
	scons profile=1 debug=0

# Start with remotery server running so we can do real time profiling.
remotery:
	scons -j8 debug=0 remotery=1

analyze:
	scan-build scons analyze=1

.PHONY: js
js:
	emscons scons -j8 debug=0 emscripten=1

.PHONY: js-debug
js-debug:
	emscons scons -j8 debug=1 emscripten=1

.PHONY: js-prof
js-prof:
	emscons scons -j8 debug=0 profile=1 emscripten=1

.PHONY: js-es6
js-es6:
	emscons scons -j8 debug=0 es6=1 emscripten=1
  
.PHONY: js-es6-debug
js-es6-debug:
	emscons scons -j8 debug=1 es6=1 emscripten=1

.PHONY: js-es6-prof
js-es6-prof:
	emscons scons -j8 debug=0 profile=1 es6=1 emscripten=1

.PHONY: setup

setup: Dockerfile
	docker build -t swe-dev . # --no-cache

dev:
	docker run -it -p 8000:8000 -v "$(PWD):/app" swe-dev

dev-jsdebug:
	docker run -it -p 8000:8000 -v "$(PWD):/app" swe-dev /bin/bash -c "source /emsdk/emsdk_env.sh && make js-debug" && cd html && python -m SimpleHTTPServer

dev-jsprof:
	docker run -it -p 8000:8000 -v "$(PWD):/app" swe-dev /bin/bash -c "source /emsdk/emsdk_env.sh && make js-prof" && cd html && python -m SimpleHTTPServer


gen-es6:
	docker run -it -p 8000:8000 -v "$(PWD):/app" swe-dev /bin/bash -c "source /emsdk/emsdk_env.sh && make js-es6"

gen-es6-debug:
	docker run -it -p 8000:8000 -v "$(PWD):/app" swe-dev /bin/bash -c "source /emsdk/emsdk_env.sh && make js-es6-debug"

gen-es6-prof:
	docker run -it -p 8000:8000 -v "$(PWD):/app"  swe-dev /bin/bash -c "source /emsdk/emsdk_env.sh && make js-es6-prof"

# Make the doc using natualdocs.  On debian, we only have an old version
# of naturaldocs available, where it is not possible to exclude files by
# pattern.  I don't want to parse the C files (only the headers), so for
# the moment I use a tmp file to copy the sources and remove the C files.
# It's a bit ugly.
.PHONY: doc
doc:
	rm -rf /tmp/swe_src
	cp -rf src /tmp/swe_src
	./stellarium-web-engine --gen-doc > /tmp/swe_src/generated-doc.h
	find /tmp/swe_src -name '*.c' | xargs rm
	mkdir -p doc/ndconfig
	naturaldocs -nag -i /tmp/swe_src -o html doc -p doc/ndconfig
