// Stellarium Web - Copyright (c) 2020 - Stellarium Labs SAS
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.
//
// This file is part of the Survey Monitoring Tool plugin, which received
// funding from the Centre national d'études spatiales (CNES).

<template>
  <v-col cols="12">
    <v-row no-gutters>
      <v-col cols="10">
        <GChart type="ColumnChart" :data="fieldResultsData.table" :options="rangeChartOptions"  style="margin-bottom: -10px; height: 120px"/>
      </v-col>
      <v-col cols="2"><v-btn v-if="wasChanged" small fab @click="cancelButtonClicked"><v-icon>mdi-close</v-icon></v-btn></v-col>
      <v-col cols="10">
        <v-range-slider hide-details class="px-3 my-0" v-model="rangeSliderValues" :min="range[0]" :max="range[1]" v-on:start="isUserDragging = true" v-on:end="isUserDragging = false"></v-range-slider>
      </v-col>
      <v-col cols="2" style="margin-top: -10px">
        <v-btn small fab :disabled="!wasChanged" @click="rangeButtonClicked">Add</v-btn>
      </v-col>
      <v-col cols="1"></v-col>
      <v-col cols="4">
        <v-text-field dense solo single-line hide-details readonly :value="formatValue(rangeSliderValues[0])"></v-text-field>
      </v-col>
      <v-col cols="1"></v-col>
      <v-col cols="4">
        <v-text-field dense solo single-line hide-details readonly :value="formatValue(rangeSliderValues[1])"></v-text-field>
      </v-col>
      <v-col cols="2"></v-col>
    </v-row>
  </v-col>
</template>

<script>
import _ from 'lodash'
import { GChart } from 'vue-google-charts'

export default {
  data: function () {
    return {
      isUserDragging: false,
      wasChanged: false,
      rangeSliderValues: [0, 1],
      rangeChartOptions: {
        chart: {
          title: 'Range'
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
  props: ['fieldResults'],
  methods: {
    formatValue: function (d) {
      if (this.fieldResults.field.formatFuncCompiled) {
        return this.fieldResults.field.formatFuncCompiled({ x: d })
      }
      return '' + d
    },
    rangeButtonClicked: function () {
      const constraint = {
        fieldId: this.fieldResults.field.id,
        operation: 'NUMBER_RANGE',
        expression: [this.rangeSliderValues[0], this.rangeSliderValues[1]],
        negate: false
      }
      this.$emit('add-constraint', constraint)
      this.wasChanged = false
    },
    cancelButtonClicked: function () {
      this.$emit('constraint-live-changed', undefined)
      this.rangeSliderValues = this.range
      this.wasChanged = false
    }
  },
  computed: {
    fieldResultsData: function () {
      if (this.fieldResults) {
        const newData = _.cloneDeep(this.fieldResults.data)
        if (newData.table.length) newData.table[0].push({ role: 'annotation' })
        for (let i = 1; i < newData.table.length; ++i) {
          newData.table[i].push('' + newData.table[i][1])
          newData.table[i][0] = this.formatValue(newData.table[i][0])
        }
        return newData
      }
      return {}
    },
    range: function () {
      if (this.fieldResults && this.fieldResults.data && this.fieldResults.data.min !== undefined) {
        return [this.fieldResults.data.min, this.fieldResults.data.max]
      }
      return [0, 1]
    }
  },
  mounted: function () {
    this.rangeSliderValues = this.range
    this.wasChanged = false
  },
  watch: {
    rangeSliderValues: function (s) {
      const constraint = {
        fieldId: this.fieldResults.field.id,
        operation: 'NUMBER_RANGE',
        expression: [this.rangeSliderValues[0], this.rangeSliderValues[1]],
        negate: false
      }
      if (this.isUserDragging) {
        this.$emit('constraint-live-changed', constraint)
        this.wasChanged = true
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
