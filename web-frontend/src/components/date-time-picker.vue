// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <v-card width="400">
    <v-container>
      <v-row justify="space-between" no-gutters class="ma-3">
        <div>
          <v-btn text icon class="up_down_bt" style="margin-left: 16px" @mousedown="incTime('years')" @touchstart.prevent="incTime('years')"><v-icon>mdi-menu-up</v-icon></v-btn>
          <v-btn text icon class="up_down_bt" style="margin-left: 21px" @mousedown="incTime('months')" @touchstart.prevent="incTime('months')"><v-icon>mdi-menu-up</v-icon></v-btn>
          <v-btn text icon class="up_down_bt" style="margin-left: 8px"  @mousedown="incTime('days')" @touchstart.prevent="incTime('days')"><v-icon>mdi-menu-up</v-icon></v-btn>
          <h1>{{ date }}</h1>
          <v-btn text icon class="up_down_bt" style="margin-left: 16px" @mousedown="decTime('years')" @touchstart.prevent="decTime('years')"><v-icon>mdi-menu-down</v-icon></v-btn>
          <v-btn text icon class="up_down_bt" style="margin-left: 21px" @mousedown="decTime('months')" @touchstart.prevent="decTime('months')"><v-icon>mdi-menu-down</v-icon></v-btn>
          <v-btn text icon class="up_down_bt" style="margin-left: 8px"  @mousedown="decTime('days')" @touchstart.prevent="decTime('days')"><v-icon>mdi-menu-down</v-icon></v-btn>
        </div>
        <div>
        <div>
        <v-tooltip top>
          <template v-slot:activator="{ on }">
            <v-btn text icon @click="resetTime" style="margin-top: 5px" v-on="on"><v-icon>mdi-history</v-icon></v-btn>
          </template>
          <span>{{ $t('Back to real time') }}</span>
        </v-tooltip>
        </div>
        <div>
        <v-tooltip top>
          <template v-slot:activator="{ on }">
            <v-btn text icon @click="togglePauseTime" style="margin-top: 0px" v-on="on"><v-icon>{{ togglePauseTimeIcon }}</v-icon></v-btn>
          </template>
          <span>{{ $t('Pause/unpause time') }}</span>
        </v-tooltip>
        </div>
        </div>
        <div>
          <v-btn text icon class="up_down_bt" @mousedown="incTime('hours')" @touchstart.prevent="incTime('hours')"><v-icon>mdi-menu-up</v-icon></v-btn>
          <v-btn text icon class="up_down_bt ml-1" @mousedown="incTime('minutes')" @touchstart.prevent="incTime('minutes')"><v-icon>mdi-menu-up</v-icon></v-btn>
          <v-btn text icon class="up_down_bt ml-1" @mousedown="incTime('seconds')" @touchstart.prevent="incTime('seconds')"><v-icon>mdi-menu-up</v-icon></v-btn>
          <h1 class="ml-2">{{ time }}</h1>
          <v-btn text icon class="up_down_bt" @mousedown="decTime('hours')" @touchstart.prevent="decTime('hours')"><v-icon>mdi-menu-down</v-icon></v-btn>
          <v-btn text icon class="up_down_bt ml-1" @mousedown="decTime('minutes')" @touchstart.prevent="decTime('minutes')"><v-icon>mdi-menu-down</v-icon></v-btn>
          <v-btn text icon class="up_down_bt ml-1" @mousedown="decTime('seconds')" @touchstart.prevent="decTime('seconds')"><v-icon>mdi-menu-down</v-icon></v-btn>
        </div>
      </v-row>
    </v-container>
    <div style="padding: 20px">
      <div style="position: absolute">
        <svg height="30" width="360">
          <defs>
            <linearGradient id="grad1" x1="0%" y1="0%" x2="100%" y2="0%">
              <stop v-for="stop in stops" :offset="stop.percent" :style="stop.style" :key="stop.percent"/>
            </linearGradient>
          </defs>
          <rect width="100%" height="100%" fill="url(#grad1)" />
        </svg>
      </div>
      <v-slider min="0" max="1439" style="padding: 0px; width: 360px;" v-model="timeMinute" :hint="sliderHint" persistent-hint></v-slider>
    </div>
  </v-card>
</template>

<script>

import Moment from 'moment'
var clickTimeout
var nbClickRepeat = 0

