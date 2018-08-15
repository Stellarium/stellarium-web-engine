// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

import Swagger from 'swagger-client'
import _ from 'lodash'
import store from 'store'

var swaggerClient

const NoctuaSkyClient = {
  serverUrl: undefined,
  state: {
    locations: [],
    observations: [],
    user: {
      first_name: undefined,
      last_name: undefined,
      email: undefined
    },
    status: 'initializing'
  },
  stateChanged: undefined,
  syncDataTable: function (tableName) {
    let that = this
    return NoctuaSkyClient[tableName].query().then(res => {
      that.state[tableName] = res
      that.stateChanged(tableName, that.state[tableName])
      return res
    }, err => {
      return err
    })
  },
  syncUserData: function () {
    let that = this
    return that.syncDataTable('locations').then(() => { return that.syncDataTable('observations') })
  },
  init: function (serverUrl, onStateChanged) {
    let that = this
    that.serverUrl = serverUrl
    that.stateChanged = onStateChanged

    // Setup with initial default state
    that.stateChanged('', that.state)
    return Swagger(serverUrl + '/doc/openapi.json', {
      authorizations: {
        APIToken: ''
      },
      requestInterceptor: req => {
        if (req.body && !req.headers['Content-Type']) {
          req.headers['Content-Type'] = 'application/json'
        }
      }
    }).then(client => {
      swaggerClient = client
      let removeTrailingNumbers = function (val, key) { return key.replace(/\d+$/, '') }
      let tmpApis = {}
      tmpApis.observations = _.mapKeys(client.apis.observations, removeTrailingNumbers)
      tmpApis.locations = _.mapKeys(client.apis.locations, removeTrailingNumbers)
      tmpApis.users = _.mapKeys(client.apis.users, removeTrailingNumbers)
      tmpApis.skysources = _.mapKeys(client.apis.skysources, removeTrailingNumbers)

      that.skysources = {
        query: function (str, limit, exact = null) {
          if (limit === undefined) {
            limit = 10
          }
          str = str.toUpperCase()
          str = str.replace(/\s+/g, '')
          return tmpApis.skysources.query({q: str, limit: limit, exact: exact}).then(res => {
            return res.body
          }, err => {
            throw err.response.body
          })
        },

        // Get data for a SkySource from its NSID in NoctuaSky api service
        get: function (nsid) {
          return tmpApis.skysources.get({nsid: nsid}).then(res => {
            return res.body
          }, err => {
            throw err.response.body
          })
        }
      }

      that.users = {
        login: function (email, password) {
          that.state.status = 'signInInProgress'
          that.stateChanged('status', that.state.status)
          return tmpApis.users.login({body: {email: email, password: password}}).then((res) => {
            that.state.user = res.body
            swaggerClient.authorizations.APIToken = 'Bearer ' + res.body.access_token
            store.set('noctuasky_token', swaggerClient.authorizations.APIToken)
            delete that.state.user.access_token
            console.log('NoctuaSky Login successful')
            that.state.status = 'loggedIn'
            that.stateChanged('', that.state)
            that.syncUserData()
            return res.body
          }, err => {
            that.users.logout()
            throw err.response.body
          })
        },
        logout: function () {
          store.remove('noctuasky_token')
          swaggerClient.authorizations.APIToken = ''
          that.state.user = {
            first_name: undefined,
            last_name: undefined,
            email: undefined
          }
          that.state.status = 'loggedOut'
          that.state.observations = []
          that.state.locations = []
          that.stateChanged('', that.state)
        },
        register: function (email, password, firstName, lastName) {
          that.state.status = 'signUpInProgress'
          that.stateChanged('status', that.state.status)
          return tmpApis.users.add({body: {email: email, password: password, first_name: firstName, last_name: lastName}}).then((res) => {
            console.log('NoctuaSky Register successful')
            that.state.status = 'loggedOut'
            that.stateChanged('status', that.state.status)
            return res.body
          }, err => {
            that.state.status = 'loggedOut'
            that.stateChanged('status', that.state.status)
            throw err.response.body
          })
        },
        deleteAccount: function () {
          return tmpApis.users.delete({id: that.state.user.id}).then(res => { that.users.logout() })
        },
        changePassword: function (currentPassword, newPassword) {
          return tmpApis.users.changePassword({id: that.state.user.id, body: {password: currentPassword, new_password: newPassword}}).then((res) => {
            console.log('Password change successful')
            return res.body
          }, err => {
            throw err.response.body
          })
        },
        get: function () {
          return tmpApis.users.get({id: that.state.user.id}).then((res) => {
            return res.body
          }, err => {
            throw err.response.body
          })
        },
        updateUserInfo: function (newInfo) {
          return tmpApis.users.update({id: that.state.user.id, body: newInfo}).then((res) => {
            console.log('User info update successful')
            return that.users.get(that.state.user.id).then((res) => {
              that.state.user = res
              that.stateChanged('user', that.state.user)
              return res
            })
          }, err => {
            throw err.response.body
          })
        }
      }

      that.locations = {
        query: function (pageSize = 100) {
          return tmpApis.locations.query({page_size: pageSize}).then(res => {
            return res.body
          }, err => {
            return err.response.body
          })
        },
        add: function (loc) {
          return tmpApis.locations.add({body: loc}).then(res => {
            that.state.locations.push(res.body)
            that.stateChanged('locations', that.state.locations)
            return res.body
          }, err => {
            return err.response.body
          })
        },
        get: function (id) {
          return tmpApis.locations.get({id: id}).then(res => {
            return res.body
          }, err => {
            return err.response.body
          })
        },
        update: function (id, loc) {
          return tmpApis.locations.update({id: id, body: loc}).then(res => {
            return that.syncDataTable('locations').then(res => { return that.locations.get(id) })
          }, err => {
            return err.response.body
          })
        },
        delete: function (id) {
          return tmpApis.locations.delete({id: id}).then(res => {
            that.state.locations = that.state.locations.filter(e => { return e.id !== id })
            that.stateChanged('locations', that.state.locations)
            return res
          }, err => {
            return err.response.body
          })
        }
      }

      that.observations = {
        query: function (pageSize = 100) {
          return tmpApis.observations.query({page_size: pageSize}).then(res => {
            return res.body
          }, err => {
            return err.response.body
          })
        },
        add: function (loc) {
          return tmpApis.observations.add({body: loc}).then(res => {
            that.state.observations.push(res.body)
            that.stateChanged('observations', that.state.observations)
            return res.body
          }, err => {
            return err.response.body
          })
        },
        get: function (id) {
          return tmpApis.observations.get({id: id}).then(res => {
            return res.body
          }, err => {
            return err.response.body
          })
        },
        update: function (id, loc) {
          return tmpApis.observations.update({id: id, body: loc}).then(res => {
            return that.syncDataTable('observations').then(res => { return that.observations.get(id) })
          }, err => {
            return err.response.body
          })
        },
        delete: function (id) {
          return tmpApis.observations.delete({id: id}).then(res => {
            that.state.observations = that.state.observations.filter(e => { return e.id !== id })
            that.stateChanged('observations', that.state.observations)
            return res
          }, err => {
            return err.response.body
          })
        },
        lastModified: function () {
          console.log('TODO: store real last modification time')
          if (!that.state.observations.length) return undefined
          return that.state.observations[that.state.observations.length - 1]
        }
      }

      console.log('Initialized NoctuaSky Client')
      let token = store.get('noctuasky_token')
      if (token) {
        console.log('Found previous token, try to re-use..')
        swaggerClient.authorizations.APIToken = token
        tmpApis.users.get({id: 'me'}).then((res) => {
          console.log('NoctuaSky Login successful')
          that.state.user = res.body
          that.state.status = 'loggedIn'
          that.stateChanged('', that.state)
          that.syncUserData()
        }, (error) => {
          console.log("Couldn't re-use saved token:" + error)
          that.users.logout()
        })
      }
    }, error => {
      console.log('Could not initialize NoctuaSky Client at ' + serverUrl + ' ' + error)
    })
  }
}

export default NoctuaSkyClient
