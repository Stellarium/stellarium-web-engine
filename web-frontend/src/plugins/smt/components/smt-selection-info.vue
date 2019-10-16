// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>
  <v-container class="pa-0 smt-selection-box">
    <v-card style="background: rgba(66, 66, 66, 0.7)">
      <v-btn icon style="position: absolute; right: 0" v-on:click="unselect()"><v-icon>mdi-close</v-icon></v-btn>
      <v-card-title>
        <div class="headline">Footprint {{ currentIndex + 1 }} / {{ selectionData.length }}
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

export default {
  data: function () {
    return {
      currentIndex: 0
    }
  },
  props: ['selectionData'],
  computed: {
    currentItem: function () {
      return this.selectionData.length ? this.selectionData[this.currentIndex].properties : {}
    }
  },
  methods: {
    unselect: function () {
      this.$emit('unselect')
    },
    decIndex: function () {
      if (this.currentIndex === 0) this.currentIndex = this.selectionData.length - 1
      else this.currentIndex--
    },
    incIndex: function () {
      if (this.currentIndex >= this.selectionData.length - 1) this.currentIndex = 0
      else this.currentIndex++
    }
  },
  watch: {
    selectionData: function () {
      this.currentIndex = 0
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
}
</style>
