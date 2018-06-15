// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

  <v-card style="margin-bottom: 40px;">
    <v-card-title><strong>{{ post.title }}</strong><v-spacer/><small>{{ date }}</small></v-card-title>
    <v-card-text>
      <p>
        <div v-html="post.__content"></div>
      </p>
    </v-card-text>
  </v-card>

</template>

<script>

import Moment from 'moment'

export default {
  props: ['post'],
  data: function () {
    return {
      date: Moment(this.post.date).format('MMM Do YYYY')
    }
  }
}
</script>

<style>
</style>
