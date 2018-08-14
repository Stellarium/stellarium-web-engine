// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

import News from './components/News'

export default {
  name: 'news',
  menuItems: [
    {title: 'News', icon: 'rss_feed', store_var_name: 'dummy', link: '/news'}
  ],
  routes: [
    {
      path: '/news',
      name: 'News',
      component: News
    }
  ]
}
