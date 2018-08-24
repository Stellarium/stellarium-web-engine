// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

import ObservationsPage from './components/observations-page.vue'
import storeModule from './store'

export default {
  vuePlugin: {
    install: (Vue, options) => {
      Vue.component('observations-page', ObservationsPage)
    }
  },
  name: 'observing',
  storeModule: storeModule,
  observingRoutes: [
    { path: '/observing/observations', component: ObservationsPage, meta: { tabName: 'My Observations' } },
    { path: '/observing', redirect: '/observing/observations' }
  ]
}
