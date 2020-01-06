# Stellarium Web frontend

This directory contains the Graphical User Interface for using
Stellarium Web Engine in a web page.

This is a Vuejs project, which can generate a fully static webpage with webpack.

Official page: [stellarium-web.org](https://stellarium-web.org)

## Build setup using Docker
Make sure docker is installed, then:

``` bash
# generate the docker image
make setup

# and run the development version (go to http://localhost:8080 on your machine)
make dev

# Optionally, compile a production version of the site with minification
make build

# and finally to host it on a test server (http://localhost:8000)
make start
```

Note that before you build the web GUI the first time, the JS version of
the engine also needs to be built by running the following commands:

``` bash
# generate the engine buider docker image. Run only once.
cd .. && make setup
```

You can the update the engine at any time by running
``` bash
make update-engine
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
[Stellarium Web Contributor License Agreement (CLA)](doc/cla/sign-cla.md).
