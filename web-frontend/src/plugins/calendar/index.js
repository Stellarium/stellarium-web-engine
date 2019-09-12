// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

import Calendar from './components/calendar.vue'

export default {
  name: 'calendar',
  panelRoutes: [
    { path: '/p/calendar', component: Calendar, meta: { tabName: 'Calendar', prio: 2 } }
  ]
}
