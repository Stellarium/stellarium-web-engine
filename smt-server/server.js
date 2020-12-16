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
import hash_sum from 'hash-sum'

const SMT_SERVER_INFO = {
  version: process.env.npm_package_version || 'dev',
  dataGitServer: 'git@github.com:Stellarium-Labs/smt-data.git',
  dataGitBranch: 'data_v01',
  dataGitSha1: '',
  baseHashKey: ''
}

console.log('Starting SMT Server ' + SMT_SERVER_INFO.version)

const app = express()
app.use(cors())
app.use(bodyParser.json())         // to support JSON-encoded bodies

// Allow to catch CTRL+C when runnning inside a docker
process.on('SIGINT', () => {
  console.info("User Interrupted")
  process.exit(0)
})

const port = process.env.PORT || 8100
const __dirname = process.cwd()

var smtConfigData

const ingestAll = function () {
  smtConfigData = fs.readFileSync(__dirname + '/data/smtConfig.json')
  let smtConfig = JSON.parse(smtConfigData)

  const fetchAndIngest = function (fn) {
    return fsp.readFile(__dirname + '/data/' + fn).then(
      data => qe.loadAllData(JSON.parse(data)),
      err => { throw err})
  }

  return qe.initDB(smtConfig.fields, SMT_SERVER_INFO.baseHashKey).then(_ => {
    const allPromise = smtConfig.sources.map(url => fetchAndIngest(url))
    return Promise.all(allPromise).then(_ => {
      console.log('Loading finished')
    })
  })
}

const syncGitData = async function (gitServer, gitBranch) {
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
  console.log('Synchronizing with SMT data git repo: ' + gitServer)
  const repo = await NodeGit.Clone(gitServer, localPath, cloneOptions)
    .catch(err => {
      console.log('Repo already exists, user local version from ' + localPath)
      return NodeGit.Repository.open(localPath)
    })
  await repo.fetchAll(cloneOptions.fetchOpts)
  console.log('Getting to last commit on branch ' + gitBranch)
  const ref = await repo.getBranch('refs/remotes/origin/' + gitBranch)
  await repo.checkoutRef(ref)
  const commit = await repo.getHeadCommit()
  const statuses = await repo.getStatus()
  const ret = {}
  ret.dataGitSha1 = await commit.sha()
  ret.modified = false
  statuses.forEach(s => { if (s.isModified()) ret.modified = true })
  if (ret.modified) console.log('Data has local modifications')
  return ret
}

const getSmtServerSourceCodeHash = async function () {
  let extraVersionHash = ''
  try {
    extraVersionHash = await fsp.readFile(__dirname + '/extraVersionHash.txt')
  } catch (err) {
    console.log('No extraVersionHash.txt file found, try to generate one from git status')
    // Check if this server is in a git and if it has modifications, generate
    // an extraVersionHash on the fly
    try {
      const repo = await NodeGit.Repository.open(__dirname + '/..')
      const commit = await repo.getHeadCommit()
      const statuses = await repo.getStatus()
      const serverCodeGitSha1 = await commit.sha()
      let modified = false
      statuses.forEach(s => { if (s.isModified()) modified = true })
      extraVersionHash = serverCodeGitSha1
      if (modified) {
        console.log('Server code has local modifications')
        extraVersionHash += '_' + Date.now()
      }
    } catch (err) {
      // This server is not in a git, just give up and return empty string
    }
  }
  return extraVersionHash
}

const initServer = async function () {
  const extraVersionHash = await getSmtServerSourceCodeHash()
  const ret = await syncGitData(SMT_SERVER_INFO.dataGitServer, SMT_SERVER_INFO.dataGitBranch)
  SMT_SERVER_INFO.dataGitSha1 = ret.dataGitSha1
  SMT_SERVER_INFO.dataLocalModifications = ret.modified

  // Compute the base hash key which is unique for a given version of the server
  // code and data. It will be used to generate cache-friendly URLs.
  let baseHashKey = SMT_SERVER_INFO.dataGitSha1 + extraVersionHash
  if (SMT_SERVER_INFO.dataLocalModifications)
    baseHashKey += '_' + Date.now()
  SMT_SERVER_INFO.baseHashKey = hash_sum(baseHashKey)
  console.log('Server base hash key: ' + SMT_SERVER_INFO.baseHashKey )

  await ingestAll()
  app.listen(port, () => {
    console.log(`SMT Server listening at http://localhost:${port}`)
  })
}
initServer()

app.get('/api/v1/smtServerInfo', (req, res) => {
  res.send(SMT_SERVER_INFO)
})

app.get('/api/v1/smtConfig', (req, res) => {
  res.send(smtConfigData)
})

app.post('/api/v1/query', async (req, res) => {
  const queryResp = await qe.query(req.body)
  res.send(queryResp)
})

app.get('/api/v1/:serverHash/query', async (req, res) => {
  if (req.params.serverHash !== SMT_SERVER_INFO.baseHashKey) {
    res.status(404).send()
    return
  }
  const q = JSON.parse(decodeURIComponent(req.query.q))
  res.set('Cache-Control', 'public, max-age=31536000')
  const queryResp = await qe.query(q)
  res.send(queryResp)
})

app.post('/api/v1/queryVisual', (req, res) => {
  res.send(qe.queryVisual(req.body))
})

app.get('/api/v1/:serverHash/queryVisual', (req, res) => {
  if (req.params.serverHash !== SMT_SERVER_INFO.baseHashKey) {
    res.status(404).send()
    return
  }
  const q = JSON.parse(decodeURIComponent(req.query.q))
  res.set('Cache-Control', 'public, max-age=31536000')
  res.send(qe.queryVisual(q))
})

app.get('/api/v1/hips/:queryHash/properties', (req, res) => {
  res.set('Cache-Control', 'public, max-age=31536000')
  res.send(qe.getHipsProperties(req.params.queryHash))
})

app.get('/api/v1/hips/:queryHash/:order(Norder\\d+)/:dir/:pix.geojson', async (req, res) => {
  res.set('Cache-Control', 'public, max-age=31536000')

  const order = parseInt(req.params.order.replace('Norder', ''))
  const pix = parseInt(req.params.pix.replace('Npix', ''))
  const tileResp = await qe.getHipsTile(req.params.queryHash, order, pix)
  if (!tileResp) {
    res.status(404).send()
    return
  }
  res.send(tileResp)
})

app.get('/api/v1/hips/:queryHash/Allsky.geojson', async (req, res) => {
  res.set('Cache-Control', 'public, max-age=31536000')
  const tileResp = await qe.getHipsTile(req.params.queryHash, -1, 0)
  if (!tileResp) {
    res.status(404).send()
    return
  }
  res.send(tileResp)
})

