// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <div style="background-color: #424242">
    <v-layout row justify-space-around>
      <v-flex xs4 v-if="doShowMyLocation">
        <v-list two-line avatar subheader>
          <v-subheader>My Locations</v-subheader>
          <v-list-tile avatar href="javascript:;" v-for="item in knownLocations" v-bind:key="item.id" @click.native.stop="selectKnownLocation(item)" :style="(item && knownLocationMode && selectedKnownLocation && item.id === selectedKnownLocation.id) ? 'background-color: #455a64' : ''">
            <v-list-tile-avatar>
              <v-icon>location_on</v-icon>
            </v-list-tile-avatar>
            <v-list-tile-content>
              <v-list-tile-title>{{ item.shortName }}</v-list-tile-title>
              <v-list-tile-sub-title>{{ item.country }}</v-list-tile-sub-title>
            </v-list-tile-content>
          </v-list-tile>
        </v-list>
      </v-flex>
      <v-flex :xs8="doShowMyLocation" :xs12="!doShowMyLocation" >
        <v-card class="blue-grey darken-2 white--text">
          <v-card-title primary-title>
            <v-container fluid>
              <v-layout>
                <v-flex xs12>
                  <div>
                    <div class="headline" style="overflow: hidden; white-space: nowrap; text-overflow: ellipsis;">{{ locationForDetail ? locationForDetail.shortName + ', ' + locationForDetail.country :  '-' }}</div>
                    <v-btn @click.native.stop="useLocation()"  style="position: absolute; right: 20px"><v-icon>keyboard_arrow_right</v-icon> Use this location</v-btn>
                    <div class="grey--text">{{ locationForDetail ? (locationForDetail.streetAddress ? locationForDetail.streetAddress : 'Unknown Address') : '-' }}</div>
                    <span class="grey--text">{{ locationForDetail ? locationForDetail.lat.toFixed(5) + ' ' + locationForDetail.lng.toFixed(5) : '-' }}</span>
                  </div>
                </v-flex>
              </v-layout>
            </v-container>
          </v-card-title>
          <v-card-media height="375px">
            <v-toolbar class="white" floating dense style="position: absolute; z-index: 10000; top: 16px;">
              <div class="gmapsearch">
                <div class="input-group input-group--prepend-icon input-group--hide-details theme--dark input-group--text-field input-group--single-line">
                  <div class="input-group__input">
                    <i class="material-icons icon input-group__prepend-icon black--text">search</i>
                    <gmap-autocomplete ref="gmapautocplt" @place_changed="setPlace" style="caret-color:#000 !important; color: #000 !important;"></gmap-autocomplete>
                  </div>
                  <div class="input-group__details">
                    <div class="input-group__messages"></div>
                 </div>
               </div>
              </div>
              <v-spacer></v-spacer>
              <v-btn icon class="black--text" @click.native.stop="centerOnRealPosition()">
                <v-icon>my_location</v-icon>
              </v-btn>
            </v-toolbar>
            <gmap-map ref="gmapmap" :center="mapCenter" :zoom="12" :options="{mapTypeControl: false, streetViewControl: false}" style="width: 100%; height: 100%;" v-observe-visibility="visibilityChanged">
              <gmap-marker :key="loc.id"
                  v-for="loc in knownLocations"
                  :position="{ lng: loc.lng, lat: loc.lat }"
                  :clickable="true"
                  :opacity="(!pickLocationMode && selectedKnownLocation && selectedKnownLocation === loc ? 1.0 : 0.25)"
                  @click="selectKnownLocation(loc)"
                  :draggable="!pickLocationMode && selectedKnownLocation && selectedKnownLocation === loc" @dragend="dragEnd"
                ></gmap-marker>
              <gmap-circle v-if="startLocation"
                :center="{ lng: startLocation.lng, lat: startLocation.lat }"
                :radius="startLocation.accuracy"
                :options="{
                  strokeColor: '#0000FF',
                  strokeOpacity: 0.5,
                  strokeWeight: 1,
                  fillColor: '#0000FF',
                  fillOpacity: 0.08}"></gmap-circle>
              <gmap-marker v-if="pickLocationMode && pickLocation" :position="{ lng: pickLocation.lng, lat: pickLocation.lat }"
                :draggable="true" @dragend="dragEnd"><gmap-infoWindow><div class="black--text">Drag to adjust</div></gmap-infoWindow></gmap-marker>

            </gmap-map>
          </v-card-media>
        </v-card>
      </v-flex>
    </v-layout>
  </div>
</template>

<script>
import swh from '@/assets/sw_helpers.js'

export default {
  data: function () {
    return {
      mode: 'pick',
      pickLocation: undefined,
      selectedKnownLocation: undefined,
      mapCenter: {lat: 43.6, lng: 1.4333}
    }
  },
  props: ['showMyLocation', 'knownLocations', 'startLocation', 'realLocation'],
  computed: {
    doShowMyLocation: function () {
      return this.showMyLocation === undefined ? false : this.showMyLocation
    },
    pickLocationMode: function () {
      return this.mode === 'pick'
    },
    knownLocationMode: function () {
      return this.mode === 'known'
    },
    locationForDetail: function () {
      if (this.pickLocationMode && this.pickLocation === undefined) {
        return this.startLocation
      }
      return this.pickLocationMode ? this.pickLocation : this.selectedKnownLocation
    }
  },
  watch: {
    startLocation: function () {
      this.setPickLocation(this.startLocation)
    }
  },
  mounted: function () {
    this.setPickLocation(this.startLocation)
  },
  methods: {
    // Workaround a map refresh bug..
    visibilityChanged: function (v) {
      this.$gmapDefaultResizeBus.$emit('resize')
      this.$refs.gmapautocplt.$el.focus()
    },
    selectKnownLocation: function (loc) {
      this.selectedKnownLocation = loc
      this.setKnownLocationMode()
      this.mapCenter = { 'lat': loc.lat, 'lng': loc.lng }
    },
    useLocation: function () {
      this.$emit('locationSelected', this.locationForDetail)
    },
    setPickLocationMode: function () {
      this.mode = 'pick'
    },
    setKnownLocationMode: function () {
      this.mode = 'known'
    },
    setPlace: function (place) {
      console.log(place)
      var that = this
      var pos = {lat: place.geometry.location.lat(), lng: place.geometry.location.lng()}
      this.mapCenter = pos
      pos.accuracy = 0
      swh.geoCodePosition(pos).then((p) => { that.pickLocation = p })
      this.setPickLocationMode()
    },
    setPickLocation: function (loc) {
      if (loc.accuracy < 100) {
        for (let l of this.knownLocations) {
          let d = swh.getDistanceFromLatLonInM(l.lat, l.lng, loc.lat, loc.lng)
          if (d < 100) {
            this.selectKnownLocation(l)
            return
          }
        }
      }
      var pos = { lat: loc.lat, lng: loc.lng }
      this.mapCenter = pos
      this.pickLocation = loc
      this.setPickLocationMode()
    },
    // Called when the user clicks on the small cross button
    centerOnRealPosition: function () {
      this.setPickLocation(this.realLocation)
    },
    dragEnd: function (newPos) {
      var that = this
      var pos = {lat: newPos.latLng.lat(), lng: newPos.latLng.lng(), accuracy: 0}
      swh.geoCodePosition(pos).then((p) => { that.pickLocation = p; that.setPickLocationMode() })
    }
  }
}
</script>

<style>
</style>
