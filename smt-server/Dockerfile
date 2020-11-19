FROM node:14.15.0-stretch

EXPOSE 8100

RUN apt-get update && apt-get install -y libcurl3-gnutls ca-certificates

WORKDIR /app

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

RUN chown node /app
USER node
RUN id node
CMD npm start
