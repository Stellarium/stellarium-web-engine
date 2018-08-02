// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <div style="height: 100%;">

    <v-toolbar dark dense>
      <v-btn v-if="!edit" icon @click.stop.native="back">
        <v-icon>arrow_back</v-icon>
      </v-btn>
      <v-spacer></v-spacer>
      <v-btn v-if="!edit" icon @click.stop.native="requestModify()"><v-icon>edit</v-icon></v-btn><v-tooltip left><span>Edit this Observation</span></v-tooltip>
      <v-dialog v-if="!edit" v-model="deleteDialog" lazy absolute max-width="400px">
        <v-btn icon slot="activator"><v-icon>delete</v-icon></v-btn><v-tooltip left><span>Delete this Observation</span></v-tooltip>
        <v-card>
          <v-card-title>
            <div class="headline">Permanently delete this observation?</div>
          </v-card-title>
          <v-card-text>This observation will be deleted from the cloud</v-card-text>
          <v-card-actions>
            <v-spacer></v-spacer>
            <v-btn class="green--text darken-1" flat="flat" @click.native="deleteDialog = false">Cancel</v-btn>
            <v-btn class="green--text darken-1" flat="flat" @click.native="deleteDialog = false; deleteObservation()">Delete</v-btn>
          </v-card-actions>
        </v-card>
      </v-dialog>
    </v-toolbar>

    <v-container fluid style="height: 100%">
      <v-list two-line subheader>
        <v-subheader inset>Observation Details</v-subheader>

        <v-list-tile>
          <v-list-tile-avatar>
            <img :src="currentSkySourceIcon" style="height: 24px; margin-top: 12px"/>
          </v-list-tile-avatar>
          <v-list-tile-content>
            <v-list-tile-title>{{ currentSkySourceName }}</v-list-tile-title>
            <v-list-tile-sub-title>{{ currentSkySourceType }}</v-list-tile-sub-title>
          </v-list-tile-content>
          <v-list-tile-action v-if="edit">
          <v-menu :close-on-content-click="false" transition="v-slide-y-transition" v-model="skySourceSearchMenu">
            <v-btn icon ripple slot="activator">
              <v-icon class="grey--text text--lighten-1">edit</v-icon>
            </v-btn>
            <v-card class="secondary" >
              <v-card-text>
                <skysource-search v-model="obsSkySource"/>
              </v-card-text>
            </v-card>
          </v-menu>
          </v-list-tile-action>
        </v-list-tile>

        <v-list-tile>
          <v-list-tile-avatar>
            <img :src="currentObservingSetupIcon" style="height: 24px; margin-top: 12px"/>
          </v-list-tile-avatar>
          <v-list-tile-content>
            <v-list-tile-title>{{ currentObservingSetupTitle }}</v-list-tile-title>
          </v-list-tile-content>
          <v-list-tile-action v-if="edit">
            <v-btn icon ripple @click.stop="equipmentMenu = !equipmentMenu">
              <v-icon class="grey--text text--lighten-1">edit</v-icon>
            </v-btn>
          </v-list-tile-action>
        </v-list-tile>

        <v-list-tile>
          <v-list-tile-avatar>
            <v-icon>event</v-icon>
          </v-list-tile-avatar>
          <v-list-tile-content>
            <v-list-tile-title>{{ addedObsDate }} {{ addedObsTime }}</v-list-tile-title>
          </v-list-tile-content>
          <v-list-tile-action v-if="edit">
            <v-btn icon ripple @click.stop="dateTimeMenu = !dateTimeMenu">
              <v-icon class="grey--text text--lighten-1">edit</v-icon>
            </v-btn>
          </v-list-tile-action>
        </v-list-tile>

        <v-list-tile>
          <v-list-tile-avatar>
            <v-icon>location_on</v-icon>
          </v-list-tile-avatar>
          <v-list-tile-content>
            <v-list-tile-title>{{ currentLocationTitle }}</v-list-tile-title>
            <v-list-tile-sub-title>{{ currentLocationSubtitle }}</v-list-tile-sub-title>
          </v-list-tile-content>
          <v-list-tile-action v-if="edit">
            <v-btn icon ripple @click.stop="locationMenu = !locationMenu">
              <v-icon class="grey--text text--lighten-1">edit</v-icon>
            </v-btn>
          </v-list-tile-action>
        </v-list-tile>

        <v-list-tile>
        <v-list-tile-avatar>
          <v-icon>grade</v-icon>
        </v-list-tile-avatar>
          <v-list-tile-content>
            <v-list-tile-title style="margin-bottom: 5px">
              <div style="position: absolute; margin-left: 100px; margin-top: -3px;">
                <div v-if="edit">
                  <img v-for="i in currentRating" src="/static/images/svg/ui/star_rate.svg" height="30px" @click.stop="setRating(i)" style="cursor: pointer;"></img
                  ><img v-for="i in 5 - currentRating" src="/static/images/svg/ui/star_border.svg" height="30px" @click.stop="setRating(currentRating + i)" style="cursor: pointer; opacity: 0.1;"></img>
                </div>
                <div v-else style="position: absolute;">
                  <img v-for="i in currentRating" src="/static/images/svg/ui/star_rate.svg" height="30px"></img
                  ><img v-for="i in 5 - currentRating" src="/static/images/svg/ui/star_border.svg" height="30px" style="opacity: 0.1;"></img>
                </div>
              </div>
              Rating
            </v-list-tile-title>
            <v-list-tile-title>
              <div style="position: absolute; margin-left: 100px; margin-top: -3px;">
                <div v-if="edit">
                  <img v-for="i in currentDifficulty" src="/static/images/svg/ui/star_rate.svg" height="30px" @click.stop="setDifficulty(i)" style="cursor: pointer;"></img
                  ><img v-for="i in 5 - currentDifficulty" src="/static/images/svg/ui/star_border.svg" height="30px" @click.stop="setDifficulty(currentDifficulty + i)" style="cursor: pointer; opacity: 0.1;"></img>
                </div>
                <div v-else>
                  <img v-for="i in currentDifficulty" src="/static/images/svg/ui/star_rate.svg" height="30px"></img
                  ><img v-for="i in 5 - currentDifficulty" src="/static/images/svg/ui/star_border.svg" height="30px" style="opacity: 0.1;"></img>
                </div>
              </div>
              Difficulty
            </v-list-tile-title>
          </v-list-tile-content>
        </v-list-tile>

      </v-list>

      <v-card>
        <v-container>
          <v-divider></v-divider>
          <v-text-field name="comment" label="Comment" v-model="currentComment" full-width multi-line :disabled="!edit"></v-text-field>
        </v-container>
      </v-card>

      <v-layout v-if="edit" justify-space-around>
        <v-btn flat dark @click.native="back()">Cancel</v-btn>
        <v-btn flat dark @click.native="save()">Save</v-btn>
      </v-layout>
    </v-container>

    <v-dialog lazy max-width="800" v-model="locationMenu">
      <v-card v-if="locationMenu" color="secondary" flat>
        <v-container>
          <location-mgr showMyLocation="true" :knownLocations="$store.state.plugins.observing.noctuaSky.locations" v-on:locationSelected="setLocation"></location-mgr>
        </v-container>
      </v-card>
    </v-dialog>

    <v-dialog lazy max-width="800" scrollable v-model="equipmentMenu">
      <observing-setup-details v-if="equipmentMenu" :instance="observation.observingSetup" @change="setObservingSetup"></observing-setup-details>
    </v-dialog>

    <v-dialog lazy max-width="600" v-model="dateTimeMenu">
      <v-card v-if="dateTimeMenu" color="secondary" flat>
      <v-container>
      <v-layout>
        <v-date-picker dark v-model="addedObsDate" scrollable>
        </v-date-picker>
        <v-time-picker dark v-model="addedObsTime" scrollable format="24hr">
        </v-time-picker>
      </v-layout>
      </v-container>
      </v-card>
    </v-dialog>
  </div>
