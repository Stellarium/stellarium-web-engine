// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

<div id="observing-panel-container" :class="{observingpanelhidden: !$store.state.showObservingPanel}" class="get-click">
  <div class="observing-panel-tabsbtn" >
    <v-btn v-for="tab in tabs" small :key="tab">{{ tab }}</v-btn>
  </div>
  <div id="observing-panel">
    <component :is="observingPanelCurrentComponent"></component>
  </div>
</div>

</template>

<script>
export default {
  data: function () {
    return {
    }
  },
  computed: {
    showObservingPanel: function () {
      return this.$store.state.showObservingPanel
    },
    observingPanelCurrentComponent: function () {
      return this.$store.state.observingPanelCurrentComponent
    },
    pluginsObservingPanelComponents: function () {
      let res = []
      for (let i in this.$stellariumWebPlugins()) {
        let plugin = this.$stellariumWebPlugins()[i]
        if (plugin.observingPanelComponent) {
          res.push(plugin.observingPanelComponent)
        }
      }
      return res
    },
    tabs: function () {
      let res = []
      for (let i in this.$stellariumWebPlugins()) {
        let plugin = this.$stellariumWebPlugins()[i]
        if (plugin.observingPanelComponent) {
          res.push(plugin.observingTabName)
        }
      }
      return res
    }
  },
  watch: {
    showObservingPanel: function (v) {
      this.$store.commit('setValue', {varName: 'showSidePanel', newValue: v})
    }
  },
  mounted: function () {
    this.$store.state.observingPanelCurrentComponent = this.pluginsObservingPanelComponents[0]
  }
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
.observing-panel-tabsbtn {
  position:absolute;
  right: 400px;
  transform-origin: bottom right;
  transform: rotate(-90deg);
  opacity: 0.5;
  width: 100vh;
  top: 12px;
  text-align: right;
}
.observingpanelhidden {
  display: none;
}
</style>
