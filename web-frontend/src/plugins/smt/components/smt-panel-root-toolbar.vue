// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>

<v-toolbar dense class="obspanel-toolbar">
  <span class="px-2 grey--text">SMT - dev version</span>
  <v-spacer></v-spacer>
  <v-progress-circular v-if="$store.state.SMT.status === 'loading'" size=18 indeterminate></v-progress-circular>
  <span class="px-2">{{ $store.state.SMT.status }}</span>
</v-toolbar>

</template>

<script>

export default {
  data: function () {
    return {
    }
  },
  methods: {
  },
  computed: {
  }
}
</script>

<style>
.obspanel-toolbar {
  background-color: #212121!important;
}
</style>
