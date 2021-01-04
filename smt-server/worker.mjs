// Stellarium Web - Copyright (c) 2021 - Stellarium Labs SAS
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.
//
// This file is part of the Survey Monitoring Tool plugin, which received
// funding from the Centre national d'études spatiales (CNES).

import qe from './query-engine.mjs'
import worker_threads from 'worker_threads'

const { dbFileName, fieldsList } = worker_threads.workerData

console.log('Init worker from: ' + dbFileName)

qe.init(dbFileName, fieldsList)
worker_threads.parentPort.on('message', ({ func, parameters }) => {
  const result = qe[func](...parameters)
  worker_threads.parentPort.postMessage(result)
})
