# Stellarium Web - Copyright (c) 2020 - Noctua Software Ltd
#
# This program is licensed under the terms of the GNU AGPL v3, or
# alternatively under a commercial licence.
#
# The terms of the AGPL v3 license can be found in the main directory of this
# repository.

.PHONY: setup dev

USER_UID := $(shell id -u)
USER_GID := $(shell id -g)
setup: Dockerfile
	# Build docker image for node development
	docker build -t smt-server --build-arg USER_UID=${USER_UID} --build-arg USER_GID=${USER_GID} .
	docker run -it -v "$(PWD):/app" smt-server npm install

dev:
	docker-compose build && docker-compose up --remove-orphans

DEPLOY_SERVER := ubuntu@smt-server.stellarium-web.org

deploy-prod:
	rm -rf data
	rm -rf node_modules
	bash -c "git log | head -n 1 | sed 's/commit //g' > extraVersionHash.txt"
	ssh -t $(DEPLOY_SERVER) "mkdir -p smt-server && cd smt-server && docker-compose down || true"
	ssh -t $(DEPLOY_SERVER) "rm -rf smt-server/*"
	scp -r * $(DEPLOY_SERVER):/home/ubuntu/smt-server/
	ssh -t $(DEPLOY_SERVER) "cd smt-server && docker-compose build && docker-compose up -d --remove-orphans"
