// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

<div id="observing-panel-container" :class="{observingpanelhidden: !$store.state.showSidePanel}" class="get-click">
  <div class="observing-panel-tabsbtn" >
    <v-btn v-for="tab in tabs" small :key="tab.tabName" :to="tab.url">{{ tab.tabName }}</v-btn>
  </div>
  <div id="observing-panel">
    <transition name="fade" mode="out-in">
      <router-view/>
    </transition>
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
      return this.$store.state.showSidePanel
    },
    tabs: function () {
      let res = []
      for (let i in this.$stellariumWebPlugins()) {
        let plugin = this.$stellariumWebPlugins()[i]
        if (plugin.observingRoutes) {
          for (let j in plugin.observingRoutes) {
            let r = plugin.observingRoutes[j]
            if (r.meta && r.meta.tabName) {
              res.push({ tabName: r.meta.tabName, url: r.path })
            }
          }
        }
      }
      return res
    }
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
