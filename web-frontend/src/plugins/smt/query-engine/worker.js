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
