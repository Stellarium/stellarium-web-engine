// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

// The Vue build version to load with the `import` command
// (runtime-only or standalone) has been set in webpack.base.conf with an alias.
import Vue from 'vue'

import Vuetify from 'vuetify'
import 'vuetify/dist/vuetify.css'

import Router from 'vue-router'

import store from './store'

import * as VueGoogleMaps from 'vue2-google-maps'

import VueObserveVisibility from 'vue-observe-visibility'
import fullscreen from 'vue-fullscreen'
import VueJsonp from 'vue-jsonp'

import VueCookie from 'vue-cookie'

import App from './App'

Vue.use(VueCookie)

// Used to work-around a gmaps component refresh bug
Vue.use(VueObserveVisibility)

Vue.use(VueGoogleMaps, {
  load: {
    key: 'AIzaSyBOfY-p-V3zecsV_K3pPuYyTPm5Vy-FURo',
    libraries: 'places' // Required to use the Autocomplete plugin
  }
})
Vue.use(Vuetify)

Vue.use(fullscreen)

Vue.use(VueJsonp)
// Vue.config.productionTip = false

var context = require.context('./plugins', true, /^\.\/.*\/index\.js$/)
var plugins = []
context.keys().forEach(function (key) {
  if (key.split('/').length === 3) {
    let plugin = context(key).default
    if (plugin.enabled !== false) {
      plugins.push(plugin)
    }
  }
})

// Don't use plugins for the moment
// Vue.SWPlugins = []
Vue.SWPlugins = plugins

Vue.use(Router)

// Add routes from plugins
let routes = [
  {
    path: '/',
    name: 'App',
    component: App
  }
]

for (let i in Vue.SWPlugins) {
  let plugin = Vue.SWPlugins[i]
  console.log('Loading plugin: ' + plugin.name)
  if (plugin.routes) {
    routes = routes.concat(plugin.routes)
  }
  if (plugin.vuePlugin) {
    Vue.use(plugin.vuePlugin)
  }
}

var router = new Router({
  mode: 'history',
  routes: routes
})

Vue.prototype.$stellariumWebPlugins = function () {
  return Vue.SWPlugins
}

/* eslint-disable no-new */
new Vue({
  el: '#app',
  router,
  store,
  template: '<router-view></router-view>'
})
