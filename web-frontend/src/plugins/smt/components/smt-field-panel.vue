// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>

<v-flex xs12>
  <h3 class="pt-3">{{ fieldDescription.name }}</h3>
  <div v-if="isTags">
    <v-chip small class="white--text" color="secondary" v-for="(count, name) in data" :key="name">
      {{ name }}&nbsp;<span class="primary--text"> ({{ count }})</span>
    </v-chip>
  </div>
  <div v-if="isDateRange">
    <GChart type="ColumnChart" :data="data" :options="dateRangeChartOptions"/>
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
        legend: {position: 'none'},
        backgroundColor: '#424242',
        chartArea: {left: '5%', top: '5%', width: '90%', height: '75%'},
        hAxis: {
          textStyle: {
            color: 'white'
          },
          gridlines: {
            color: '#424242',
            units: {
              days: {format: ['DD HH:MM']},
              hours: {format: ['HH:MM']}
            }
          }
        },
        vAxis: {
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
  },
  computed: {
    isTags: function () {
      return this.fieldDescription && this.fieldDescription.widget === 'tags' && this.fieldResults
    },
    isDateRange: function () {
      return this.fieldDescription && this.fieldDescription.widget === 'date_range' && this.fieldResults
    },
    data: function () {
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
    }
  },
  components: { GChart }
}
</script>

<style>
</style>