export default {
  data: function () {
    return {
      stops: [],
      stopCacheKey: {
        sliderStartTime: undefined,
        location: undefined
      }
    }
  },
  props: ['value', 'location'],
  computed: {
    // The MomentJS time in local time
    localTime: {
      get: function () {
        const m = Moment(this.value)
        m.local()
        return m
      },
      set: function (newValue) {
        this.$emit('input', newValue.format())
      }
    },
    time: {
      get: function () {
        return this.localTime.format('HH:mm:ss')
      }
    },
    date: {
      get: function () {
        return this.localTime.format('YYYY-MM-DD')
      }
    },
    timeMinute: {
      get: function () {
        // 0 means 12:00, 720 means midnight, 1440 (=24*60) means 12:00 the day after
        const t = this.localTime
        return t.hours() < 12 ? (t.hours() + 12) * 60 + t.minutes() : (t.hours() - 12) * 60 + t.minutes()
      },
      set: function (newValue) {
        var t = Moment(this.sliderStartTime)
        t.add(newValue, 'minutes')
        this.$emit('input', t.format())
      }
    },
    sliderStartTime: function () {
      const t = this.localTime.clone()
      if (t.hours() < 12) {
        t.subtract(1, 'days')
      }
      t.hours(12)
      t.minutes(0)
      t.seconds(0)
      t.milliseconds(0)
      return t
    },
    sliderHint: function () {
      const tm = this.timeMinute
      const stop = this.stops[Math.floor(tm * this.stops.length / 1440)]
      if (!stop) return ''
      if (stop.sunAlt > 0) {
        return this.$t('Daylight')
      }
      if (stop.sunAlt < -16) {
        return stop.moonAlt < 5 ? this.$t('Dark night') : this.$t('Moonlight')
      }
      return tm > 720 ? this.$t('Dawn') : this.$t('Twilight')
    },
    isTimePaused: function () {
      return this.$store.state.stel.time_speed === 0
    },
    togglePauseTimeIcon: function () {
      return this.isTimePaused ? 'mdi-play' : 'mdi-pause'
    }
  },
  methods: {
    resetTime: function () {
      const m = Moment()
      m.local()
      this.$emit('input', m.format())
    },
    togglePauseTime: function () {
      this.$stel.core.time_speed = (this.$stel.core.time_speed === 0) ? 1 : 0
    },
    incTime: function (unit) {
      this.startIncTime(1, unit)
    },
    decTime: function (unit) {
      this.startIncTime(-1, unit)
    },
    startIncTime: function (v, unit) {
      const that = this
      clickTimeout = setTimeout(_ => {
        const t = this.localTime.clone()
        t.add(v, unit)
        this.$emit('input', t.format())
        nbClickRepeat++
        that.startIncTime(v, unit)
      }, nbClickRepeat === 0 ? 0 : (nbClickRepeat === 1 ? 500 : (nbClickRepeat < 10 ? 100 : (nbClickRepeat < 100 ? 50 : 20))))
    },
    stopIncTime: function () {
      if (clickTimeout) {
        clearTimeout(clickTimeout)
        clickTimeout = undefined
        nbClickRepeat = 0
      }
    },
    // 0 means 12:00, 720 means midnight, 1440 (=24*60) means 12:00 the day after
    timeMinuteRangeToUTC: function (tm) {
      return this.sliderStartTime.toDate().getMJD() + tm * 1 / (24 * 60)
    },
    refreshStops: function () {
      if (this.stopCacheKey.sliderStartTime === this.sliderStartTime.format() && this.stopCacheKey.location === JSON.stringify(this.location)) {
        return
      }
      const res = []
      const nbStop = 49
      const obs = this.$stel.core.observer.clone()
      const sun = this.$stel.getObj('NAME Sun')
      const moon = this.$stel.getObj('NAME Moon')
      for (let i = 0; i <= nbStop; ++i) {
        obs.utc = this.timeMinuteRangeToUTC(1440 * i / nbStop)
        const sunAlt = this.$stel.anpm(this.$stel.c2s(this.$stel.convertFrame(obs, 'ICRF', 'OBSERVED', sun.getInfo('radec', obs)))[1]) * 180.0 / Math.PI
        const moonAlt = this.$stel.anpm(this.$stel.c2s(this.$stel.convertFrame(obs, 'ICRF', 'OBSERVED', moon.getInfo('radec', obs)))[1]) * 180.0 / Math.PI
        const brightnessForAltitude = function (sunAlt, moonAlt) {
          const moonBrightness = moonAlt < 0 ? 0 : 2 / 35 * Math.min(20, moonAlt) / 20
          if (sunAlt > 0) return Math.min(10, 1 + sunAlt) + moonBrightness
          if (sunAlt < -16) return moonBrightness
          if (sunAlt < -10) return 1 / 35 * (16 + sunAlt) / 6 + moonBrightness
          return (1 - 1 / 35) * (10 + sunAlt) / 10 + 1 / 35 + moonBrightness
        }
        const brightness = Math.log10(1 + brightnessForAltitude(sunAlt, moonAlt) * 10) / 2
        res.push({
          percent: i / nbStop,
          style: 'stop-color:rgb(64,209,255);stop-opacity:' + brightness,
          sunAlt: sunAlt,
          moonAlt: moonAlt
        })
      }
      obs.destroy()
      this.stopCacheKey.sliderStartTime = this.sliderStartTime.format()
      this.stopCacheKey.location = JSON.stringify(this.location)
      this.stops = res
    }
  },
  mounted: function () {
    this.refreshStops()
    const that = this
    window.addEventListener('mouseup', function (event) {
      that.stopIncTime()
    })
    window.addEventListener('touchend', function (event) {
      that.stopIncTime()
      event.preventDefault()
    })
  },
  watch: {
    sliderStartTime: function () {
      this.refreshStops()
    },
    location: function () {
      this.refreshStops()
    }
  }
}
</script>

<style>
.up_down_bt {
  margin-bottom: -10px!important;
  margin-top: -10px!important;
  margin-left: 0px;
  margin-right: 0px!important;
}
</style>
