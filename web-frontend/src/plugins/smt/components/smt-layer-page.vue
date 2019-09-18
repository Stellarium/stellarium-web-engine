// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>
  <div style="height: 100%;">
    <smt-panel-root-toolbar></smt-panel-root-toolbar>
    <div class="scroll-container">
      <v-container fluid style="height: 100%">
        <v-card>
          <v-card-title primary-title>
            <h3 class="headline mb-0">Layer</h3>
          </v-card-title>
          <v-card-text>
            <v-layout column justify-space-between>
              <p>Count: {{ results.summary.count }}</p>
              <p>Nb constraints: {{ query.constraints.length }}</p>
            </v-layout>
          </v-card-text>
        </v-card>
        <v-layout column>
          <smt-field-panel v-for="(field,i) in $smt.fieldsList" :key="field.id" :fieldDescription="field" :fieldResults="results.fields[i]">
          </smt-field-panel>
        </v-layout>
      </v-container>
    </div>
  </div>
</template>

<script>
import SmtPanelRootToolbar from './smt-panel-root-toolbar.vue'
import SmtFieldPanel from './smt-field-panel.vue'
import alasql from 'alasql'
import Vue from 'vue'

export default {
  data: function () {
    return {
      query: {
        constraints: []
      },
      results: {
        summary: {
          count: 0
        },
        fields: []
      }
    }
  },
  methods: {
    fId2AlaSql: function (fieldId) {
      return fieldId.replace(/\./g, '->')
    },
    thumbClicked: function (obs) {
      this.$router.push('/p/observations/' + obs.id)
    },
    refreshObservationGroups: function () {
      let that = this

      alasql.promise('SELECT COUNT(*) AS total FROM features').then(res => {
        that.results.summary.count = res[0].total
      })

      alasql.aggr.VALUES_AND_COUNT = function (value, accumulator, stage) {
        if (stage === 1) {
          let ac = {}
          ac[value] = 1
          return ac
        } else if (stage === 2) {
          accumulator[value] = (accumulator[value] !== undefined) ? accumulator[value] + 1 : 1
          return accumulator
        } else if (stage === 3) {
          return accumulator
        }
      }

      that.results.fields = that.$smt.fieldsList.map(function (e) { return { 'status': 'loading', 'data': [] } })
      for (let i in that.$smt.fieldsList) {
        let field = that.$smt.fieldsList[i]
        let fid = that.fId2AlaSql(field.id)
        if (field.widget === 'tags') {
          alasql.promise('SELECT VALUES_AND_COUNT(' + fid + ') AS tags FROM features').then(res => {
            that.results.fields[i] = {
              status: 'ok',
              data: res[0].tags
            }
          })
        }
        if (field.widget === 'date_range') {
          alasql.promise('SELECT MIN(DATE(' + fid + ')) AS dmin, MAX(DATE(' + fid + ')) AS dmax FROM features').then(res => {
            let start = new Date(res[0].dmin)
            let step = (res[0].dmax - res[0].dmin) / 10 + 0.00000001
            alasql.promise('SELECT COUNT(*) AS c FROM features GROUP BY FLOOR((DATE(' + fid + ') - ?) / ?)', [start, step]).then(res2 => {
              let h = res2.map(c => c['c'])
              Vue.set(that.results.fields[i], 'status', 'ok')
              Vue.set(that.results.fields[i], 'data', { labels: [], values: h })
            })
          })
        }
      }
    }
  },
  watch: {
    '$store.state.SMT.status': function () {
      this.refreshObservationGroups()
    }
  },
  computed: {
  },
  mounted: function () {
    this.refreshObservationGroups()
  },
  components: { SmtPanelRootToolbar, SmtFieldPanel }
}
</script>

<style>
.scroll-container {
  height: calc(100% - 48px);
  overflow-y: auto;
  backface-visibility: hidden;
}
ul {
  padding-left: 30px;
}
</style>
