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

// The line below is the best I could find to get webworker running in dev mode
// as for some reasons, the inline version won't run on my localhost and the not
// inline version doesn't run on the real server because of CORS settings..

export default {
  fieldsList: undefined,
  smtServerInfo: undefined,

  fId2AlaSql: function (fieldId) {
    return fieldId.replace(/\./g, '_')
  },

  initDB: async function () {
    const that = this
    let resp = await fetch(process.env.VUE_APP_SMT_SERVER + '/api/v1/smtConfig', {
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json'
      }
    })
    const smtConfig = await resp.json()
    that.fieldsList = smtConfig.fields

    resp = await fetch(process.env.VUE_APP_SMT_SERVER + '/api/v1/smtServerInfo', {
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json'
      }
    })
    that.smtServerInfo = await resp.json()

    return smtConfig
  },

  query: function (q) {
    return fetch(process.env.VUE_APP_SMT_SERVER + '/api/v1/query', {
      method: 'POST',
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(q)
    }).then(function (response) {
      return response.json()
    })
  },

  queryVisual: function (q) {
    return fetch(process.env.VUE_APP_SMT_SERVER + '/api/v1/queryVisual', {
      method: 'POST',
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(q)
    }).then(function (response) {
      return response.text()
    })
  }
}
