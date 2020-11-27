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
  <v-container v-if="selectionData !== undefined" class="pa-0 smt-selection-box">
    <v-card style="background: rgba(66, 66, 66, 0.7)">
      <v-btn icon style="position: absolute; right: 0" v-on:click="unselect()"><v-icon>mdi-close</v-icon></v-btn>
      <v-card-title>
        <div class="headline">Footprint {{ currentIndex + 1 }} / {{ selectionData ? selectionData.count : 0 }}
          <v-btn icon v-on:click="decIndex()"><v-icon>mdi-chevron-left</v-icon></v-btn>
          <v-btn icon v-on:click="incIndex()"><v-icon>mdi-chevron-right</v-icon></v-btn>
        </div>
      </v-card-title>
      <v-card-text>
        <vue-json-pretty :data="currentItem" :deep="2" :showLength="true" :showDoubleQuotes="false" :showLine="false"></vue-json-pretty>
      </v-card-text>
    </v-card>
  </v-container>
</template>

<script>
import VueJsonPretty from 'vue-json-pretty'
import qe from '../query-engine'

export default {
  data: function () {
    return {
      currentIndex: 0,
      selectionData: undefined
    }
  },
  props: ['selectedFeatures', 'query'],
  computed: {
    currentItem: function () {
      return this.selectionData && this.selectionData.features.length ? this.selectionData.features[this.currentIndex].properties : {}
    }
  },
  methods: {
    unselect: function () {
      this.$emit('unselect')
    },
    decIndex: function () {
      if (this.currentIndex === 0) this.currentIndex = this.selectionData.features.length - 1
      else this.currentIndex--
    },
    incIndex: function () {
      if (this.currentIndex >= this.selectionData.features.length - 1) this.currentIndex = 0
      else this.currentIndex++
    }
  },
  watch: {
    selectedFeatures: function () {
      const that = this
      this.currentIndex = 0
      if (!this.selectedFeatures || !this.selectedFeatures.length) {
        this.selectionData = undefined
        return
      }

      let featuresCount = 0
      this.selectedFeatures.forEach(f => { featuresCount += f.geogroup_size })
      const geogroupIds = this.selectedFeatures.map(f => f.geogroup_id)
      const hpIndices = [...new Set(this.selectedFeatures.map(f => f.healpix_index))]
      const q = {
        constraints: [
          { field: { id: 'geogroup_id', type: 'string' }, operation: 'IN', expression: geogroupIds, negate: false },
          { field: { id: 'healpix_index', type: 'number' }, operation: 'IN', expression: hpIndices, negate: false }
        ],
        projectOptions: {
          id: 1,
          properties: 1
        },
        limit: 50
      }
      q.constraints = that.query.constraints.concat(q.constraints)
      qe.query(q).then(qres => {
        that.currentIndex = 0
        if (!qres.res.length) {
          that.selectionData = undefined
          return
        }
        that.selectionData = {
          count: featuresCount,
          features: qres.res
        }
        console.assert(featuresCount === qres.res.length)
      })
    }
  },
  components: { VueJsonPretty }
}
</script>

<style>
.smt-selection-box {
  position: fixed;
  left: 5px;
  top: 53px;
  width: 450px;
  max-height: calc(100% - 150px);
  overflow-y: auto;
  backface-visibility: hidden;
}
</style>
