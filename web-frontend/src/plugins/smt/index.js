// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

import SmtLayerPage from './components/smt-layer-page.vue'
import storeModule from './store'
import alasql from 'alasql'
import Vue from 'vue'
import VueGoogleCharts from 'vue-google-charts'
import _ from 'lodash'

Vue.use(VueGoogleCharts)

function fId2AlaSql (fieldId) {
  return fieldId.replace(/properties\./g, '').replace(/\./g, '_')
}

function fType2AlaSql (fieldType) {
  if (fieldType === 'string') return 'STRING'
  if (fieldType === 'date') return 'INT' // Dates are converted to unix time stamp
  return 'JSON'
}

async function initDB (fieldsList) {
  console.log('Create Data Base')
  let sqlFields = fieldsList.map(f => fId2AlaSql(f.id) + ' ' + fType2AlaSql(f.type)).join(', ')
  await alasql.promise('CREATE TABLE features (geometry JSON, properties JSON, ' + sqlFields + ')')

  // Create an index on each field
  for (let i in fieldsList) {
    let field = fieldsList[i]
    await alasql.promise('CREATE INDEX idx_' + i + ' ON features(' + fId2AlaSql(field.id) + ')')
  }
}

async function loadAllData (fieldsList, jsonData) {
  console.log('Loading ' + jsonData.features.length + ' features')

  // Insert all data
  let req = alasql.compile('INSERT INTO features VALUES (?, ?, ' + fieldsList.map(f => '?').join(', ') + ')')
  for (let feature of jsonData.features) {
    let arr = [feature.geometry, feature.properties]
    for (let i in fieldsList) {
      let d = _.get(feature, fieldsList[i].id, undefined)
      if (d !== undefined && fieldsList[i].type === 'date') {
        d = new Date(d).getTime()
      }
      arr.push(d)
    }
    await req.promise(arr)
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
          return loadAllData(smtConfig.fields, jsonData).then(_ => {
            return jsonData.length
          })
        }, err => { throw err })
      }, err => {
        throw err.response.body
      })
    }

    let allPromise = smtConfig.sources.map(url => fetchAndIngest(url))

    initDB(smtConfig.fields).then(_ => {
      Promise.all(allPromise).then(_ => {
        app.$store.commit('setValue', { varName: 'SMT.status', newValue: 'ready' })
      })
    })
  }
}
