// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <div style="position: relative;">
    <v-text-field prepend-icon="search" label="Search..." v-model="searchText" @keyup.native.esc="resetSearch()" hide-details single-line dark v-click-outside="resetSearch"></v-text-field>
    <v-list dense v-if="showList" two-line :style="listStyle" class="get-click">
      <v-list-tile v-for="source in autoCompleteChoices" :key="source.nsid" @click="sourceClicked(source)">
        <v-list-tile-action>
          <img :src="iconForSkySource(source)"/>
        </v-list-tile-action>
        <v-list-tile-content>
          <v-list-tile-title>{{ nameForSkySource(source) }}</v-list-tile-title>
          <v-list-tile-sub-title>{{ typeToName(source.types[0]) }}</v-list-tile-sub-title>
        </v-list-tile-content>
      </v-list-tile>
    </v-list>
  </div>
</template>

<script>
import swh from '@/assets/sw_helpers.js'
import _ from 'lodash'
import NoctuaSkyClient from '@/assets/noctuasky-client'

export default {
  data: function () {
    return {
      autoCompleteChoices: [],
      searchText: '',
      lastQuery: undefined
    }
  },
  props: ['value', 'floatingList'],
  watch: {
    searchText: function () {
      if (this.searchText === '') {
        this.autoCompleteChoices = []
        this.lastQuery = undefined
        return
      }
      this.refresh()
    }
  },
  computed: {
    listStyle: function () {
      return this.floatingList ? 'position: absolute; z-index: 1000; margin-top: 8px' : ''
    },
    showList: function () {
      return this.searchText !== ''
    }
  },
  methods: {
    sourceClicked: function (val) {
      this.$emit('input', val)
      this.resetSearch()
    },
    resetSearch: function () {
      this.searchText = ''
    },
    refresh: _.debounce(function () {
      var that = this
      let str = that.searchText
      str = str.toUpperCase()
      str = str.replace(/\s+/g, '')
      if (this.lastQuery === str) {
        return
      }
      this.lastQuery = str
      NoctuaSkyClient.skysources.query(str, 10).then(results => {
        if (str !== that.lastQuery) {
          console.log('Cancelled query: ' + str)
          return
        }
        that.autoCompleteChoices = results
      }, err => { console.log(err) })
    }, 200),
    nameForSkySource: function (s) {
      let cn = swh.cleanupOneSkySourceName(s.match)
      let n = swh.nameForSkySource(s)
      if (cn === n) {
        return n
      } else {
        return cn + ' (' + n + ')'
      }
    },
    typeToName: function (t) {
      return swh.nameForSkySourceType(t)
    },
    iconForSkySource: function (s) {
      return swh.iconForSkySource(s)
    }
  },
  mounted: function () {
    var that = this
    const onClick = e => {
      if (that.searchText !== '') {
        that.searchText = ''
      }
    }
    const guiParent = document.querySelector('stel') || document.body
    guiParent.addEventListener('click', onClick, false)
  }
}
</script>

<style>

</style>
