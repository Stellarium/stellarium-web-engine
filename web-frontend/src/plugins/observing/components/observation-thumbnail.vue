// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <v-card flat tile @click.capture="thumbClicked()" style="cursor: pointer;">
    <v-card-text class="text-xs-center" style="padding-left: 0px; padding-right: 0px; padding-bottom: 5px;">
      <img :src="obsIcon(obsData)" height="70px" style="min-height: 70px"></img>
      <div class="text-xs-center" style="overflow: hidden; white-space: nowrap; text-overflow: ellipsis;">{{ obsName }}</div>
      <div><img :src="instIcon(obsData)" height="20px" style="padding-right: 10px"></img><span v-html="ratingHtml(obsData)"></span></div>
    </v-card-text>
  </v-card>
</template>

<script>
import NoctuaSkyClient from '@/assets/noctuasky-client'
import { swh } from '@/assets/sw_helpers.js'

export default {
  props: ['obsData'],
  data: function () {
    return {
    }
  },
  computed: {
    obsName: function () {
      return this.obsData.title ? this.obsData.title : swh.nameForSkySource(this.obsData.target)
    }
  },
  methods: {
    instIcon: function (obsData) {
      return NoctuaSkyClient.iconForObservingSetup(obsData.observingSetup)
    },
    obsIcon: function (obsData) {
      return swh.iconForObservation(obsData)
    },
    ratingHtml: function (obsData) {
      var res = '<img src="/static/images/svg/ui/star_rate.svg" height="15px"></img>'.repeat(obsData.rating)
      res += '<img src="/static/images/svg/ui/star_border.svg"  height="15px" style="opacity: 0.1;"></img>'.repeat(5 - obsData.rating)
      return res
    },
    thumbClicked: function () {
      this.$emit('thumbClicked')
    }
  }
}
</script>

<style>
</style>
