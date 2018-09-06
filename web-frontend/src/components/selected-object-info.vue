// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <v-card v-if="selectedObject" transparent style="background: rgba(66, 66, 66, 0.3);">
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
        <v-flex xs4 style="margin-top: -2px; color: #dddddd">Also known as</v-flex> <span class="caption" text-color="white" v-for="(mname, index) in otherNames" v-if="index > 0 && index < 8" :key="mname" style="margin-right: 15px; font-weight: 500">{{ mname }}</span>
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
    </v-card-text>
    <v-card-text>
      <v-layout row wrap style="width: 100%">
        <div v-html="wikipediaSummary"></div>
        <div v-if="wikipediaSummary" class="grey--text caption" style="margin-left:auto; margin-right:0;"><i> read more on <b><a style="color: #62d1df;" target="_blank" :href="wikipediaLink">wikipedia</a></b></i></div>
      </v-layout>
    </v-card-text>
    <div style="position: absolute; right: 20px; bottom: -50px;">
      <v-btn v-for="btn in extraButtons" :key="btn.id" dark color="transparent" @click.native="extraButtonClicked(btn)">
        {{ btn.name }}<v-icon right dark>{{ btn.icon }}</v-icon>
      </v-btn>
      <v-btn v-if="!showPointToButton" fab dark small color="transparent" @click.native="showShareLinkDialog = !showShareLinkDialog">
        <v-icon>link</v-icon>
      </v-btn>
      <v-btn v-if="showPointToButton" fab dark small color="transparent" v-on:click.native="lockToSelection()">
        <img src="/static/images/svg/ui/point_to.svg" height="40px" style="min-height: 40px"></img>
      </v-btn>
      <v-btn v-if="!showPointToButton" fab dark small color="transparent" v-on:click.native="zoomOutButtonClicked()">
        <img :class="{bt_disabled: !zoomOutButtonEnabled}" src="/static/images/svg/ui/remove_circle_outline.svg" height="40px" style="min-height: 40px"></img>
      </v-btn>
      <v-btn v-if="!showPointToButton" fab dark small color="transparent" v-on:click.native="zoomInButtonClicked()">
        <img :class="{bt_disabled: !zoomInButtonEnabled}" src="/static/images/svg/ui/add_circle_outline.svg" height="40px" style="min-height: 40px"></img>
      </v-btn>
    </div>
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
    <v-snackbar bottom left :timeout="2000" v-model="copied" color="secondary" >
      Link copied
    </v-snackbar>
  </v-card>
</template>

<script>

import Moment from 'moment'
import swh from '@/assets/sw_helpers.js'

