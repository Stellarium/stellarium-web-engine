// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
<v-dialog lazy max-width='600' v-model="$store.state.showLangSettingsDialog">
<v-card v-if="$store.state.showLangSettingsDialog" class="secondary white--text">
  <v-card-title><div class="headline">{{ $t('ui.lang_settings_dialog.lang_settings') }}</div></v-card-title>
  <v-layout row wrap>
    <div style="margin: 24px;">
      <p>Change application's language to:</p>
      <v-select
        :items="items"
        :label="$t('ui.lang_settings_dialog.select_lang')"
        single-line
        item-text="long_name"
        item-value="short_name"
        return-object
        persistent-hint
        v-on:change="changeLang"
      ></v-select>
    </div>
  </v-layout>
  <v-card-actions>
    <v-spacer></v-spacer><v-btn class="blue--text darken-1" flat @click.native="$store.state.showLangSettingsDialog = false">{{ $t('ui.common.close') }}</v-btn>
  </v-card-actions>
</v-card>
</v-dialog>
</template>

<script>
import Vue from 'vue'
import langs from '../plugins/langs.js'

export default {
  data () {
    return {
      // add your language here as a new object
      items: [
        { long_name: 'English', short_name: 'en' },
        { long_name: 'Polski', short_name: 'pl' }
      ]
    }
  },
  methods: {
    changeLang (a) {
      Vue.config.lang = a.short_name
      this.$i18n.locale = Vue.config.lang
      langs.updateLanguage(a.short_name)
    }
  }
}
</script>
