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
      fieldsList: fieldsList
    })
  },

  loadAllData: function (jsonData) {
    return promiseWorker.postMessage({
      type: 'loadAllData',
      jsonData: jsonData
    })
  },

  query: function (q) {
    return promiseWorker.postMessage({
      type: 'query',
      q: q
    })
  }
}
