// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <div style="height: 100%;">
    <v-progress-circular v-if="isLoading" indeterminate size="50" style="position: absolute; left: 0; right: 0; margin-left: auto; margin-right: auto; margin-top: 45%;"></v-progress-circular>
    <v-toolbar dark dense v-if="!isLoading">
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

    <v-container fluid style="height: 100%" v-if="!isLoading">
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
          <location-mgr showMyLocation="true" :knownLocations="$store.state.noctuaSky.locations" v-on:locationSelected="setLocation" :startLocation="$store.state.currentLocation" :realLocation="$store.state.autoDetectedLocation"></location-mgr>
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
import NoctuaSkyClient from '@/assets/noctuasky-client'
import { swh } from '@/assets/sw_helpers.js'
import _ from 'lodash'

export default {
  data: function () {
    return {
      modify: false,
      create: false,
      isLoading: false,
      observationBackup: undefined,
      observation: {
        target: undefined,
        mjd: this.$store.state.stel.observer.utc,
        location: this.$store.state.currentLocation,
        difficulty: 0,
        rating: 0,
        comment: '',
        observingSetup: {
          'id': 'eyes_observation',
          'state': {}
        }
      },
      skySourceSearchMenu: false,
      locationMenu: false,
      dateTimeMenu: false,
      equipmentMenu: false,
      deleteDialog: false,
      footprintShape: undefined,
      savedViewSettings: []
    }
  },
  props: ['id'],
  methods: {
    setLocation: function (loc) {
      this.observation.location = loc
      this.locationMenu = false
      this.setViewSettingsForObservation()
    },
    setObservingSetup: function (obsSetup) {
      this.observation.observingSetup = obsSetup
      this.equipmentMenu = false
      this.setViewSettingsForObservation()
    },
    save: function () {
      var that = this

      let obsToSave = _.cloneDeep(this.observation)
      // Backend can't deal with compacted form of observingSetup (a simple string)
      obsToSave.observingSetup = obsToSave.observingSetup.id ? obsToSave.observingSetup : {id: obsToSave.observingSetup, state: {}}

      let save2 = function (obs) {
        if (that.modify) {
          // We are editing an existing observation
          console.log('Updating observation ' + obs.id)
          return NoctuaSkyClient.observations.update(obs.id, obs).then(res => {
            that.modify = false
          }, err => {
            console.log('Failed to update observation:')
            console.log(JSON.stringify(err))
          })
        }
        return NoctuaSkyClient.observations.add(obs).then(res => {
          that.back()
        }, err => {
          console.log('Failed to create new observation:')
          console.log(JSON.stringify(err))
        })
      }

      if (obsToSave.location.id) {
        obsToSave.location = obsToSave.location.id
        return save2(obsToSave)
      }
      console.log('Location is new, save it first..')
      return NoctuaSkyClient.locations.add(obsToSave.location).then(res => {
        obsToSave.location = res.id
        return save2(obsToSave)
      })
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
      this.$router.go(-1)
    },
    deleteObservation: function () {
      var that = this
      console.log('Delete ' + this.observation.id)
      NoctuaSkyClient.observations.delete(this.observation.id).then(function (res) { that.back() })
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
        loc = obs.location
      } else {
        loc = this.$store.state.currentLocation
      }
      this.$stel.core.observer.latitude = loc.lat * Math.PI / 180.0
      this.$stel.core.observer.longitude = loc.lng * Math.PI / 180.0
      this.$stel.core.observer.elevation = loc.alt
      let azalt
      if (!obs.target) {
        azalt = this.$stel.core.observer.azalt
      } else {
        // First check if the object already exists in SWE
        let ss = swh.skySource2SweObj(obs.target)
        if (!ss) {
          ss = this.$stel.createObj(obs.target.model, obs.target)
        }
        ss.update()
        azalt = ss.azalt
      }
      let expandedObservingSetup = NoctuaSkyClient.equipments.fullEquipmentInstanceState(obs.observingSetup)
      let fp = expandedObservingSetup.footprint({ra: 0, de: 0})
      let fov = fp[2] * Math.PI / 180.0
      this.$stel.core.lock = 0
      this.$stel.core.lookat(azalt, 1.0, fov * 1.5)
      var shapeParams = {
        pos: [-azalt[0], -azalt[1], -azalt[2]],
        frame: this.$stel.FRAME_OBSERVED,
        size: [2.0 * Math.PI - fov, 2.0 * Math.PI - fov],
        color: [0.1, 0.1, 0.1, 0.8],
        border_color: [0.6, 0.1, 0.1, 1]
      }
      if (this.footprintShape) this.footprintShape.destroy()
      this.footprintShape = this.$observingLayer.add('circle', shapeParams)
    },
    saveViewSettings: function () {
      this.savedViewSettings['utc'] = this.$store.state.stel.observer.utc
      this.savedViewSettings['loc'] = this.$store.state.currentLocation
      this.savedViewSettings['azalt'] = this.$store.state.stel.observer.azalt
      this.savedViewSettings['fov'] = this.$store.state.stel.fov
      this.savedViewSettings['lock'] = this.$stel.core.lock
    },
    restoreViewSettings: function () {
      if (!this.savedViewSettings) return
      if (this.savedViewSettings.utc) this.$stel.core.observer.utc = this.savedViewSettings.utc
      if (this.savedViewSettings.loc) {
        let loc = this.savedViewSettings.loc
        this.$stel.core.observer.latitude = loc.lat * Math.PI / 180.0
        this.$stel.core.observer.longitude = loc.lng * Math.PI / 180.0
        this.$stel.core.observer.elevation = loc.alt
        this.$store.commit('setCurrentLocation', loc)
      }
      if (this.savedViewSettings.lock) this.$stel.core.lock = this.savedViewSettings.lock
      if (this.savedViewSettings.azalt && this.savedViewSettings.fov) this.$stel.core.lookat(this.savedViewSettings.azalt, 1.0, this.savedViewSettings.fov)
    },
    getDefaultObservation: function () {
      let res = {
        target: undefined,
        mjd: this.$store.state.stel.observer.utc,
        location: this.$store.state.currentLocation,
        difficulty: 0,
        rating: 0,
        comment: '',
        observingSetup: {
          'id': 'eyes_observation',
          'state': {}
        }
      }
      if (this.$store.state.selectedObject) {
        res.target = this.$store.state.selectedObject
      }
      return res
    },
    resetObs: function (obs) {
      if (obs.location && !obs.location.lat) {
        obs.location = NoctuaSkyClient.locations.get(obs.location)
      }
      this.observation = obs
      this.observationBackup = JSON.parse(JSON.stringify(this.observation))
      this.isLoading = false
    },
    refreshObs: function (v) {
      let that = this
      if (!v) return
      let obs
      if (v === 'create') {
        console.log('Create observation')
        // Set this to default values
        obs = this.getDefaultObservation()
        let lastModified = NoctuaSkyClient.observations.lastModified()
        if (lastModified) {
          obs.mjd = lastModified.mjd
          obs.location = NoctuaSkyClient.locations.get(lastModified.location)
          obs.observingSetup = lastModified.observingSetup
        }
        this.create = true
      } else if (v === 'create_from_selection') {
        obs = this.getDefaultObservation()
        let lastModified = NoctuaSkyClient.observations.lastModified()
        if (lastModified) {
          obs.observingSetup = lastModified.observingSetup
        }
        this.create = true
      } else {
        console.log('Loading observation: ' + v)
        that.isLoading = true
        obs = NoctuaSkyClient.observations.get(v).then(ob => {
          that.create = false
          that.resetObs(ob)
          that.isLoading = false
        })
        return
      }
      this.resetObs(obs)
    }
  },
  beforeRouteEnter (to, from, next) {
    // called before the route that renders this component is confirmed.
    // does NOT have access to `this` component instance,
    // because it has not been created yet when this guard is called!
    next(vm => {
      vm.saveViewSettings()
      vm.refreshObs(vm.id)
    })
  },
  beforeRouteUpdate (to, from, next) {
    // called when the route that renders this component has changed,
    // but this component is reused in the new route.
    // For example, for a route with dynamic params `/foo/:id`, when we
    // navigate between `/foo/1` and `/foo/2`, the same `Foo` component instance
    // will be reused, and this hook will be called when that happens.
    // has access to `this` component instance.
    this.refreshObs(this.id)
    next()
  },
  beforeRouteLeave (to, from, next) {
    // called when the route that renders this component is about to
    // be navigated away from.
    // has access to `this` component instance.
    this.restoreViewSettings()
    next()
  },
  watch: {
    obsSkySource: function () {
      this.skySourceSearchMenu = false
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
      return swh.iconForObservation(this.observation)
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
        return this.observation.location ? this.observation.location.shortName : ''
      } else {
        return 'Location'
      }
    },
    currentLocationSubtitle: function () {
      if (this.observation.location) {
        return this.observation.location ? this.observation.location.country : ''
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
      return NoctuaSkyClient.titleForObservingSetup(this.observation.observingSetup)
    },
    currentObservingSetupIcon: function () {
      return NoctuaSkyClient.iconForObservingSetup(this.observation.observingSetup)
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
