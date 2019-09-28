// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>

<v-flex xs12>
  <h3 class="pt-3">{{ fieldDescription.name }}</h3>
  <div v-if="isTags">
    <v-chip small class="white--text ma-1" color="secondary" v-for="(count, name) in fieldResultsData" :key="name" @click="chipClicked(name)">
      {{ name }}&nbsp;<span class="primary--text"> ({{ count }})</span>
    </v-chip>
  </div>
  <div v-if="isDateRange">
    <v-row no-gutters>
      <v-col cols="10">
        <GChart type="ColumnChart" :data="fieldResultsData" :options="dateRangeChartOptions"  style="margin-bottom: -20px"/>
      </v-col>
      <v-col cols="2"></v-col>
      <v-col cols="10">
        <v-range-slider></v-range-slider>
      </v-col>
      <v-col cols="2" style="margin-top: -10px">
        <v-btn small fab>OK</v-btn>
      </v-col>
    </v-row>
  </div>
</v-flex>

</template>

<script>
import _ from 'lodash'
import { GChart } from 'vue-google-charts'

export default {
  data: function () {
    return {
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
      },
      date_range: [20, 60]
    }
  },
  props: ['fieldDescription', 'fieldResults'],
  methods: {
    chipClicked: function (name) {
      let constraint = { 'field': this.fieldDescription, 'operation': 'STRING_EQUAL', 'expression': name, 'negate': false }
      this.$emit('add-constraint', constraint)
    }
  },
  computed: {
    isTags: function () {
      return this.fieldDescription && this.fieldDescription.widget === 'tags' && this.fieldResults
    },
    isDateRange: function () {
      return this.fieldDescription && this.fieldDescription.widget === 'date_range' && this.fieldResults
    },
    fieldResultsData: function () {
      if (this.isTags) {
        return this.fieldResults.data
      }
      if (this.isDateRange) {
        let newData = _.cloneDeep(this.fieldResults.data)
        if (newData.length) newData[0].push({ role: 'annotation' })
        for (let i = 1; i < newData.length; ++i) {
          newData[i].push('' + newData[i][1])
        }
        return newData
      }
      return {}
    }
  },
  components: { GChart }
}
</script>

<style>
</style>
