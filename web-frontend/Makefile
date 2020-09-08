# Stellarium Web - Copyright (c) 2019 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

.PHONY: setup build dev start

test-page:
	cd .. && docker run -it -p 8000:8000 -v "$(PWD)/..:/app" swe-dev /bin/bash -c "source /emsdk/emsdk_env.sh && make js-debug" && python3 -m http.server 8000

dev-jsdebug:
	cd .. && docker run -it -p 8000:8000 -v "$(PWD)/..:/app" swe-dev /bin/bash -c "source /emsdk/emsdk_env.sh && make js-debug" && cd html && python3 -m http.server 8000

dev-jsprof:
	cd .. && docker run -it -p 8000:8000 -v "$(PWD)/..:/app" swe-dev /bin/bash -c "source /emsdk/emsdk_env.sh && make js-prof" && cd html && python3 -m http.server 8000

gen-es6:
	cd .. && docker run -it -p 8000:8000 -v "$(PWD)/..:/app" swe-dev /bin/bash -c "source /emsdk/emsdk_env.sh && make js-es6"

gen-es6-debug:
	cd .. && docker run -it -p 8000:8000 -v "$(PWD)/..:/app" swe-dev /bin/bash -c "source /emsdk/emsdk_env.sh && make js-es6-debug"

gen-es6-prof:
	cd .. && docker run -it -p 8000:8000 -v "$(PWD)/..:/app"  swe-dev /bin/bash -c "source /emsdk/emsdk_env.sh && make js-es6-prof"


update-engine:
	make gen-es6
	cp ../build/stellarium-web-engine.js src/assets/js/
	cp ../build/stellarium-web-engine.wasm src/assets/js/

update-engine-debug:
	make gen-es6-debug
	cp ../build/stellarium-web-engine.js src/assets/js/
	cp ../build/stellarium-web-engine.wasm src/assets/js/

USER_UID := $(shell id -u)
USER_GID := $(shell id -g)

setup: Dockerfile Dockerfile.jsbuild
	# Build docker image for compilation with emscripten
	docker build -f Dockerfile.jsbuild -t swe-dev . # --no-cache
	make update-engine
	# Build docker image for webpack/node development
	docker build -t stellarium-web-dev --build-arg USER_UID=${USER_UID} --build-arg USER_GID=${USER_GID} .
	docker run -it -v "$(PWD):/app" -v "$(PWD)/../data:/data" stellarium-web-dev yarn install

dev:
	docker run -it -p 8080:8080 -p 8888:8888 -v "$(PWD):/app" -v "$(PWD)/../data:/data" stellarium-web-dev yarn run dev

lint:
	docker run -it -p 8080:8080 -p 8888:8888 -v "$(PWD):/app" -v "$(PWD)/../data:/data" stellarium-web-dev yarn run lint

build:
	docker run -it -v "$(PWD):/app" -v "$(PWD)/../data:/data" stellarium-web-dev yarn run build

start:
	cd dist && python3 -m http.server 8080

i18n:
	python3 ./tools/update-i18n-en.py
