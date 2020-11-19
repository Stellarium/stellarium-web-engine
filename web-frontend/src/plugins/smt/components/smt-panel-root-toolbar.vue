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
  <span class="px-2 grey--text">SMT - <a href="https://github.com/Stellarium-Labs/smt/blob/survey-monitoring-tool/smt-server/CHANGELOG.md" target="_blank">{{ $store.state.SMT.smtServerInfo.version }}</a></span>
  <v-spacer></v-spacer>
  <v-progress-circular v-if="$store.state.SMT.status === 'loading'" size=18 indeterminate></v-progress-circular>
  <span class="px-2">Server {{ $store.state.SMT.status }}</span>
  <v-btn icon class="transparent" @click.stop="dialog = true"><v-icon>mdi-information-outline</v-icon></v-btn>

  <v-dialog v-model="dialog" max-width="600">
    <v-card>
      <v-card-title class="headline">SMT Server Info</v-card-title>
      <v-card-text>
        SMT Server version: <b>{{ $store.state.SMT.smtServerInfo.version }}</b><br>
        Displaying data coming from server: <b>{{ $store.state.SMT.smtServerInfo.dataGitServer }}</b><br>
        Git Branch: <b>{{ $store.state.SMT.smtServerInfo.dataGitBranch }}</b><br>
        Last ref: <b>{{ $store.state.SMT.smtServerInfo.dataGitSha1 }}</b>
      </v-card-text>
      <v-card-actions>
        <v-spacer></v-spacer>
        <v-btn color="green darken-1" text @click="dialog = false">OK</v-btn>
      </v-card-actions>
    </v-card>
  </v-dialog>
</v-toolbar>

</template>

<script>

export default {
  data: function () {
    return {
      dialog: false
    }
  },
  methods: {
    showSMTServerInfo: function () {

    }
  },
  computed: {
  }
}
</script>

<style>
.obspanel-toolbar {
  background-color: #212121!important;
  flex: 0 1 auto;
}
</style>
