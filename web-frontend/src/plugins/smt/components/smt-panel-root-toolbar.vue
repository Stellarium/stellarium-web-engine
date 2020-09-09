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
