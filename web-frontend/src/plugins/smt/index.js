// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

import SmtLayerPage from './components/smt-layer-page.vue'
import storeModule from './store'
import alasql from 'alasql'

async function loadAllData (jsonData) {
  let features = jsonData.features
  console.log('Loading ' + features.length + ' features')

  console.log('Create db')
  await alasql.promise('CREATE TABLE tiles (id INT PRIMARY KEY, geo JSON, scenario JSON)')
  await alasql.promise('CREATE INDEX idx_time ON tiles(scenario->contractual_date)')

  let req = alasql.compile('INSERT INTO tiles VALUES (?, ?, ?)')
  for (let feature of features) {
    let id = feature.id
    let geo = feature.geometry
    let scenario = feature.properties.scenario
    await req.promise([id, geo, scenario])
  }

  console.log('Test query by scenario.contractual_date')
  let res = await alasql.promise('SELECT * FROM tiles WHERE DATE(scenario->contractual_date) > DATE("2022-04-05")')
  console.log(res)
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
    // Init base view settings
    app.$stel.core.lines.equatorial.visible = true
    app.$stel.core.atmosphere.visible = false
    app.$stel.core.landscapes.visible = false
    app.$stel.core.landscapes.fog_visible = false
    app.$stel.core.cardinals.visible = false
    app.$stel.core.planets.visible = false
    app.$stel.core.mount_frame = app.$stel.FRAME_ICRF
    app.$stel.core.projection = 4 // Hammer
    app.$stel.core.observer.refraction = false
    app.$stel.core.observer.yaw = 0
    app.$stel.core.observer.pitch = 0
    app.$stel.core.fov = 270

    let jsonData = require('./euclid-test.json')
    loadAllData(jsonData)
  }
}
