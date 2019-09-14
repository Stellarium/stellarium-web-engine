// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <v-card v-if="selectedObject" transparent style="background: rgba(66, 66, 66, 0.3);">
    <div style="min-height: 100%; max-height: calc(100vh - 140px); overflow: auto;">
      <v-btn icon style="position: absolute; right: 0" v-on:click.native="unselect()"><v-icon>close</v-icon></v-btn>
      <v-card-title primary-title>
        <div style="width: 100%">
          <img :src="icon" height="48" width="48" align="left" style="margin-top: 3px; margin-right: 10px"/>
          <div style="overflow: hidden; text-overflow: ellipsis;">
            <div class="headline">{{ title }}</div>
            <span class="grey--text">{{ type }}</span>
          </div>
        </div>
      </v-card-title>
      <v-card-text style="padding-bottom: 5px;">
        <v-layout v-if="otherNames.length > 1" row wrap style="width: 100%;">
          <v-flex xs4 style="margin-top: -2px; color: #dddddd">{{ $t('ui.selected_object_info.known_as') }}</v-flex> <span class="caption" text-color="white" v-for="(mname, index) in otherNames" v-if="index > 0 && index < 8" :key="mname" style="margin-right: 15px; font-weight: 500">{{ mname }}</span>
          <v-btn small icon class="grey--text" v-if="otherNames.length > 8" v-on:click.native="showMinorNames = !showMinorNames" style="margin-top: -5px; margin-bottom: -5px;"><v-icon>more_horiz</v-icon></v-btn>
          <span class="caption" text-color="white" v-for="(mname, index) in otherNames" :key="mname"  v-if="showMinorNames && index >= 8" style="margin-right: 15px; font-weight: 500">{{ mname }}</span>
        </v-layout>
      </v-card-text>
      <v-card-text>
        <v-layout row wrap style="width: 100%">
          <template v-for="item in items">
            <v-flex xs4 style="color: #dddddd">{{ item.key }}</v-flex>
            <v-flex xs8 style="font-weight: 500"><span v-html="item.value"></span></v-flex>
          </template>
        </v-layout>
        <div style="margin-top: 15px" v-html="wikipediaSummary"></div>
        <div v-if="wikipediaSummary" class="grey--text caption" style="margin-left:auto; margin-right:0;"><i>{{ $t('ui.selected_object_info.read_more_on') }}<b><a style="color: #62d1df;" target="_blank" :href="wikipediaLink">wikipedia</a></b></i></div>
      </v-card-text>
    </div>
    <v-card-actions style="margin-top: -25px">
      <v-spacer/>
      <template v-for="item in pluginsSelectedInfoExtraGuiComponents">
        <component :is="item"></component>
      </template>
    </v-card-actions>
    <v-dialog v-model="showShareLinkDialog" width="500px" lazy absolute>
      <v-card style="height: 180px" class="secondary white--text">
        <v-card-title primary-title>
          <div>
            <h3 class="headline mb-0">Share link</h3>
          </div>
        </v-card-title>
        <v-card-text style="width:100%;">
          <v-layout row wrap style="width: 100%">
            <v-text-field id="link_inputid" v-model="shareLink" label="Link" solo readonly></v-text-field>
            <v-btn @click.native.stop="copyLink">Copy</v-btn>
          </v-layout>
        </v-card-text>
      </v-card>
    </v-dialog>
    <div v-if="$store.state.showSelectedInfoButtons" style="position: absolute; right: 0px; bottom: -50px;">
      <v-btn v-if="!showPointToButton" fab dark small color="transparent" @click.native="showShareLinkDialog = !showShareLinkDialog">
        <v-icon>link</v-icon>
      </v-btn>
      <v-btn v-if="showPointToButton" fab dark small color="transparent" v-on:click.native="lockToSelection()">
        <img src="/static/images/svg/ui/point_to.svg" height="40px" style="min-height: 40px"></img>
      </v-btn>
      <v-btn v-if="!showPointToButton" fab dark small color="transparent" @mousedown="zoomOutButtonClicked()">
        <img :class="{bt_disabled: !zoomOutButtonEnabled}" src="/static/images/svg/ui/remove_circle_outline.svg" height="40px" style="min-height: 40px"></img>
      </v-btn>
      <v-btn v-if="!showPointToButton" fab dark small color="transparent" @mousedown="zoomInButtonClicked()">
        <img :class="{bt_disabled: !zoomInButtonEnabled}" src="/static/images/svg/ui/add_circle_outline.svg" height="40px" style="min-height: 40px"></img>
      </v-btn>
    </div>
    <v-snackbar bottom left :timeout="2000" v-model="copied" color="secondary" >
      Link copied
    </v-snackbar>
  </v-card>
