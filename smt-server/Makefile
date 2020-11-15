# Stellarium Web - Copyright (c) 2020 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

.PHONY: setup start

USER_UID := $(shell id -u)
USER_GID := $(shell id -g)
setup: Dockerfile
	# Build docker image for node development
	docker build -t smt-server --build-arg USER_UID=${USER_UID} --build-arg USER_GID=${USER_GID} .
	docker run -it -v "$(PWD):/app" smt-server npm install

start:
	docker run -e NPM_CONFIG_LOGLEVEL=info -it -p 3000:3000 -v "$(PWD):/app" smt-server node app.js