export default {
  data: function () {
    return {
      showMinorNames: false,
      // Contains the
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
      return page.extract.replace(/<p>/g, '').replace(/<\/p>/g, '')
    },
    wikipediaLink: function () {
      let page = this.wikipediaData.query.pages[Object.keys(this.wikipediaData.query.pages)[0]]
      if (!page || !page.extract) return ''
      return 'https://en.wikipedia.org/wiki/' + page.title
    },
    type: function () {
      if (!this.selectedObject) return 'Unknown'
      let morpho = ''
      if (this.selectedObject.model_data && this.selectedObject.model_data.morpho) {
        morpho = swh.nameForGalaxyMorpho(this.selectedObject.model_data.morpho)
        if (morpho) {
          morpho = morpho + ' '
        }
      }
      return morpho + swh.nameForSkySourceType(this.selectedObject.types[0])
    },
    icon: function () {
      return swh.iconForSkySource(this.selectedObject)
    },
    items: function () {
      let obj = this.$stel.core.selection
      if (!obj) return []

      obj.update(this.$stel.core.observer)
      let ret = []

      let addAttr = (key, attr, format) => {
        if (!obj[attr]) return
        let v = obj[attr]
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
      let posCIRS = this.$stel.convertPosition(this.$stel.core.observer, 'ICRS', 'CIRS', obj.icrs)
      let radecCIRS = this.$stel.c2s(posCIRS)
      let raCIRS = this.$stel.anp(radecCIRS[0])
      let decCIRS = this.$stel.anpm(radecCIRS[1])
      ret.push({
        key: 'Ra/Dec',
        value: formatRA(raCIRS) + '&nbsp;&nbsp;&nbsp;' + formatDec(decCIRS)
      })
      ret.push({
        key: 'Az/Alt',
        value: formatAz(obj.az) + '&nbsp;&nbsp;&nbsp;' + formatDec(obj.alt)
      })
      addAttr('Phase', 'phase', this.formatPhase)
      addAttr('Rise', 'rise', this.formatTime)
      addAttr('Set', 'set', this.formatTime)
      return ret
    },
    showPointToButton: function () {
      if (!this.$store.state.stel.lock) return true
      if (this.$store.state.stel.lock !== this.$store.state.stel.selection) return true
      return false
    },
    zoomInButtonEnabled: function () {
      if (!this.$store.state.stel.lock || !this.selectedObject) return false
      let fovs = swh.fovsForSkySource(this.selectedObject)
      let currentFov = this.$store.state.stel.fov * 180 / Math.PI
      return (fovs[fovs.length - 1] < currentFov - 0.0001)
    },
    zoomOutButtonEnabled: function () {
      if (!this.$store.state.stel.lock || !this.selectedObject) return false
      let fovs = swh.fovsForSkySource(this.selectedObject)
      fovs.unshift(100)
      let currentFov = this.$store.state.stel.fov * 180 / Math.PI
      return (fovs[0] > currentFov + 0.0001)
    },
    extraButtons: function () {
      return swh.selectedObjectExtraButtons
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
        this.$store.commit('setSelectedObject', undefined)
        return
      }
      swh.sweObj2SkySource(this.$stel.core.selection).then(res => {
        this.$store.commit('setSelectedObject', res)
      }, err => {
        console.log("Couldn't find info for object " + s + ':' + err)
        this.$store.commit('setSelectedObject', undefined)
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
        return 'Unknown'
      }
      return v
    },
    formatDistance: function (d) {
      // d is in AU
      if (!d) {
        return 'NAN'
      }
      let ly = d * swh.astroConstants.ERFA_AULT / swh.astroConstants.ERFA_DAYSEC / swh.astroConstants.ERFA_DJY
      if (ly >= 0.1) {
        return ly.toFixed(2) + '<span class="radecUnit"> light years</span>'
      }
      if (d >= 0.1) {
        return d.toFixed(2) + '<span class="radecUnit"> AU</span>'
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
      this.$stel.core.selection = undefined
    },
    lockToSelection: function () {
      if (this.$stel.core.selection) {
        this.$stel.core.lock = this.$stel.core.selection
        this.$stel.core.lock.update()
        this.$stel.core.lookat(this.$stel.core.lock.azalt, 1.0)
      }
    },
    zoomInButtonClicked: function () {
      let fovs = swh.fovsForSkySource(this.selectedObject)
      let currentFov = this.$store.state.stel.fov * 180 / Math.PI
      for (let i in fovs) {
        if (fovs[i] < currentFov - 0.0001) {
          this.$stel.core.lookat(this.$stel.core.lock.azalt, 1.0, fovs[i] * Math.PI / 180)
          return
        }
      }
    },
    zoomOutButtonClicked: function () {
      let fovs = swh.fovsForSkySource(this.selectedObject)
      fovs.unshift(100)
      let currentFov = this.$store.state.stel.fov * 180 / Math.PI
      for (let i in fovs) {
        let f = fovs[fovs.length - i - 1]
        if (f > currentFov + 0.0001) {
          this.$stel.core.lookat(this.$stel.core.lock.azalt, 1.0, f * Math.PI / 180)
          return
        }
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
