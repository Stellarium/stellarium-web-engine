// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

<div class="click-through" style="position:absolute; width: 100%; height: 100%; display:flex; align-items: flex-end;">
  <toolbar class="get-click"></toolbar>
  <observing-panel></observing-panel>
  <template v-for="item in pluginsGuiComponents">
    <component :is="item"></component>
  </template>
  <template v-for="item in dialogs">
    <component :is="item"></component>
  </template>
  <selected-object-info style="position: absolute; top: 48px; left: 0px; width: 430px; max-width: calc(100vw - 12px); margin: 6px" class="get-click"></selected-object-info>
  <progress-bars style="position: absolute; bottom: 54px; right: 12px;"></progress-bars>
  <bottom-bar style="position:absolute; width: 100%; justify-content: center; bottom: 0; display:flex; margin-bottom: 0px" class="get-click"></bottom-bar>
</div>

</template>

<script>
import Toolbar from '@/components/toolbar.vue'
import BottomBar from '@/components/bottom-bar.vue'
import SelectedObjectInfo from '@/components/selected-object-info.vue'
import ProgressBars from '@/components/progress-bars'

import AboutDialog from '@/components/about-dialog.vue'
import DataCreditsDialog from '@/components/data-credits-dialog.vue'
import PrivacyDialog from '@/components/privacy-dialog.vue'
import ViewSettingsDialog from '@/components/view-settings-dialog.vue'
import SkyThisMonth from '@/components/sky-this-month.vue'
import PlanetsVisibility from '@/components/planets-visibility.vue'
import LocationDialog from '@/components/location-dialog.vue'
import ObservingPanel from '@/components/observing-panel.vue'

export default {
  data: function () {
    return {
      dialogs: [
        'about-dialog',
        'data-credits-dialog',
        'privacy-dialog',
        'view-settings-dialog',
        'sky-this-month',
        'planets-visibility',
        'location-dialog']
    }
  },
  methods: {
  },
  computed: {
    pluginsGuiComponents: function () {
      let res = []
      for (let i in this.$stellariumWebPlugins()) {
        let plugin = this.$stellariumWebPlugins()[i]
        if (plugin.guiComponents) {
          res = res.concat(plugin.guiComponents)
        }
      }
      return res
    }
  },
  components: { Toolbar, BottomBar, AboutDialog, DataCreditsDialog, PrivacyDialog, ViewSettingsDialog, SkyThisMonth, PlanetsVisibility, SelectedObjectInfo, LocationDialog, ProgressBars, ObservingPanel }
}
</script>

<style>
</style>
