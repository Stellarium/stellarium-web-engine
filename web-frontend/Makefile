# Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

.PHONY: setup build dev

setup: Dockerfile
	docker build -t stellarium-web-dev .
	docker run -it -v "$(PWD):/app" stellarium-web-dev yarn

dev:
	docker run -it -p 8080:8080 -p 8888:8888 -v "$(PWD):/app" stellarium-web-dev yarn run dev

build:
	docker run -it -v "$(PWD):/app" -e CDN_ENV="$(CDN_ENV)" stellarium-web-dev yarn run build

start:
	cd dist && python -m SimpleHTTPServer
