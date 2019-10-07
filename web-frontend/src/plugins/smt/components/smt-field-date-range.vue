// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>
  <v-col cols="12">
    <v-row no-gutters>
      <v-col cols="10">
        <GChart type="ColumnChart" :data="fieldResultsData.table" :options="dateRangeChartOptions"  style="margin-bottom: -10px; height: 120px"/>
      </v-col>
      <v-col cols="2"></v-col>
      <v-col cols="10">
        <v-range-slider hide-details class="px-3 my-0" v-model="dateRangeSliderValues" :min="dateRange[0]" :max="dateRange[1]" v-on:start="isUserDragging = true" v-on:end="isUserDragging = false"></v-range-slider>
      </v-col>
      <v-col cols="2" style="margin-top: -10px">
        <v-btn small fab @click="rangeButtonClicked">OK</v-btn>
      </v-col>
      <v-col cols="1"></v-col>
      <v-col cols="4">
        <v-text-field dense solo single-line hide-details v-mask="dateMask" :value="formatDate(dateRangeSliderValues[0])" @change="rangeMinTextuallyChanged"></v-text-field>
      </v-col>
      <v-col cols="1"></v-col>
      <v-col cols="4">
        <v-text-field dense solo single-line hide-details  v-mask="dateMask" :value="formatDate(dateRangeSliderValues[1])" @change="rangeMaxTextuallyChanged"></v-text-field>
      </v-col>
      <v-col cols="2"></v-col>
    </v-row>
  </v-col>
</template>

<script>
import _ from 'lodash'
import { GChart } from 'vue-google-charts'
import Moment from 'moment'
import { mask } from 'vue-the-mask'

export default {
  data: function () {
    return {
      isUserDragging: false,
      dateMask: '####-##-##',
      dateRangeSliderValues: [0, 1],
      dateRangeChartOptions: {
        chart: {
          title: 'Date Range'
        },
        legend: { position: 'none' },
        backgroundColor: '#212121',
        annotations: {
          highContrast: false,
          textStyle: {
            color: '#ffffff'
          }
        },
        chartArea: { left: '5%', top: '5%', width: '90%', height: '65%' },
        hAxis: {
          textStyle: {
            color: 'white'
          },
          gridlines: {
            color: 'transparent'
          }
        },
        vAxis: {
          textStyle: {
            color: 'transparent'
          },
          gridlines: {
            count: 0
          }
        }
      }
    }
  },
  directives: { mask },
  props: ['fieldResults'],
  methods: {
    formatDate: function (d) {
      return new Moment(d).format('YYYY-MM-DD')
    },
    rangeButtonClicked: function () {
      let constraint = {
        'field': this.fieldResults.field,
        'operation': 'DATE_RANGE',
        'expression': [this.dateRangeSliderValues[0], this.dateRangeSliderValues[1]],
        'negate': false
      }
      this.$emit('add-constraint', constraint)
    },
    rangeMinTextuallyChanged: function (v) {
      try {
        let t = new Date(v).getTime()
        this.dateRangeSliderValues = [t, this.dateRangeSliderValues[1]]
      } catch (e) {
      }
    },
    rangeMaxTextuallyChanged: function (v) {
      try {
        let t = new Date(v).getTime()
        this.dateRangeSliderValues = [this.dateRangeSliderValues[0], t]
      } catch (e) {
      }
    }
  },
  computed: {
    fieldResultsData: function () {
      if (this.fieldResults) {
        let newData = _.cloneDeep(this.fieldResults.data)
        if (newData.table.length) newData.table[0].push({ role: 'annotation' })
        for (let i = 1; i < newData.table.length; ++i) {
          newData.table[i].push('' + newData.table[i][1])
        }
        return newData
      }
      return {}
    },
    dateRange: function () {
      if (this.fieldResults && this.fieldResults.data && this.fieldResults.data.min) {
        return [this.fieldResults.data.min.getTime(), this.fieldResults.data.max.getTime()]
      }
      return [0, 1]
    }
  },
  mounted: function () {
    this.dateRangeSliderValues = this.dateRange
  },
  watch: {
    dateRangeSliderValues: function (s) {
      let constraint = {
        'field': this.fieldResults.field,
        'operation': 'DATE_RANGE',
        'expression': [this.dateRangeSliderValues[0], this.dateRangeSliderValues[1]],
        'negate': false
      }
      if (this.isUserDragging) {
        this.$emit('constraint-live-changed', constraint)
      }
    }
  },
  components: { GChart }
}
</script>

<style>
.v-input__slot {
  min-height: 10px;
}
</style>
