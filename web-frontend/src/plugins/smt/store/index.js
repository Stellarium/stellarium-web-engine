// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

import _ from 'lodash'

const moduleStore = {
  namespaced: true,
  state: {
    status: undefined,
    watermarkImage: '',
    dataLoadingImage: ''
  },
  mutations: {
    setValue (state, { varName, newValue }) {
      _.set(state, varName, newValue)
    }
  },
  actions: {
  }
}

export default moduleStore