</template>

<script>

import SkysourceSearch from '@/components/skysource-search.vue'
import LocationMgr from '@/components/location-mgr.vue'
import ObservingSetupDetails from './observing-setup-details.vue'
import Moment from 'moment'
import { swh } from '@/assets/sw_helpers.js'
import { nsh } from '../ns_helpers.js'

export default {
  data: function () {
    return {
      modify: false,
      observationBackup: nsh.getDefaultObservation(this),
      observation: nsh.getDefaultObservation(this),
      skySourceSearchMenu: false,
      locationMenu: false,
      dateTimeMenu: false,
      equipmentMenu: false,
      deleteDialog: false,
      footprintShape: undefined
    }
  },
  props: ['value', 'create'],
  methods: {
    setLocation: function (loc) {
      if (!loc.id) {
        var that = this
        console.log('Location is new, needs to be saved...')
        this.$store.dispatch('observing/addLocation', loc).then((newLoc) => { that.observation.location = newLoc.id; that.locationMenu = false })
      } else {
        this.observation.location = loc.id
        this.locationMenu = false
      }
      this.setViewSettingsForObservation()
    },
    setObservingSetup: function (obsSetup) {
      this.observation.observingSetup = obsSetup
      this.equipmentMenu = false
      this.setViewSettingsForObservation()
    },
    save: function () {
      var that = this
      if (!this.observation.locationPublicData) {
        this.observation.locationPublicData = nsh.locationForId(this, this.observation.location).publicData
      }
      this.$store.dispatch('observing/addObservation', this.observation).then(function (res) {
        if (that.modify) {
          that.modify = false
        } else {
          that.back()
        }
      }, function (error) { alert(error) })
    },
    back: function () {
      if (this.modify) {
        // We want to cancel the edit
        this.observation = JSON.parse(JSON.stringify(this.observationBackup))
        this.modify = false
        return
      }
      if (this.footprintShape) this.footprintShape.destroy()
      this.footprintShape = undefined
      this.$emit('back')
    },
    deleteObservation: function () {
      var that = this
      console.log('Delete ' + this.observation.id)
      this.$store.dispatch('observing/deleteObservations', this.observation.id).then(function (res) { that.back() })
    },
    setRating: function (r) {
      this.observation.rating = r
    },
    setDifficulty: function (r) {
      this.observation.difficulty = r
    },
    requestModify: function () {
      this.observationBackup = JSON.parse(JSON.stringify(this.observation))
      this.modify = true
    },
    setViewSettingsForObservation: function () {
      if (!this.observation) {
        return
      }
      let obs = this.observation
      this.$stel.core.observer.utc = obs.mjd
      let loc
      if (obs.location) {
        loc = nsh.locationForId(this, obs.location)
      } else {
        loc = this.$store.state.currentLocation
      }
      this.$stel.core.observer.latitude = loc.lat * Math.PI / 180.0
      this.$stel.core.observer.longitude = loc.lng * Math.PI / 180.0
      this.$stel.core.observer.elevation = loc.alt
      let azalt
      if (!obs.target) {
        azalt = this.$stel.core.observer.azalt
      } else if (obs.target.nsid) {
        // First check if the object already exists in SWE
        let ss = this.$stel.getObjByNSID(obs.target.nsid)
        if (!ss) {
          ss = this.$stel.createObj(obs.target.model, obs.target)
        }
        ss.update()
        azalt = ss.azalt
      }
      let expandedObservingSetup = nsh.fullEquipmentInstanceState(obs.observingSetup)
      let fp = expandedObservingSetup.footprint({ra: 0, de: 0})
      let fov = fp[2] * Math.PI / 180.0
      this.$stel.core.lock = undefined
      this.$stel.core.lookat(azalt, 1.0, fov * 1.5)
      var shapeParams = {
        pos: [-azalt[0], -azalt[1], -azalt[2]],
        frame: this.$stel.FRAME_OBSERVED,
        size: [2.0 * Math.PI - fov, 2.0 * Math.PI - fov],
        color: [0.1, 0.1, 0.1, 0.8],
        border_color: [1, 0.1, 0.1, 1]
      }
      if (this.footprintShape) this.footprintShape.destroy()
      this.footprintShape = this.$observingLayer.add('circle', shapeParams)
    }
  },
  watch: {
    obsSkySource: function () {
      this.skySourceSearchMenu = false
    },
    value: function (newValue) {
      if (newValue !== undefined) {
        this.observation = newValue
      } else {
        this.observation = nsh.getDefaultObservation(this)
        this.observationBackup = nsh.getDefaultObservation(this)
      }
    },
    observation: function (obs) {
      this.setViewSettingsForObservation()
    }
  },
  computed: {
    edit: function () {
      return this.create || this.modify
    },
    obsSkySource: {
      get: function () {
        return this.observation.target
      },
      set: function (v) {
        this.observation.target = v
        this.setViewSettingsForObservation()
      }
    },
    currentSkySourceIcon: function () {
      return nsh.iconForObservation(this.observation)
    },
    currentSkySourceName: function () {
      if (this.observation.target === undefined) {
        return 'Target'
      }
      return swh.nameForSkySource(this.observation.target)
    },
    currentSkySourceType: function () {
      return this.observation.target ? swh.nameForSkySourceType(this.observation.target.types[0]) : 'Please select a source'
    },
    currentLocationTitle: function () {
      if (this.observation.location) {
        let loc = nsh.locationForId(this, this.observation.location)
        return loc ? loc.shortName : ''
      } else {
        return 'Location'
      }
    },
    currentLocationSubtitle: function () {
      if (this.observation.location) {
        let loc = nsh.locationForId(this, this.observation.location)
        return loc ? loc.country : ''
      } else {
        return 'Please select a location'
      }
    },
    currentRating: function () {
      return this.observation.rating
    },
    currentDifficulty: function () {
      return this.observation.difficulty
    },
    currentComment: {
      get: function () {
        return this.observation.comment
      },
      set: function (v) {
        this.observation.comment = v
      }
    },
    currentObservingSetupTitle: function () {
      return nsh.titleForObservingSetup(this.observation.observingSetup)
    },
    currentObservingSetupIcon: function () {
      return nsh.iconForObservingSetup(this.observation.observingSetup)
    },
    addedObsDate: {
      get: function () {
        var d = new Date()
        d.setMJD(this.observation.mjd)
        return Moment(d).format('YYYY-MM-DD')
      },
      set: function (v) {
        let m = Moment(v + ' ' + this.addedObsTime + ':00')
        this.observation.mjd = m.toDate().getMJD()
        this.setViewSettingsForObservation()
      }
    },
    addedObsTime: {
      get: function () {
        var d = new Date()
        d.setMJD(this.observation.mjd)
        return Moment(d).format('HH:mm')
      },
      set: function (v) {
        let m = Moment(this.addedObsDate + ' ' + v + ':00')
        this.observation.mjd = m.toDate().getMJD()
        this.setViewSettingsForObservation()
      }
    }
  },
  components: { SkysourceSearch, LocationMgr, ObservingSetupDetails }
}
</script>

<style>
</style>
