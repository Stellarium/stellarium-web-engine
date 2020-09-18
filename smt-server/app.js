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

import express from 'express'
import cors from 'cors'
import fsp from 'fs/promises'
import fs from 'fs'
import qe from './query-engine.mjs'
import bodyParser from 'body-parser'

const app = express()
app.use(cors())

const port = 3000

const __dirname = process.cwd();
const smtConfigData = fs.readFileSync(__dirname + '/data/smtConfig.json')
let smtConfig = JSON.parse(smtConfigData)

app.use(bodyParser.json())         // to support JSON-encoded bodies

const fetchAndIngest = function (fn) {
  return fsp.readFile(__dirname + '/data/' + fn).then(
    data => qe.loadAllData(JSON.parse(data)),
    err => { throw err})
}

qe.initDB(smtConfig.fields).then(_ => {
  const allPromise = smtConfig.sources.map(url => fetchAndIngest(url))
  Promise.all(allPromise).then(_ => {
    console.log('Loading finished')
  })
})

app.get('/smtConfig', (req, res) => {
  res.send(smtConfigData)
})

app.post('/query', (req, res) => {
  qe.query(req.body).then(res.send.bind(res))
})

app.post('/queryVisual', (req, res) => {
  res.send(qe.queryVisual(req.body))
})

app.get('/queryVisual', (req, res) => {
  res.send(qe.queryVisual(req.body))
})

app.get('/hips/:queryHash/properties', (req, res) => {
  res.send(qe.getHipsProperties(req.params.queryHash))
})

app.get('/hips/:queryHash/:order(Norder\\d+)/:dir/:pix.geojson', async (req, res) => {
  const order = parseInt(req.params.order.replace('Norder', ''))
  const pix = parseInt(req.params.pix.replace('Npix', ''))
  const tileResp = await qe.getHipsTile(req.params.queryHash, order, pix)
  if (!tileResp) {
    res.status(404).send()
    return
  }
  res.send(tileResp)
})

app.listen(port, () => {
  console.log(`Example app listening at http://localhost:${port}`)
})
