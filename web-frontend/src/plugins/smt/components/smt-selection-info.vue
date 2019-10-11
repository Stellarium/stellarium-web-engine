// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>
  <v-container class="pa-0 smt-selection-box">
    <v-card style="background: rgba(66, 66, 66, 0.7)">
      <v-card-title><div class="headline">Selected Footprint</div></v-card-title>
      <v-card-text>
        <tree-view :data="selectionData" :options="{maxDepth: 2}"></tree-view>
      </v-card-text>
    </v-card>
  </v-container>
</template>

<script>
export default {
  data: function () {
    return {
    }
  },
  props: ['selectionData'],
  methods: {
  }
}
</script>

<style>
.smt-selection-box {
  position: fixed;
  left: 5px;
  top: 53px;
  width: 600px;
}
</style>
