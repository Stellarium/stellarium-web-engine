// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

import NoctuaSkyClient from '@/assets/noctuasky-client'

var loadParseTable = function (commit, tableName) {
  return NoctuaSkyClient[tableName].query({page_size: 100}).then(function (results) {
    return new Promise((resolve, reject) => {
      console.log('Successfully retrieved ' + results.body.length + ' ' + tableName)
      commit('setParseTable', { 'tableName': tableName, 'newValue': results.body })
      resolve()
    })
  },
  function (error) {
    return error.response
  })
}

const moduleStore = {
  namespaced: true,
  state: {
    showObservingPanel: false,

    noctuaSky: {
      loginStatus: 'loggedOut',
      userName: 'loggedOut',
      lastSavedLocationId: undefined,
      lastUsedObservingSetup: {
        'id': 'eyes_observation',
        'state': {}
      },
      observations: [],
      locations: []
    }
  },
  mutations: {
    toggleBool (state, varName) {
      state[varName] = !state[varName]
    },
    setLoginStatus (state, newValue) {
      if (NoctuaSkyClient.currentUser) {
        state.noctuaSky.userName = NoctuaSkyClient.currentUser.email
      }
      state.noctuaSky.loginStatus = newValue
    },
    setParseTable (state, {tableName, newValue}) {
      state.noctuaSky[tableName] = newValue
    },
    setLastSavedLocationId (state, newValue) {
      state.noctuaSky.lastSavedLocationId = newValue
    },
    setLastUsedObservingSetup (state, newValue) {
      state.noctuaSky.lastUsedObservingSetup = newValue
    }
  },
  actions: {
    initLoginStatus ({ dispatch, commit, state }) {
      var currentUser = NoctuaSkyClient.currentUser
      if (currentUser) {
        commit('setLoginStatus', 'loggedIn')
        return dispatch('loadUserData')
      }
    },
    signIn ({ dispatch, commit, state }, data) {
      commit('setLoginStatus', 'signInInProgress')
      return NoctuaSkyClient.login(data.email, data.password).then(function (user) {
        commit('setLoginStatus', 'loggedIn')
        console.log('loggedIn!')
        return dispatch('loadUserData')
      }, function (error) {
        console.log('Signin Error: ' + JSON.stringify(error))
        commit('setLoginStatus', 'loggedOut')
        throw error.response
      })
    },
    signUp ({ dispatch, commit, state }, data) {
      commit('setLoginStatus', 'signInInProgress')
      console.log(data)
      return NoctuaSkyClient.register(data.email, data.password, data.firstName, data.lastName).then(function (res) {
        commit('setLoginStatus', 'loggedOut')
        console.log('SignUp done')
        return res
      }, function (error) {
        console.log('Signup Error: ' + JSON.stringify(error))
        commit('setLoginStatus', 'loggedOut')
        throw error
      })
    },
    signOut ({ commit, state }) {
      NoctuaSkyClient.logout()
      commit('setLoginStatus', 'loggedOut')
    },
    loadUserData ({ commit, state }) {
      return loadParseTable(commit, 'locations').then(() => { return loadParseTable(commit, 'observations') })
    },
    addObservation ({ dispatch, commit, state }, data) {
      // Backend can't deal with compacted form of observingSetup (a simple string)
      data.observingSetup = data.observingSetup.id ? data.observingSetup : {id: data.observingSetup, state: {}}
      commit('setLastSavedLocationId', data.location)
      commit('setLastUsedObservingSetup', data.observingSetup)

      if (data.id) {
        // We are editing an existing observation
        console.log('Updating observation ' + data.id)
        let obsId = data.id
        return NoctuaSkyClient.observations.update({obs_id: obsId, body: data})
      }
      let obs
      return NoctuaSkyClient.observations.add({body: data}).then(function (res) { obs = res.body; return loadParseTable(commit, 'observations') },
        function (error) { console.log('Failed to create new observation:'); console.log(JSON.stringify(error.response)) }).then(() => { return obs })
    },
    deleteObservations ({ dispatch, commit, state }, data) {
      return NoctuaSkyClient.observations.delete({obs_id: data}).then(function () { return loadParseTable(commit, 'observations') })
    },
    addLocation ({ dispatch, commit, state }, data) {
      var nl
      return NoctuaSkyClient.locations.add({body: data}).then(function (newLoc) { nl = newLoc.body; return loadParseTable(commit, 'locations') }).then(() => { return nl })
    }
  }
}

export default moduleStore
