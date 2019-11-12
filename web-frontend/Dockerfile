FROM node:12.10.0-alpine
EXPOSE 8080
EXPOSE 8888

WORKDIR /app

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

RUN deluser --remove-home node

# Configure default user environment
ARG USER_UID=1000
ARG USER_GID=1000
RUN addgroup -S app -g $USER_GID && adduser -S -G app -u $USER_UID app

RUN chown app /app
USER app
RUN id app

RUN yarn

CMD yarn run dev
