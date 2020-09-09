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

import PromiseWorker from 'promise-worker'

// The line below is the best I could find to get webworker running in dev mode
// as for some reasons, the inline version won't run on my localhost and the not
// inline version doesn't run on the real server because of CORS settings..

// eslint-disable-next-line
const Worker = process.env.NODE_ENV === 'development' ? require('worker-loader!./worker') : require('worker-loader?{"inline":true,"fallback":false}!./worker')

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
