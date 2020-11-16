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
import NodeGit from 'nodegit'

console.log('Starting SMT Server')

const app = express()
app.use(cors())
app.use(bodyParser.json())         // to support JSON-encoded bodies

// Allow to catch CTRL+C when runnning inside a docker
process.on('SIGINT', () => {
  console.info("User Interrupted")
  process.exit(0)
})

const port = 8100
const __dirname = process.cwd();

const cloneURL = "git@github.com:Stellarium-Labs/smt-data.git"
const localPath = __dirname + '/data'
const cloneOptions = {
  fetchOpts: {
    callbacks: {
      certificateCheck: function() { return 0 },
      credentials: function(url, userName) {
        if (fs.existsSync(__dirname + '/access_key.pub')) {
          return NodeGit.Cred.sshKeyNew(
            userName,
            __dirname + '/access_key.pub',
            __dirname + '/access_key', '')
        } else {
          return NodeGit.Cred.sshKeyFromAgent(userName)
        }
      }
    }
  }
}

var smtConfigData

const ingestAll = function () {
  smtConfigData = fs.readFileSync(__dirname + '/data/smtConfig.json')
  let smtConfig = JSON.parse(smtConfigData)

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
}

console.log('Synchronizing with SMT data git repo: ' + cloneURL)
NodeGit.Clone(cloneURL, localPath, cloneOptions)
  .catch(err => {
    return NodeGit.Repository.open(localPath)
  })
  .then(repo => {
    console.log('Pulling to last commit')
    repo.fetchAll(cloneOptions.fetchOpts)
    .then(function() {
      repo.mergeBranches("master", "origin/master")
      ingestAll()

      app.listen(port, () => {
        console.log(`SMT Server listening at http://localhost:${port}`)
      })
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

app.get('/hips/:queryHash/Allsky.geojson', async (req, res) => {
  const tileResp = await qe.getHipsTile(req.params.queryHash, -1, 0)
  if (!tileResp) {
    res.status(404).send()
    return
  }
  res.send(tileResp)
})

