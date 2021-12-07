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
import App from './App.vue'
import vuetify from './plugins/vuetify'
import 'roboto-fontface/css/roboto/roboto-fontface.css'
import '@mdi/font/css/materialdesignicons.css'
import store from './store'
import Router from 'vue-router'
import fullscreen from 'vue-fullscreen'
import VueJsonp from 'vue-jsonp'
import VueCookie from 'vue-cookie'
import _ from 'lodash'

import { Icon } from 'leaflet'
import 'leaflet/dist/leaflet.css'
import 'leaflet-control-geocoder/dist/Control.Geocoder.css'
import VueI18n from 'vue-i18n'
import Moment from 'moment'

Vue.config.productionTip = false

// this part resolve an issue where the markers would not appear
delete Icon.Default.prototype._getIconUrl

Icon.Default.mergeOptions({
  iconRetinaUrl: require('leaflet/dist/images/marker-icon-2x.png'),
  iconUrl: require('leaflet/dist/images/marker-icon.png'),
  shadowUrl: require('leaflet/dist/images/marker-shadow.png')
})

Vue.use(VueCookie)
Vue.use(fullscreen)
Vue.use(VueJsonp)
Vue.use(VueI18n)

// Load all plugins JS modules found in the plugins directory
var plugins = []
const ctx = require.context('./plugins/', true, /\.\/\w+\/index\.js$/)
for (const i in ctx.keys()) {
  const key = ctx.keys()[i]
  console.log('Loading plugin: ' + key)
  const mod = ctx(key)
  plugins.push(mod.default)
}
Vue.SWPlugins = plugins

// Loads all GUI translations found in the src/locales/ directory
var messages = {}
const guiLocales = require.context('./locales', true, /[A-Za-z0-9-_,\s]+\.json$/i)
guiLocales.keys().forEach(key => {
  const matched = key.match(/([A-Za-z0-9-_]+)\./i)
  if (matched && matched.length > 1) {
    const locale = matched[1]
    messages[locale] = guiLocales(key)
  }
})

// Loads all GUI translations found in the src/plugins/xxx/locales directories
const pluginsLocales = require.context('./plugins/', true, /\.\/\w+\/locales\/([A-Za-z0-9-_]+)\.json$/i)
pluginsLocales.keys().forEach(key => {
  const matched = key.match(/\.\/\w+\/locales\/([A-Za-z0-9-_]+)\.json/i)
  if (matched && matched.length > 1) {
    const locale = matched[1]
    if (messages[locale] === undefined) {
      messages[locale] = pluginsLocales(key)
    } else {
      _.merge(messages[locale], pluginsLocales(key))
    }
  }
})

const loc = 'en'
// Un-comment to select user's language automatically
// loc = (navigator.language || navigator.userLanguage).split('-')[0] || 'en'
Moment.locale(loc)
var i18n = new VueI18n({
  locale: loc,
  messages: messages,
  formatFallbackMessages: true,
  fallbackLocale: 'en',
  silentTranslationWarn: true
})

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
  meta: { prio: 2 }
}
for (const i in Vue.SWPlugins) {
  const plugin = Vue.SWPlugins[i]
  if (plugin.routes) {
    routes = routes.concat(plugin.routes)
  }
  if (plugin.panelRoutes) {
    routes[0].children = routes[0].children.concat(plugin.panelRoutes)
    for (const j in plugin.panelRoutes) {
      const r = plugin.panelRoutes[j]
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
  base: '/',
  routes: routes
})

// Expose plugins singleton to all Vue instances
Vue.prototype.$stellariumWebPlugins = function () {
  return Vue.SWPlugins
}

/* eslint-disable no-new */
new Vue({
  router,
  store,
  i18n,
  vuetify
}).$mount('#app')
