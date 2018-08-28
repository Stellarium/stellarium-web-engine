// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

  <div style="height: 100%">
    <observing-panel-root-toolbar></observing-panel-root-toolbar>
    <div class="scroll-container">
      <v-container fluid style="height: 100%">
        <v-card>
          <v-card-title primary-title>
            <h3 class="headline mb-0">Calendar of events</h3>
          </v-card-title>
          <v-card-text>
            <v-layout row justify-space-between>
              <v-select :items="months" v-model="month"></v-select>
              &nbsp; &nbsp;
              <v-text-field :value="year" type="Number"></v-text-field>
            </v-layout>
          </v-card-text>
        </v-card>
        <v-list two-line subheader style="margin-top: 10px">
          <v-list-tile v-if="showProgress" style="position: relative">
            <v-progress-circular indeterminate v-bind:size="50" style="position: absolute; left: 0; right: 0; margin-left: auto; margin-right: auto;"></v-progress-circular>
          </v-list-tile>
          <v-list-tile avatar v-for="event in events" v-bind:key="event.id"  @click="eventClicked(event)">
            <v-list-tile-avatar>
              <img v-bind:src="getIcon(event.type)"/>
            </v-list-tile-avatar>
            <v-list-tile-content>
              <v-list-tile-title>{{ event.desc }}</v-list-tile-title>
              <v-list-tile-sub-title class="grey--text">{{ event.time.format('MMMM DD, HH:mm') }}</v-list-tile-sub-title>
            </v-list-tile-content>
          </v-list-tile>
        </v-list>
      </v-container>
    </div>
  </div>

</template>

<script>
import ObservingPanelRootToolbar from '@/components/observing-panel-root-toolbar.vue'
import Moment from 'moment'

export default {
  data: function () {
    let d = new Date()
    d.setMJD(this.$store.state.stel.observer.utc)
    return {
      year: Moment(d).format('YYYY'),
      month: Moment(d).format('MMMM'),
      months: Moment.months(),
      computingInProgress: false
    }
  },
  methods: {
    getIcon: function (type) {
      return '/static/images/events/' + type + '.svg'
    },
    eventClicked: function (event) {
      this.$stel.core.observer.utc = event.time.toDate().getMJD()
      let ss = this.$stel.getObjByNSID(event.o1.nsid)
      this.$stel.core.selection = ss
      ss.update()
      this.$stel.core.lookat(ss.azalt, 1.0)
    }
  },
  computed: {
    events: function () {
      if (!this.$store.state.initComplete) {
        return []
      }
      console.log('Compute Sky This Month Events')
      let start = Moment(this.year + '-' + this.month + '-01', 'YYYY-MMMM-DD')
      console.log('Recomputing ephemeris for', start.format('MMMM YYYY'))
      let ret = []
      let end = start.clone()
      end.endOf('month')
      this.$stel.calendar({
        start: start.toDate(),
        end: end.toDate(),
        onEvent: function (ev) {
          ev.id = ev.time + ev.type + ev.desc
          ev.time = new Moment(ev.time)
          ret.push(ev)
        }})
      return ret
    },
    showProgress: function () {
      return this.computingInProgress || !this.$store.state.initComplete
    }
  },
  components: { ObservingPanelRootToolbar }
}
</script>

<style>
.input-group {
  margin: 0px;
}

.scroll-container {
  height: calc(100% - 48px);
  overflow-y: auto;
  backface-visibility: hidden;
}
</style>
