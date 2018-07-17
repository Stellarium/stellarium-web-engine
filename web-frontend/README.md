# stellarium-web client

A light version of Stellarium running in the browser. It is mostly Graphical
User Interface for Stellarium Web Engine, a light C sky display engine, compiled into
Web Assembly.

[stellarium-web.org](https://stellarium-web.org)

This is a Vuejs project, which can generate a fully static webpage with webpack.

## Build setup using Docker
Make sure docker is installed, then:

``` bash
# generate the docker image
make setup

# and run the development version
make dev

# compile a production version of the site with minification
make build

# and finally to host it on a test server (http://localhost:8000)
make start
```

## Build Setup without Docker

``` bash
# install dependencies
yarn

# serve with hot reload at localhost:8080
yarn run dev

# build for production with minification
yarn run build
```

For a detailed explanation on how things work, check out the [guide](http://vuejs-templates.github.io/webpack/) and [docs for vue-loader](http://vuejs.github.io/vue-loader).

## Contributing

In order for your contribution to Stellarium Web to be accepted, you have to sign the
[Stellarium Web Contributor License Agreement (CLA)](doc/cla/sign-cla.md]).
