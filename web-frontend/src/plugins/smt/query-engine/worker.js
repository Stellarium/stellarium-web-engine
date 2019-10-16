// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

import qe from './query-engine-alasql'
import registerPromiseWorker from 'promise-worker/register'

registerPromiseWorker((message) => {
  if (message.type === 'initDB') {
    return qe.initDB(message.fieldsList)
  } else if (message.type === 'loadGeojson') {
    return qe.loadGeojson(message.url)
  } else if (message.type === 'query') {
    return qe.query(message.q)
  }
})
