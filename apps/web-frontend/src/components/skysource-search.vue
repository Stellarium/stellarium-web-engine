// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <div style="position: relative;" v-click-outside="resetSearch">
    <v-text-field prepend-icon="mdi-magnify" :label="$t('Search...')" v-model="searchText" @keyup.native.esc="resetSearch()" hide-details single-line></v-text-field>
    <v-list dense v-if="showList" two-line :style="listStyle">
      <v-list-item v-for="source in autoCompleteChoices" :key="source.names[0]" @click="sourceClicked(source)">
        <v-list-item-action>
          <img :src="iconForSkySource(source)"/>
        </v-list-item-action>
        <v-list-item-content>
          <v-list-item-title>{{ nameForSkySource(source) }}</v-list-item-title>
          <v-list-item-subtitle>{{ typeToName(source.types[0]) }}</v-list-item-subtitle>
        </v-list-item-content>
      </v-list-item>
    </v-list>
  </div>
</template>

<script>
import swh from '@/assets/sw_helpers.js'
import vClickOutside from 'v-click-outside'
import _ from 'lodash'

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
      swh.querySkySources(str, 10).then(results => {
        if (str !== that.lastQuery) {
          console.log('Cancelled query: ' + str)
          return
        }
        that.autoCompleteChoices = results
      }, err => { console.log(err) })
    }, 200),
    nameForSkySource: function (s) {
      const cn = swh.cleanupOneSkySourceName(s.match)
      const n = swh.nameForSkySource(s)
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
  },
  directives: {
    clickOutside: vClickOutside.directive
  }
}
</script>

<style>

</style>
