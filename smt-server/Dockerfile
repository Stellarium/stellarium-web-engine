FROM node:14.15.0-stretch

EXPOSE 8100

RUN apt-get update && apt-get install -y libcurl3-gnutls ca-certificates

WORKDIR /app

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

# RUN deluser --remove-home node

# Configure default user environment
#ARG USER_UID=1000
#ARG USER_GID=1000
#RUN addgroup --gid $USER_GID app && useradd -g app -u $USER_UID app

RUN chown node /app
USER node
RUN id node
CMD node app.js
