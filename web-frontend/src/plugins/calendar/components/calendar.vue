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
            <v-row justify="space-between">
              <v-col><v-select :items="months" v-model="month"></v-select></v-col>
              <v-col><v-text-field :value="year" type="Number"></v-text-field></v-col>
            </v-row>
          </v-card-text>
        </v-card>
        <v-list two-line subheader style="margin-top: 10px">
          <v-list-item v-if="showProgress" style="position: relative">
            <v-progress-circular indeterminate v-bind:size="50" style="position: absolute; left: 0; right: 0; margin-left: auto; margin-right: auto;"></v-progress-circular>
          </v-list-item>
          <v-list-item v-for="event in events" v-bind:key="event.id"  @click="eventClicked(event)">
            <v-list-item-avatar>
              <img v-bind:src="getIcon(event.type)"/>
            </v-list-item-avatar>
            <v-list-item-content>
              <v-list-item-title>{{ event.desc }}</v-list-item-title>
              <v-list-item-subtitle class="grey--text">{{ event.time.format('MMMM DD, HH:mm') }}</v-list-item-subtitle>
            </v-list-item-content>
          </v-list-item>
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
      return process.env.BASE_URL + 'plugins/calendar/events/' + type + '.svg'
    },
    eventClicked: function (event) {
      this.$stel.core.observer.utc = event.time.toDate().getMJD()
      this.$stel.core.selection = event.o1
      this.$stel.pointAndLock(event.o1)
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
        } })
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
