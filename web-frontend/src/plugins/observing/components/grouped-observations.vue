// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <div style="margin-bottom: 15px">
    <div>{{ title }}</div>
    <v-container fluid grid-list-sm="true">
      <v-layout row wrap>
        <v-flex xs4 v-for="(obs, index) in obsGroupData.f1" :key="obs.id">
          <observation-thumbnail :obsData="obs" @thumbClicked="thumbClicked(obs)"></observation-thumbnail>
        </v-flex>
      </v-layout>
    </v-container>
  </div>
</template>

<script>
import ObservationThumbnail from './observation-thumbnail.vue'
import { swh } from '@/assets/sw_helpers.js'
import { nsh } from '../ns_helpers.js'
import Moment from 'moment'

export default {
  props: ['obsGroupData'],
  data: function () {
    return {
    }
  },
  computed: {
    title: function () {
      if (this.obsGroupData.f0 !== undefined) {
        var dateMin = new Date()
        dateMin.setMJD(this.obsGroupData.groupDate.min)
        dateMin = Moment(dateMin)
        var dayString = dateMin.date()
        var humanFriendlyDaytime = 'Evening'
        if (this.obsGroupData.f0 > 1) {
          var dateMax = new Date()
          dateMax.setMJD(this.obsGroupData.groupDate.max)
          dateMax = Moment(dateMax)
          if (dateMin.date() !== dateMax.date()) {
            dayString = dateMin.date() + '-' + dateMax.date()
            humanFriendlyDaytime = 'Night'
          } else {
            if (dateMin.hours() < 12) {
              humanFriendlyDaytime = 'Morning'
            }
          }
        }
        var loc = nsh.locationForId(this, this.obsGroupData.f1[0].location)
        var locName = loc ? loc.shortName : 'Unknown Location'
        return dayString + ' ' + swh.monthNames[dateMin.month()] + ', ' + humanFriendlyDaytime + ' in ' + locName
      }
    }
  },
  methods: {
    thumbClicked: function (obs) {
      this.$emit('thumbClicked', obs)
    }
  },
  components: { ObservationThumbnail }
}
</script>

<style>
</style>
