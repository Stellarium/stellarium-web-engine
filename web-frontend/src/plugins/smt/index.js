// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

import SmtLayerPage from './components/smt-layer-page.vue'
import storeModule from './store'
import alasql from 'alasql'
import Vue from 'vue'
import VueGoogleCharts from 'vue-google-charts'

Vue.use(VueGoogleCharts)

async function initDB (fieldsList) {
  console.log('Create Data Base')
  await alasql.promise('CREATE TABLE features (geometry JSON, properties JSON)')

  // Create an index on each field
  for (let i in fieldsList) {
    let field = fieldsList[i]
    await alasql.promise('CREATE INDEX idx_' + i + ' ON features(' + field.id.replace(/\./g, '->') + ')')
  }
}

async function loadAllData (fieldsList, jsonData) {
  console.log('Loading ' + jsonData.features.length + ' features')

  // Insert all data
  let req = alasql.compile('INSERT INTO features VALUES (?, ?)')
  for (let feature of jsonData.features) {
    let geometry = feature.geometry
    let properties = feature.properties
    await req.promise([geometry, properties])
  }
}

export default {
  vuePlugin: {
    install: (Vue, options) => {
      Vue.component('smt-layer-page', SmtLayerPage)
    }
  },
  name: 'SMT',
  storeModule: storeModule,
  panelRoutes: [
    { path: '/p/smt', component: SmtLayerPage, meta: { tabName: 'Survey Tool', prio: 2 } }
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
    app.$stel.core.mount_frame = app.$stel.FRAME_ICRF
    app.$stel.core.projection = 5 // PROJ_MOLLWEIDE
    app.$stel.core.observer.refraction = false
    app.$stel.core.observer.yaw = 0
    app.$stel.core.observer.pitch = 0
    app.$stel.core.fov = 270
    app.$store.commit('setValue', { varName: 'timeSpeed', newValue: 0 })
    app.$store.commit('setValue', { varName: 'showLocationButton', newValue: false })
    app.$store.commit('setValue', { varName: 'showTimeButtons', newValue: false })

    let fieldsList = require('./fieldsList.json')
    Vue.prototype.$smt = { fieldsList: fieldsList }

    app.$store.commit('setValue', { varName: 'SMT.status', newValue: 'loading' })

    let fetchAndIngest = function (url) {
      return fetch(url).then(function (response) {
        if (!response.ok) {
          throw response.body
        }
        return response.json().then(jsonData => {
          return loadAllData(fieldsList, jsonData).then(_ => {
            return jsonData.length
          })
        }, err => { throw err })
      }, err => {
        throw err.response.body
      })
    }

    let urls = [
      '/plugins/smt/Surveys/CFIS_cap.geojson',
      '/plugins/smt/Surveys/CFIS_crescent.geojson',
      '/plugins/smt/Surveys/CFIS_JEDIS_PAN_north.geojson',
      '/plugins/smt/Surveys/CFIS_JEDIS_PAN_south.geojson',
      '/plugins/smt/Surveys/DES.geojson',
      '/plugins/smt/Surveys/euclid-test1000.geojson',
      '/plugins/smt/Surveys/lsst_main.geojson',
      '/plugins/smt/Surveys/lsst_north_cap.geojson',
      '/plugins/smt/Surveys/lsst_north_crescent.geojson'
    ]
    let allPromise = urls.map(url => fetchAndIngest(url))

    initDB(fieldsList).then(_ => {
      Promise.all(allPromise).then(_ => {
        app.$store.commit('setValue', { varName: 'SMT.status', newValue: 'ready' })
      })
    })
  }
}
