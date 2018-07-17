// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

<v-app dark>
  <div id="toolbar-image">
    <v-toolbar class="transparent" dense dark>
      <img id="stellarium-web-toolbar-logo" src="/static/images/logo.svg" width="30" height="30" alt="Stellarium Web Logo"/>
      <span class="tbtitle">Stellarum<sup>Web</sup></span>
      <v-spacer></v-spacer>
    </v-toolbar>
  </div>

  <div style="padding: 20px; width: 100vw;">
    <div style="max-width: 70vw; margin-left: auto; margin-right: auto;">
      <h2 class="white--text">Stellarium Web Latest News</h2>
      <template v-for="(item,i) in news">
        <news-post :post='item'></news-post>
      </template>
    </div>
  </div>

</v-app>
</template>

<script>
import NewsPost from './news-post.vue'

export default {
  data: function () {
    const files = require.context('markdown-with-front-matter-loader!@/plugins/news/posts/', false, /.md$/)
    var news = []
    for (var i of files.keys()) {
      var content = files(i)
      news.unshift(content)
    }
    return { news: news }
  },
  components: {
    NewsPost
  }
}
</script>

<style>


#toolbar-image {
  background: url("/static/images/header.png") center;
  background-position-x: 55px;
  background-position-y: 0px;
  height: 48px;
}

#stellarium-web-toolbar-logo {
  margin-right: 10px;
  margin-left: 30px;
}

.tbtitle {
  font-size: 20px;
  font-weight: 500;
}

a {
  color: #82b1ff;
}

a:link {
  text-decoration-line: none;
}

.divider {
  margin-top: 20px;
  margin-bottom: 20px;
}

ul {
padding-left: 30px;
}

</style>
