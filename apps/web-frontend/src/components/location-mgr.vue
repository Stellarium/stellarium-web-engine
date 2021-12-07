// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <div>
    <v-row justify="space-around">
      <v-col cols="4" v-if="doShowMyLocation">
        <v-list two-line subheader>
          <v-subheader>{{ $t('My Locations') }}</v-subheader>
          <v-list-item href="javascript:;" v-for="item in knownLocations" v-bind:key="item.id" @click.native.stop="selectKnownLocation(item)" :style="(item && knownLocationMode && selectedKnownLocation && item.id === selectedKnownLocation.id) ? 'background-color: #455a64' : ''">
            <v-list-item-icon>
              <v-icon>mdi-map-marker</v-icon>
            </v-list-item-icon>
            <v-list-item-content>
              <v-list-item-title>{{ item.short_name }}</v-list-item-title>
              <v-list-item-subtitle>{{ item.country }}</v-list-item-subtitle>
            </v-list-item-content>
          </v-list-item>
        </v-list>
      </v-col>
      <v-col cols="doShowMyLocation ? 8 : 12" >
        <v-card class="blue-grey darken-2 white--text">
          <v-card-title primary-title>
            <v-container fluid>
              <v-row>
                <v-col>
                  <div>
                    <div class="text-h5" style="overflow: hidden; white-space: nowrap; text-overflow: ellipsis;">{{ locationForDetail ? locationForDetail.short_name + ', ' + locationForDetail.country :  '-' }}</div>
                    <v-btn @click.native.stop="useLocation()" style="position: absolute; right: 20px"><v-icon>mdi-chevron-right</v-icon> {{ $t('Use this location') }}</v-btn>
                    <div class="grey--text text-subtitle-2" v-if="locationForDetail.street_address">{{ locationForDetail ? (locationForDetail.street_address ? locationForDetail.street_address : $t('Unknown Address')) : '-' }}</div>
                    <div class="grey--text text-subtitle-2">{{ locationForDetail ? locationForDetail.lat.toFixed(5) + ' ' + locationForDetail.lng.toFixed(5) : '-' }}</div>
                  </div>
                </v-col>
              </v-row>
            </v-container>
          </v-card-title>
          <div style="height: 375px">
              <v-btn light fab class="mx-0 pa-0" @click.native.stop="centerOnRealPosition()" style="position: absolute; z-index: 10000; bottom: 16px; right: 12px;">
                <v-icon>mdi-crosshairs-gps</v-icon>
              </v-btn>
            <l-map class="black--text" ref="myMap" :center="mapCenter" :zoom="10" style="width: 100%; height: 375px;" :options="{zoomControl: false}">
              <l-control-zoom position="topright"></l-control-zoom>
              <l-tile-layer :url="url" attribution='&copy; <a target="_blank" rel="noopener" href="http://osm.org/copyright">OpenStreetMap</a> contributors'></l-tile-layer>
              <l-marker :key="loc.id"
                  v-for="loc in knownLocations"
                  :lat-lng="[ loc.lat, loc.lng ]"
                  :clickable="true"
                  :opacity="(!pickLocationMode && selectedKnownLocation && selectedKnownLocation === loc ? 1.0 : 0.25)"
                  @click="selectKnownLocation(loc)"
                  :draggable="!pickLocationMode && selectedKnownLocation && selectedKnownLocation === loc" @dragend="dragEnd"
                ></l-marker>
              <l-circle v-if="startLocation"
                :lat-lng="[ startLocation.lat, startLocation.lng ]"
                :radius="startLocation.accuracy"
                :options="{
                  strokeColor: '#0000FF',
                  strokeOpacity: 0.5,
                  strokeWeight: 1,
                  fillColor: '#0000FF',
                  fillOpacity: 0.08}"></l-circle>
              <l-marker v-if="pickLocationMode && pickLocation" :lat-lng="[ pickLocation.lat, pickLocation.lng ]"
                :draggable="true" @dragend="dragEnd"><l-tooltip><div class="black--text">Drag to adjust</div></l-tooltip></l-marker>
            </l-map>
          </div>
        </v-card>
      </v-col>
    </v-row>
  </div>
</template>

<script>
import swh from '@/assets/sw_helpers.js'
import { LMap, LTileLayer, LMarker, LCircle, LTooltip, LControlZoom } from 'vue2-leaflet'
import { L } from 'leaflet-control-geocoder/dist/Control.Geocoder.js'

export default {
  data: function () {
    return {
      mode: 'pick',
      pickLocation: undefined,
      selectedKnownLocation: undefined,
      mapCenter: [43.6, 1.4333],
      url: 'https://{s}.tile.osm.org/{z}/{x}/{y}.png'
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
    const that = this
    this.setPickLocation(this.startLocation)
    this.$nextTick(() => {
      const map = this.$refs.myMap.mapObject
      map._onResize()

      var geocoder = new L.Control.Geocoder({
        defaultMarkGeocode: false,
        position: 'topleft',
        collapsed: false
      }).on('markgeocode', function (e) {
        var pos = { lat: e.geocode.center.lat, lng: e.geocode.center.lng }
        that.mapCenter = [pos.lat, pos.lng]
        pos.accuracy = 100
        const ll = that.$t('Lat {0}° Lon {1}°', [pos.lat.toFixed(3), pos.lng.toFixed(3)])
        var loc = {
          short_name: pos.accuracy > 500 ? that.$t('Near {0}', [ll]) : ll,
          country: that.$t('Unknown'),
          lng: pos.lng,
          lat: pos.lat,
          alt: pos.alt ? pos.alt : 0,
          accuracy: pos.accuracy,
          street_address: ''
        }
        const res = e.geocode.properties
        const city = res.address.city ? res.address.city : (res.address.village ? res.address.village : res.name)
        loc.short_name = pos.accuracy > 500 ? that.$t('Near {0}', [city]) : city
        loc.country = res.address.country
        if (pos.accuracy < 50) {
          loc.street_address = res.address.road ? res.address.road : res.display_name
        }
        that.pickLocation = loc
        that.setPickLocationMode()
        geocoder.setQuery('')
      })
      geocoder.addTo(map)
    })
  },
  methods: {
    selectKnownLocation: function (loc) {
      this.selectedKnownLocation = loc
      this.setKnownLocationMode()
      this.mapCenter = [loc.lat, loc.lng]
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
    setPickLocation: function (loc) {
      if (loc.accuracy < 100) {
        for (const l of this.knownLocations) {
          const d = swh.getDistanceFromLatLonInM(l.lat, l.lng, loc.lat, loc.lng)
          if (d < 100) {
            this.selectKnownLocation(l)
            return
          }
        }
      }
      var pos = { lat: loc.lat, lng: loc.lng }
      this.mapCenter = [pos.lat, pos.lng]
      this.pickLocation = loc
      this.setPickLocationMode()
    },
    // Called when the user clicks on the small cross button
    centerOnRealPosition: function () {
      this.setPickLocation(this.realLocation)
    },
    dragEnd: function (event) {
      var that = this
      var pos = { lat: event.target._latlng.lat, lng: event.target._latlng.lng, accuracy: 0 }
      swh.geoCodePosition(pos, that).then((p) => { that.pickLocation = p; that.setPickLocationMode() })
    }
  },
  components: { LMap, LTileLayer, LMarker, LCircle, LTooltip, LControlZoom }
}
</script>

<style>
.leaflet-control-geocoder-form input {
  caret-color:#000 !important;
  color: #000 !important;
}
</style>
