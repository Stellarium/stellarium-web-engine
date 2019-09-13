// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>
  <div style="height: 100%;">
    <smt-panel-root-toolbar></smt-panel-root-toolbar>
    <div class="scroll-container">
      <v-container fluid style="height: 100%">
        <v-card>
          <v-card-title primary-title>
            <h3 class="headline mb-0">Layer</h3>
          </v-card-title>
          <v-card-text>
            <v-layout column justify-space-between>
              <p>Count: {{ results.summary.count }}</p>
              <p>Nb constraints: {{ query.constraints.length }}</p>
            </v-layout>
          </v-card-text>
        </v-card>
        <v-layout column>
          <v-flex xs12 v-for="field in $smt.fieldsList" :key="field.id">
            {{ field.name }}
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
      query: {
        constraints: []
      },
      results: {
        summary: {
          count: 0
        },
        fields: []
      }
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
