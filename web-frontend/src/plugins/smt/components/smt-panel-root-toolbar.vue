// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>

<v-toolbar dense class="obspanel-toolbar">
  <v-btn icon to="/"><v-icon>mdi-close</v-icon></v-btn>
  <v-spacer></v-spacer>
  <span>{{ $store.state.SMT.status }}</span>
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
