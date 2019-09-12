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

import fullscreen from 'vue-fullscreen'
import VueJsonp from 'vue-jsonp'

import VueCookie from 'vue-cookie'

import App from './App'

import { L } from 'vue2-leaflet'
import 'leaflet/dist/leaflet.css'
import 'leaflet-control-geocoder/dist/Control.Geocoder.css'

// this part resolve an issue where the markers would not appear
delete L.Icon.Default.prototype._getIconUrl

L.Icon.Default.mergeOptions({
  iconRetinaUrl: require('leaflet/dist/images/marker-icon-2x.png'),
  iconUrl: require('leaflet/dist/images/marker-icon.png'),
  shadowUrl: require('leaflet/dist/images/marker-shadow.png')
})

Vue.use(VueCookie)

Vue.use(Vuetify)

Vue.use(fullscreen)

Vue.use(VueJsonp)
// Vue.config.productionTip = false

// Load all plugins JS modules found the in the plugins directory
var plugins = []
let ctx = require.context('./plugins/', true, /\.\/\w+\/index\.js$/)
for (let i in ctx.keys()) {
  let key = ctx.keys()[i]
  console.log('Loading plugin: ' + key)
  let mod = ctx(key)
  plugins.push(mod.default)
}
Vue.SWPlugins = plugins

// Setup routes for the app
Vue.use(Router)
// Base routes
let routes = [
  {
    // The main page
    path: '/',
    name: 'App',
    component: App,
    children: []
  },
  {
    // Main page, but centered on the passed sky source name
    path: '/skysource/:name',
    component: App,
    alias: '/'
  }
]
// Routes exposed by plugins
let defaultObservingRoute = {
  path: '/p/calendar',
  meta: {prio: 2}
}
for (let i in Vue.SWPlugins) {
  let plugin = Vue.SWPlugins[i]
  if (plugin.routes) {
    routes = routes.concat(plugin.routes)
  }
  if (plugin.panelRoutes) {
    routes[0].children = routes[0].children.concat(plugin.panelRoutes)
    for (let j in plugin.panelRoutes) {
      let r = plugin.panelRoutes[j]
      if (r.meta && r.meta.prio && r.meta.prio < defaultObservingRoute.meta.prio) {
        defaultObservingRoute = r
      }
    }
  }
  if (plugin.vuePlugin) {
    Vue.use(plugin.vuePlugin)
  }
}
routes[0].children.push({ path: '/p', redirect: defaultObservingRoute.path })
var router = new Router({
  mode: 'history',
  routes: routes
})

// Expose plugins singleton to all Vue instances
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
