// Stellarium Web - Copyright (c) 2020 - Stellarium Labs SAS
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.
//
// This file is part of the Survey Monitoring Tool plugin, which received
// funding from the Centre national d'études spatiales (CNES).

import SmtLayerPage from './components/smt-layer-page.vue'
import storeModule from './store'
import Vue from 'vue'
import VueGoogleCharts from 'vue-google-charts'
import qe from './query-engine'
import filtrex from 'filtrex'
import sprintfjs from 'sprintf-js'

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
    app.$stel.core.lines.boundary.visible = true
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

    return qe.initDB().then(smtConfig => {
      const filtrexOptions = {
        extraFunctions: { sprintf: (fmt, x) => sprintfjs.sprintf(fmt, x) }
      }
      for (const field of smtConfig.fields) {
        if (field.formatFunc) {
          field.formatFuncCompiled = filtrex.compileExpression(field.formatFunc, filtrexOptions)
        }
      }

      Vue.prototype.$smt = smtConfig

      if (smtConfig.watermarkImage) {
        app.$store.commit('setValue', { varName: 'SMT.watermarkImage', newValue: smtConfig.watermarkImage })
      }
      if (smtConfig.dataLoadingImage) {
        app.$store.commit('setValue', { varName: 'SMT.dataLoadingImage', newValue: smtConfig.dataLoadingImage })
      }
      app.$store.commit('setValue', { varName: 'SMT.status', newValue: 'loading' })
      app.$store.commit('setValue', { varName: 'SMT.status', newValue: 'ready' })
    },
    err => {
      app.$store.commit('setValue', { varName: 'SMT.status', newValue: 'error' })
      throw err
    })
  }
}
