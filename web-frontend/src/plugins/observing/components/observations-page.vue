// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <div style="height: 100%;">
    <observing-panel-root-toolbar></observing-panel-root-toolbar>
    <div class="scroll-container">
      <v-container fluid style="height: 100%">
        <div v-if="this.$store.state.noctuaSky.status !== 'loggedIn'" style="text-align: center; margin-bottom: 15px; color: red;">Warning: you are not logged in, you can add and edit observations, but they all will be suppressed if you leave the page, or login with your account.</div>
        <v-layout row wrap>
          <v-flex xs12 v-for="obsg in observationGroups" :key="obsg.f1[0].id">
            <grouped-observations :obsGroupData="obsg" @thumbClicked="thumbClicked"></grouped-observations>
          </v-flex>
        </v-layout>
        <div v-if="showEmptyMessage" style="margin-top: 40vh; text-align: center;">Nothing here yet..<br>Add new observations by clicking on the + button below</div>
        <v-layout row wrap>
          <v-btn fab dark absolute color="pink" bottom right class="pink" slot="activator" style="bottom: 16px; margin-right: 10px" to="/observing/observations/create">
            <v-icon dark>add</v-icon>
          </v-btn>
            <v-tooltip left>
              <span>Log a new Observation</span>
            </v-tooltip>
        </v-layout>
      </v-container>
    </div>
  </div>
</template>

<script>
import ObservingPanelRootToolbar from '@/components/observing-panel-root-toolbar.vue'
import GroupedObservations from './grouped-observations.vue'
import NoctuaSkyClient from '@/assets/noctuasky-client'
import swh from '@/assets/sw_helpers.js'

export default {
  data: function () {
    return {
    }
  },
  created: function () {
    let that = this
    swh.addSelectedObjectExtraButtons({
      id: 'observe',
      name: 'Observation',
      icon: 'add',
      callback: function (b) {
        that.$router.push('/observing/observations/create_from_selection')
      }
    })
  },
  methods: {
    thumbClicked: function (obs) {
      this.$router.push('/observing/observations/' + obs.id)
    }
  },
  computed: {
    showEmptyMessage: function () {
      return this.observationGroups.length === 0
    },
    observationGroups: function () {
      var aggregator = [
        { $group: { id: { location: '$location', julian_day: { $floor: '$mjd' } }, groupDate: { min: { $min: '$mjd' }, max: { $max: '$mjd' } }, f0: { $sum: 1 }, f1: { $push: '$$ROOT' } } },
        { $sort: { 'groupDate.max': -1 } }
      ]
      return NoctuaSkyClient.observations.aggregate(aggregator)
    }
  },
  components: { GroupedObservations, ObservingPanelRootToolbar }
}
</script>

<style>
.scroll-container {
  height: calc(100% - 48px);
  overflow-y: auto;
  backface-visibility: hidden;
}
</style>
