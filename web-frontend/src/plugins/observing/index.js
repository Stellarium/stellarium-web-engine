// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

import ObservingPanel from './components/observing-panel.vue'
import storeModule from './store'

export default {
  vuePlugin: {
    install: (Vue, options) => {
      Vue.component('observing-panel', ObservingPanel)
    }
  },
  name: 'observing',
  storeModule: storeModule,
  menuItems: [{header: 'Observing'},
    {title: 'Show Observing Panel', icon: 'visibility', store_var_name: 'plugins.observing.showObservingPanel', switch: 'true'}
  ],
  guiComponents: ['observing-panel']
}
