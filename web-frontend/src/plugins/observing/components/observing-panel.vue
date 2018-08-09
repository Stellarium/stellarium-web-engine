// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

<div id="observing-panel-container" v-if="$store.state.plugins.observing.showObservingPanel" class="get-click">
  <div id="observing-panel">
    <component v-bind:is="currentComponent"></component>
  </div>
</div>

</template>

<script>
import ObservationsPage from './observations-page.vue'

export default {
  data: function () {
    return {
    }
  },
  computed: {
    currentComponent: function () {
      return 'ObservationsPage'
    },
    showObservingPanel: function () {
      return this.$store.state.plugins.observing.showObservingPanel
    }
  },
  watch: {
    showObservingPanel: function (v) {
      this.$store.commit('setValue', {varName: 'showSidePanel', newValue: v})
    }
  },
  components: { ObservationsPage }
}
</script>

<style>
#observing-panel-container {
  position:absolute;
  height: 100%;
  width: 400px;
  right: -400px;
}
#observing-panel {
  height: 100%;
  color: white;
  background-color: #212121;
}
</style>
