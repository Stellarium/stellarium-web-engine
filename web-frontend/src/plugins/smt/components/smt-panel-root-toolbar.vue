// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>

<v-toolbar dark dense>
  <v-btn icon to="/"><v-icon>close</v-icon></v-btn>
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
</style>
