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

import _ from 'lodash'

const moduleStore = {
  namespaced: true,
  state: {
    status: undefined,
    watermarkImage: '',
    smtServerInfo: {
      version: '',
      dataGitServer: '',
      dataGitBranch: '',
      dataGitSha1: ''
    }
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
