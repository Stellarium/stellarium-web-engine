// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

import PromiseWorker from 'promise-worker'
// eslint-disable-next-line
import Worker from 'worker-loader!./worker'

const promiseWorker = new PromiseWorker(new Worker())

export default {
  fieldsList: undefined,

  fId2AlaSql: function (fieldId) {
    return fieldId.replace(/\./g, '_')
  },

  initDB: function (fieldsList) {
    this.fieldsList = fieldsList
    return promiseWorker.postMessage({
      type: 'initDB',
      fieldsList: JSON.parse(JSON.stringify(fieldsList))
    })
  },

  loadGeojson: function (url) {
    return promiseWorker.postMessage({
      type: 'loadGeojson',
      url: url
    })
  },

  query: function (q) {
    return promiseWorker.postMessage({
      type: 'query',
      q: JSON.parse(JSON.stringify(q))
    })
  }
}
