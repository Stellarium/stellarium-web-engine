// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

var $parse = require('parse')
$parse.initialize('noctuasky')
$parse.serverURL = 'https://api.noctuasky.org/parse'

var loadParseTable = function (commit, tableName, fieldIncludes) {
  var objType = $parse.Object.extend(tableName)
  var query = new $parse.Query(objType)
  for (var finc in fieldIncludes) {
    query.include(fieldIncludes[finc])
  }
  return query.find().then(function (results) {
    return new Promise((resolve, reject) => {
      console.log('Successfully retrieved ' + results.length + ' ' + tableName)
      commit('setParseTable', { 'tableName': tableName, 'newValue': JSON.parse(JSON.stringify(results)) })
      resolve()
    })
  },
  function (error) {
    console.log('Error: ' + error.code + ' ' + error.message)
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
      Observation: [],
      Location: []
    }
  },
  mutations: {
    toggleBool (state, varName) {
      state[varName] = !state[varName]
    },
    setLoginStatus (state, newValue) {
      var currentUser = $parse.User.current()
      if (currentUser) {
        state.noctuaSky.userName = $parse.User.current().get('username')
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
      var currentUser = $parse.User.current()
      if (currentUser) {
        commit('setLoginStatus', 'loggedIn')
        return dispatch('loadUserData')
      }
    },
    signIn ({ dispatch, commit, state }, data) {
      commit('setLoginStatus', 'signInInProgress')
      return $parse.User.logIn(data.userName, data.password).then(function (user) {
        commit('setLoginStatus', 'loggedIn')
        return dispatch('loadUserData')
      }, function (error) {
        console.log('Signin Error: ' + JSON.stringify(error))
        commit('setLoginStatus', 'loggedOut')
        return error
      })
    },
    signUp ({ dispatch, commit, state }, data) {
      commit('setLoginStatus', 'signInInProgress')
      var user = new $parse.User()
      user.set('username', data.userName)
      user.set('password', data.password)
      user.set('email', data.email)
      return user.signUp({firstName: data.firstName, lastName: data.lastName}).then(function (user) {
        commit('setLoginStatus', 'loggedIn')
        return dispatch('loadUserData')
      }, function (error) {
        console.log('Signup Error: ' + JSON.stringify(error))
        commit('setLoginStatus', 'loggedOut')
        return error
      })
    },
    signOut ({ commit, state }) {
      $parse.User.logOut().then(() => {
        commit('setLoginStatus', 'loggedOut')
      })
    },
    loadUserData ({ commit, state }) {
      return Promise.all([
        loadParseTable(commit, 'Location')]).then(() => { return loadParseTable(commit, 'Observation') })
    },
    addObservation ({ dispatch, commit, state }, data) {
      var Observation = $parse.Object.extend('Observation')

      if (data.objectId && !data.parseObj) {
        // We are editing an existing observation
        console.log('Updating observation ' + data.objectId)
        var query = new $parse.Query(Observation)
        return query.get(data.objectId).then((parseObj) => { data.parseObj = parseObj; return dispatch('addObservation', data) })
      }

      var obs = data.parseObj ? data.parseObj : new Observation()
      obs.set('owner', $parse.User.current())
      obs.set('target', data.target)
      obs.set('difficulty', data.difficulty)
      obs.set('rating', data.rating)
      obs.set('comment', data.comment)
      obs.set('mjd', data.mjd)
      obs.set('locationRef', data.locationRef)
      obs.set('locationPublicData', data.locationPublicData)

      // Backend can't deal with compacted form of observingSetup (a simple string)
      var obss = data.observingSetup.id ? data.observingSetup : {id: data.observingSetup, state: {}}
      obs.set('observingSetup', obss)
      commit('setLastSavedLocationId', data.locationRef)
      commit('setLastUsedObservingSetup', data.observingSetup)

      return obs.save().then(function () { return loadParseTable(commit, 'Observation') },
        function (error) { alert('Failed to create new observation, with error code: ' + error.message) }).then(() => { return obs })
    },
    deleteObservations ({ dispatch, commit, state }, data) {
      var Observation = $parse.Object.extend('Observation')
      var allObs = []
      data.forEach(function (id) {
        var obs = new Observation()
        obs.id = id
        allObs.push(obs)
      })
      return $parse.Object.destroyAll(allObs).then(function () { return loadParseTable(commit, 'Observation') })
    },
    addLocation ({ dispatch, commit, state }, data) {
      var Location = $parse.Object.extend('Location')
      var loc = new Location()
      loc.set('shortName', data.shortName)
      loc.set('streetAddress', data.streetAddress)
      loc.set('country', data.country)
      loc.set('lat', data.lat)
      loc.set('lng', data.lng)
      loc.set('alt', data.alt)
      loc.set('owner', $parse.User.current())
      loc.set('publicData', data.publicData)
      var nl
      return loc.save().then(function (newLoc) { nl = newLoc; return loadParseTable(commit, 'Location') }).then(() => { return nl })
    }
  }
}

export default moduleStore
