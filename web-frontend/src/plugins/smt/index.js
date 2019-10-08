// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

import SmtLayerPage from './components/smt-layer-page.vue'
import storeModule from './store'
import Vue from 'vue'
import VueGoogleCharts from 'vue-google-charts'
import qe from './queryEngine'

Vue.use(VueGoogleCharts)

export default {
  vuePlugin: {
    install: (Vue, options) => {
      Vue.component('smt-layer-page', SmtLayerPage)
    }
  },
  name: 'SMT',
  storeModule: storeModule,
  panelRoutes: [
    { path: '/p/smt', component: SmtLayerPage, meta: { tabName: 'Survey Tool', prio: 1 } }
  ],
  onEngineReady: function (app) {
    app.$store.commit('setValue', { varName: 'SMT.status', newValue: 'initializing' })

    // Init base view settings
    app.$stel.core.lines.equatorial.visible = true
    app.$stel.core.atmosphere.visible = false
    app.$stel.core.landscapes.visible = false
    app.$stel.core.landscapes.fog_visible = false
    app.$stel.core.cardinals.visible = false
    app.$stel.core.planets.visible = false
    app.$stel.core.dsos.visible = false
    app.$stel.core.comets.visible = false
    app.$stel.core.satellites.visible = false
    app.$stel.core.minor_planets.visible = false
    app.$stel.core.mount_frame = app.$stel.FRAME_ICRF
    app.$stel.core.projection = 5 // PROJ_MOLLWEIDE
    app.$stel.core.observer.refraction = false
    app.$stel.core.observer.yaw = 0
    app.$stel.core.observer.pitch = 0
    app.$stel.core.fov = 270
    app.$store.commit('setValue', { varName: 'timeSpeed', newValue: 0 })
    app.$store.commit('setValue', { varName: 'showLocationButton', newValue: false })
    app.$store.commit('setValue', { varName: 'showTimeButtons', newValue: false })
    app.$store.commit('setValue', { varName: 'showFPS', newValue: true })

    let smtConfig = require('./smtConfig.json')
    Vue.prototype.$smt = smtConfig

    app.$store.commit('setValue', { varName: 'SMT.status', newValue: 'loading' })

    let fetchAndIngest = function (url) {
      return fetch(url).then(function (response) {
        if (!response.ok) {
          throw response.body
        }
        return response.json().then(jsonData => {
          return qe.loadAllData(jsonData).then(_ => {
            return jsonData.length
          })
        }, err => { throw err })
      }, err => {
        throw err.response.body
      })
    }

    qe.initDB(smtConfig.fields).then(_ => {
      let allPromise = smtConfig.sources.map(url => fetchAndIngest(url))
      Promise.all(allPromise).then(_ => {
        app.$store.commit('setValue', { varName: 'SMT.status', newValue: 'ready' })
      })
    })
  }
}
