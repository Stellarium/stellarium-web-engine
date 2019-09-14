// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
<v-dialog lazy max-width='600' v-model="$store.state.showViewSettingsDialog">
<v-card v-if="$store.state.showViewSettingsDialog" class="secondary white--text">
  <v-card-title><div class="headline">{{ $t('ui.view_settings_dialog.view_settings') }}</div></v-card-title>
  <v-card-text>
    <v-checkbox :label="$t('ui.view_settings_dialog.milky_way')" v-model="milkyWayOn"></v-checkbox>
    <v-checkbox :label="$t('ui.view_settings_dialog.dss')" v-model="dssOn"></v-checkbox>
    <v-checkbox :label="$t('ui.view_settings_dialog.simulate_refraction')" v-model="refractionOn"></v-checkbox>
    <v-checkbox :label="$t('ui.view_settings_dialog.meridian_line')" v-model="meridianOn"></v-checkbox>
    <v-checkbox :label="$t('ui.view_settings_dialog.ecliptic_line')" v-model="eclipticOn"></v-checkbox>
    <v-checkbox :label="$t('ui.view_settings_dialog.orange_night_mode')" v-model="orangeOn"></v-checkbox>
  </v-card-text>
  <v-card-actions>
    <v-spacer></v-spacer><v-btn class="blue--text darken-1" flat @click.native="$store.state.showViewSettingsDialog = false">{{ $t('ui.common.close') }}</v-btn>
  </v-card-actions>
</v-card>
</v-dialog>
</template>

<script>

export default {
  data: function () {
    return {
    }
  },
  computed: {
    dssOn: {
      get: function () {
        return this.$store.state.stel.dss.visible
      },
      set: function (newValue) {
        this.$stel.core.dss.visible = newValue
      }
    },
    milkyWayOn: {
      get: function () {
        return this.$store.state.stel.milkyway.visible
      },
      set: function (newValue) {
        this.$stel.core.milkyway.visible = newValue
      }
    },
    refractionOn: {
      get: function () {
        return this.$store.state.stel.observer.refraction
      },
      set: function (newValue) {
        this.$stel.core.observer.refraction = newValue
      }
    },
    meridianOn: {
      get: function () {
        return this.$store.state.stel.lines.meridian.visible
      },
      set: function (newValue) {
        this.$stel.core.lines.meridian.visible = newValue
      }
    },
    eclipticOn: {
      get: function () {
        return this.$store.state.stel.lines.ecliptic.visible
      },
      set: function (newValue) {
        this.$stel.core.lines.ecliptic.visible = newValue
      }
    },
    orangeOn: {
      get: function () {
        return this.$store.state.orange
      },
      set: function (newValue) {
        this.$store.commit('toggleBool', 'orange')
        document.getElementById('nightmode').style.background = newValue ? '#ff6800' : '#ff0000'
      }
    }
  }
}
</script>

<style>
.input-group {
  margin: 0px;
}
</style>