</template>

<script>

import Moment from 'moment'
import swh from '@/assets/sw_helpers.js'
import { i18n } from '../plugins/i18n.js'
import langs from '../plugins/langs.js'

var language = langs.language()

export default {
  data: function () {
    return {
      showMinorNames: false,
      wikipediaData: undefined,
      shareLink: undefined,
      showShareLinkDialog: false,
      copied: false
    }
  },
  computed: {
    selectedObject: function () {
      return this.$store.state.selectedObject
    },
    stelSelectionId: function () {
      return this.$store.state.stel && this.$store.state.stel.selection ? this.$store.state.stel.selection : undefined
    },
    title: function () {
      return this.selectedObject ? swh.nameForSkySource(this.selectedObject) : 'Selection'
    },
    otherNames: function () {
      return this.selectedObject ? swh.sortedNamesForSkySource(this.selectedObject) : undefined
    },
    wikipediaSummary: function () {
      if (!this.wikipediaData) return ''
      let page = this.wikipediaData.query.pages[Object.keys(this.wikipediaData.query.pages)[0]]
      if (!page || !page.extract) return ''
      return page.extract.replace(/<p>/g, '').replace(/<\/p>/g, '') + '<span class="grey--text caption" style="margin-left:auto; margin-right:0;"><i>&nbsp; more on <b><a style="color: #62d1df;" target="_blank" href="' + this.wikipediaLink + '">wikipedia</a></b></i></span>'
    },
    wikipediaLink: function () {
      let page = this.wikipediaData.query.pages[Object.keys(this.wikipediaData.query.pages)[0]]
      if (!page || !page.extract) return ''
      return `https://${language}.wikipedia.org/wiki/${page.title}`
    },
    type: function () {
      if (!this.selectedObject) return i18n.t('ui.selected_object_info.unknown')
      let morpho = ''
      if (this.selectedObject.model_data && this.selectedObject.model_data.morpho) {
        morpho = swh.nameForGalaxyMorpho(this.selectedObject.model_data.morpho)
        if (morpho) {
          morpho = morpho + ' '
        }
      }
      if (language === 'en') {
        return `${morpho} ${swh.nameForSkySourceType(this.selectedObject.types[0])}`
      } else if (language === 'pl') {
        if (morpho) {
          return `${morpho} (${swh.nameForSkySourceType(this.selectedObject.types[0])})`
        } else {
          return `${morpho} ${swh.nameForSkySourceType(this.selectedObject.types[0])}`
        }
      }
    },
    icon: function () {
      return swh.iconForSkySource(this.selectedObject)
    },
    items: function () {
      let obj = this.$stel.core.selection
      if (!obj) return []

      let ret = []

      let addAttr = (key, attr, format) => {
        let v = obj.getInfo(attr)
        if (v && !isNaN(v)) {
          ret.push({
            key: key,
            value: format ? format(v) : v.toString()
          })
        }
      }

      addAttr('Magnitude', 'vmag', this.formatMagnitude)
      addAttr('Distance', 'distance', this.formatDistance)
      if (this.selectedObject.model_data) {
        if (this.selectedObject.model_data.radius) {
          ret.push({
            key: 'Radius',
            value: this.selectedObject.model_data.radius.toString() + ' Km'
          })
        }
        if (this.selectedObject.model_data.spect_t) {
          ret.push({
            key: 'Spectral Type',
            value: this.selectedObject.model_data.spect_t
          })
        }
        if (this.selectedObject.model_data.dimx) {
          let dimy = this.selectedObject.model_data.dimy ? this.selectedObject.model_data.dimy : this.selectedObject.model_data.dimx
          ret.push({
            key: 'Size',
            value: this.selectedObject.model_data.dimx.toString() + "' x " + dimy.toString() + "'"
          })
        }
      }
      let formatInt = function (num, padLen) {
        let pad = new Array(1 + padLen).join('0')
        return (pad + num).slice(-pad.length)
      }
      let that = this
      const formatRA = function (a) {
        let raf = that.$stel.a2tf(a, 1)
        return '<div class="radecVal">' + formatInt(raf.hours, 2) + '<span class="radecUnit">h</span>&nbsp;</div><div class="radecVal">' + formatInt(raf.minutes, 2) + '<span class="radecUnit">m</span></div><div class="radecVal">' + formatInt(raf.seconds, 2) + '.' + raf.fraction + '<span class="radecUnit">s</span></div>'
      }
      const formatAz = function (a) {
        let raf = that.$stel.a2af(a, 1)
        return '<div class="radecVal">' + formatInt(raf.degrees < 0 ? raf.degrees + 180 : raf.degrees, 3) + '<span class="radecUnit">°</span></div><div class="radecVal">' + formatInt(raf.arcminutes, 2) + '<span class="radecUnit">\'</span></div><div class="radecVal">' + formatInt(raf.arcseconds, 2) + '.' + raf.fraction + '<span class="radecUnit">"</span></div>'
      }
      const formatDec = function (a) {
        let raf = that.$stel.a2af(a, 1)
        return '<div class="radecVal">' + raf.sign + formatInt(raf.degrees, 2) + '<span class="radecUnit">°</span></div><div class="radecVal">' + formatInt(raf.arcminutes, 2) + '<span class="radecUnit">\'</span></div><div class="radecVal">' + formatInt(raf.arcseconds, 2) + '.' + raf.fraction + '<span class="radecUnit">"</span></div>'
      }
      let posCIRS = this.$stel.convertFrame(this.$stel.core.observer, 'ICRF', 'JNOW', obj.getInfo('radec'))
      let radecCIRS = this.$stel.c2s(posCIRS)
      let raCIRS = this.$stel.anp(radecCIRS[0])
      let decCIRS = this.$stel.anpm(radecCIRS[1])
      ret.push({
        key: 'Ra/Dec',
        value: formatRA(raCIRS) + '&nbsp;&nbsp;&nbsp;' + formatDec(decCIRS)
      })
      let azalt = this.$stel.c2s(this.$stel.convertFrame(this.$stel.core.observer, 'ICRF', 'OBSERVED', obj.getInfo('radec')))
      let az = this.$stel.anp(azalt[0])
      let alt = this.$stel.anpm(azalt[1])
      ret.push({
        key: 'Az/Alt',
        value: formatAz(az) + '&nbsp;&nbsp;&nbsp;' + formatDec(alt)
      })
      addAttr('Phase', 'phase', this.formatPhase)
      let vis = obj.computeVisibility()
      let str = ''
      if (vis.length === 0) {
        str = 'Not visible tonight'
      } else if (vis[0].rise === null) {
        str = 'Always visible tonight'
      } else {
        str = 'Rise: ' + this.formatTime(vis[0].rise) + '&nbsp;&nbsp;&nbsp; Set: ' + this.formatTime(vis[0].set)
      }
      ret.push({
        key: 'Visibility',
        value: str
      })
      return ret
    },
    showPointToButton: function () {
      if (!this.$store.state.stel.lock) return true
      if (this.$store.state.stel.lock !== this.$store.state.stel.selection) return true
      return false
    },
    zoomInButtonEnabled: function () {
      if (!this.$store.state.stel.lock || !this.selectedObject) return false
      return true
    },
    zoomOutButtonEnabled: function () {
      if (!this.$store.state.stel.lock || !this.selectedObject) return false
      return true
    },
    extraButtons: function () {
      return swh.selectedObjectExtraButtons
    },
    pluginsSelectedInfoExtraGuiComponents: function () {
      let res = []
      for (let i in this.$stellariumWebPlugins()) {
        let plugin = this.$stellariumWebPlugins()[i]
        if (plugin.selectedInfoExtraGuiComponents) {
          res = res.concat(plugin.selectedInfoExtraGuiComponents)
        }
      }
      return res
    }
  },
  watch: {
    selectedObject: function (s) {
      this.showMinorNames = false
      this.wikipediaData = undefined
      if (!s) return
      var that = this
      swh.getSkySourceSummaryFromWikipedia(s).then(data => {
        that.wikipediaData = data
      }, reason => { })
    },
    stelSelectionId: function (s) {
      if (!this.$stel.core.selection) {
        this.$store.commit('setSelectedObject', 0)
        return
      }
      swh.sweObj2SkySource(this.$stel.core.selection).then(res => {
        this.$store.commit('setSelectedObject', res)
      }, err => {
        console.log("Couldn't find info for object " + s + ':' + err)
        this.$store.commit('setSelectedObject', 0)
      })
    },
    showShareLinkDialog: function (b) {
      this.shareLink = swh.getShareLink(this)
    }
  },
  methods: {
    formatPhase: function (v) {
      return (v * 100).toFixed(0) + '%'
    },
    formatMagnitude: function (v) {
      if (!v) {
        return i18n.t('ui.selected_object_info.unknown')
      }
      return v.toFixed(2)
    },
    formatDistance: function (d) {
      // d is in AU
      if (!d) {
        return 'NAN'
      }
      let ly = d * swh.astroConstants.ERFA_AULT / swh.astroConstants.ERFA_DAYSEC / swh.astroConstants.ERFA_DJY
      if (ly >= 0.1) {
        return ly.toFixed(2) + '<span class="radecUnit"> ' + i18n.t('ui.selected_object_info.light_years') + '</span>'
      }
      if (d >= 0.1) {
        return d.toFixed(2) + '<span class="radecUnit"> ' + i18n.t('ui.selected_object_info.au') + '</span>'
      }
      let meter = d * swh.astroConstants.ERFA_DAU
      if (meter >= 1000) {
        return (meter / 1000).toFixed(2) + '<span class="radecUnit"> km</span>'
      }
      return meter.toFixed(2) + '<span class="radecUnit"> m</span>'
    },
    formatTime: function (jdm) {
      var d = new Date()
      d.setMJD(jdm)
      let utc = new Moment(d)
      utc.utcOffset(this.$store.state.stel.utcoffset)
      return utc.format('HH:mm')
    },
    unselect: function () {
      this.$stel.core.selection = 0
    },
    lockToSelection: function () {
      if (this.$stel.core.selection) {
        this.$stel.pointAndLock(this.$stel.core.selection, 0.5)
      }
    },
    zoomInButtonClicked: function () {
      let currentFov = this.$store.state.stel.fov * 180 / Math.PI
      this.$stel.zoomTo(currentFov * 0.3 * Math.PI / 180, 0.4)
      let that = this
      this.zoomTimeout = setTimeout(_ => { that.zoomInButtonClicked() }, 300)
    },
    zoomOutButtonClicked: function () {
      let currentFov = this.$store.state.stel.fov * 180 / Math.PI
      this.$stel.zoomTo(currentFov * 3 * Math.PI / 180, 0.6)
      let that = this
      this.zoomTimeout = setTimeout(_ => { that.zoomOutButtonClicked() }, 200)
    },
    stopZoom: function () {
      if (this.zoomTimeout) {
        clearTimeout(this.zoomTimeout)
        this.zoomTimeout = undefined
      }
    },
    extraButtonClicked: function (btn) {
      btn.callback()
    },
    copyLink: function () {
      const input = document.querySelector('#link_inputid')
      input.focus()
      input.select()
      this.copied = document.execCommand('copy')
      window.getSelection().removeAllRanges()
      this.showShareLinkDialog = false
    }
  },
  mounted: function () {
    let that = this
    window.addEventListener('mouseup', function (event) {
      that.stopZoom()
    })
  }
}
</script>

<style>
.bt_disabled {
  filter: opacity(0.2);
}

.radecVal {
  display: inline-block;
  font-family: monospace;
  padding-right: 2px;
  font-size: 13px;
  font-weight: bold;
}

.radecUnit {
  color: #dddddd;
  font-weight: normal
}
</style>
