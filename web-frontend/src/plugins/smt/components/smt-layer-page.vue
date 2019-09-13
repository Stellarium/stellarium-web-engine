// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>
  <div style="height: 100%;">
    <smt-panel-root-toolbar></smt-panel-root-toolbar>
    <div class="scroll-container">
      <v-container fluid style="height: 100%">
        <v-layout row>
          <v-flex xs12 v-for="obsg in observationGroups" :key="obsg.f1[0].id">
            <grouped-observations :obsGroupData="obsg" @thumbClicked="thumbClicked"></grouped-observations>
          </v-flex>
        </v-layout>
      </v-container>
    </div>
  </div>
</template>

<script>
import SmtPanelRootToolbar from './smt-panel-root-toolbar.vue'

export default {
  data: function () {
    return {
      observationGroups: []
    }
  },
  methods: {
    thumbClicked: function (obs) {
      this.$router.push('/p/observations/' + obs.id)
    },
    refreshObservationGroups: function () {
    }
  },
  watch: {
    '$store.state.selectedObject': function () {
      this.refreshObservationGroups()
    }
  },
  computed: {
  },
  mounted: function () {
    this.refreshObservationGroups()
  },
  components: { SmtPanelRootToolbar }
}
</script>

<style>
.scroll-container {
  height: calc(100% - 48px);
  overflow-y: auto;
  backface-visibility: hidden;
}
ul {
  padding-left: 30px;
}
</style>
