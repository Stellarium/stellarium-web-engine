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
  currentUser: undefined,
  serverUrl: undefined,
  init: function (serverUrl) {
    let that = this
    that.serverUrl = serverUrl
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
      console.log('Initialized NoctuaSky Client')
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
          return tmpApis.users.login({body: {email: email, password: password}}).then((res) => {
            that.currentUser = res.body
            swaggerClient.authorizations.APIToken = 'Bearer ' + res.body.access_token
            store.set('noctuasky_token', swaggerClient.authorizations.APIToken)
            delete that.currentUser.access_token
            console.log('NoctuaSky Login successful')
            return res.body
          }, err => {
            throw err.response.body
          })
        },
        logout: function () {
          store.remove('noctuasky_token')
          swaggerClient.authorizations.APIToken = ''
          that.currentUser = undefined
        },
        register: function (email, password, firstName, lastName) {
          return tmpApis.users.add({body: {email: email, password: password, first_name: firstName, last_name: lastName}}).then((res) => {
            console.log('NoctuaSky Register successful')
            return res.body
          }, err => {
            throw err.response.body
          })
        },
        deleteAccount: function () {
          return tmpApis.users.delete({user_id: that.currentUser.id})
        },
        changePassword: function (currentPassword, newPassword) {
          return tmpApis.users.changePassword({user_id: that.currentUser.id, body: {password: currentPassword, new_password: newPassword}}).then((res) => {
            console.log('Password change successful')
            return res.body
          }, err => {
            throw err.response.body
          })
        },
        get: function () {
          return tmpApis.users.get({user_id: that.currentUser.id}).then((res) => {
            return res.body
          }, err => {
            throw err.response.body
          })
        },
        updateUserInfo: function (newInfo) {
          return tmpApis.users.update({user_id: that.currentUser.id, body: newInfo}).then((res) => {
            console.log('User info update successful')
            return that.users.get(that.currentUser.id).then((res) => {
              that.currentUser = res
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
            return res.body
          }, err => {
            return err.response.body
          })
        },
        get: function (id) {
          return tmpApis.locations.get({loc_id: id}).then(res => {
            return res.body
          }, err => {
            return err.response.body
          })
        },
        update: function (id, loc) {
          return tmpApis.locations.update({loc_id: id, body: loc}).then(res => {
            return that.locations.get(id)
          }, err => {
            return err.response.body
          })
        },
        delete: function (id) {
          return tmpApis.locations.delete({loc_id: id}).then(res => {
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
            return res.body
          }, err => {
            return err.response.body
          })
        },
        get: function (id) {
          return tmpApis.observations.get({obs_id: id}).then(res => {
            return res.body
          }, err => {
            return err.response.body
          })
        },
        update: function (id, loc) {
          return tmpApis.observations.update({obs_id: id, body: loc}).then(res => {
            return that.observations.get(id)
          }, err => {
            return err.response.body
          })
        },
        delete: function (id) {
          return tmpApis.observations.delete({obs_id: id}).then(res => {
            return res
          }, err => {
            return err.response.body
          })
        }
      }

      let token = store.get('noctuasky_token')
      if (token) {
        console.log('Found previous token, try to re-use..')
        swaggerClient.authorizations.APIToken = token
        tmpApis.users.get({user_id: 'me'}).then((res) => {
          console.log('NoctuaSky Login successful')
          that.currentUser = res.body
          return that.currentUser
        }, (error) => {
          console.log("Couldn't re-use saved token:" + error)
          that.logout()
        })
      }
    }, error => {
      console.log('Could not initialize NoctuaSky Client at ' + serverUrl + ' ' + error)
    })
  }
}

export default NoctuaSkyClient
