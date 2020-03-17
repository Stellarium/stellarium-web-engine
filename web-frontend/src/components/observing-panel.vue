// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

<div id="observing-panel-container" :class="{observingpanelhidden: !$store.state.showSidePanel}" class="get-click">
  <div class="observing-panel-tabsbtn" v-if="$store.state.showObservingPanelTabsButtons">
    <v-btn class='tab-bt' v-for="tab in tabs" small :key="tab.tabName" :to="tab.url" active-class="tab-bt-active">{{ $t(tab.tabName) }}</v-btn>
  </div>
  <div id="observing-panel">
    <router-view style="height: 100%"/>
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
      const res = []
      for (const i in this.$stellariumWebPlugins()) {
        const plugin = this.$stellariumWebPlugins()[i]
        if (plugin.panelRoutes) {
          for (const j in plugin.panelRoutes) {
            const r = plugin.panelRoutes[j]
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
  width: 100vh;
  top: 12px;
  text-align: right;
}
.observingpanelhidden {
  display: none;
}
.tab-bt {
  opacity: 0.5;
}
.tab-bt-active {
  opacity: 1;
}
</style>
