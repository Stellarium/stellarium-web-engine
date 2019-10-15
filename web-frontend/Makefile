# Stellarium Web - Copyright (c) 2019 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

.PHONY: setup build dev start

update-engine:
	cd .. && make gen-es6
	cp ../stellarium-web-engine.js src/assets/js/
	cp ../stellarium-web-engine.wasm src/assets/js/

update-engine-debug:
	cd .. && make gen-es6-debug
	cp ../stellarium-web-engine.js src/assets/js/
	cp ../stellarium-web-engine.wasm src/assets/js/

setup: Dockerfile
	docker build -t stellarium-web-dev .
	docker run -it -v "$(PWD):/app" stellarium-web-dev yarn

src/assets/js/stellarium-web-engine.js: update-engine
src/assets/js/stellarium-web-engine.wasm: update-engine

dev: src/assets/js/stellarium-web-engine.js src/assets/js/stellarium-web-engine.wasm
	docker run -it -p 8080:8080 -p 8888:8888 -v "$(PWD):/app" stellarium-web-dev yarn run dev

build: src/assets/js/stellarium-web-engine.js src/assets/js/stellarium-web-engine.wasm
	docker run -it -v "$(PWD):/app" stellarium-web-dev yarn run build

start:
	cd dist && python -m SimpleHTTPServer
