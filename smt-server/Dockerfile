FROM node:14.15.0-stretch

EXPOSE 8102

RUN apt-get update && apt-get install -y libcurl3-gnutls ca-certificates

WORKDIR /app

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

# Configure default user environment, can be overriden when passing USER_UID
# and USER_GID when building this image
ARG USER_UID=1000
ARG USER_GID=1000
# the node base docker image creates a "node" user with UID 1000: change
# it to match the values passed in USER_UID/USER_GID
RUN groupmod -g $USER_GID node && usermod -u $USER_UID -g $USER_GID node

RUN chown node /app
USER node
RUN id node
CMD npm install --no-fund && npm start
