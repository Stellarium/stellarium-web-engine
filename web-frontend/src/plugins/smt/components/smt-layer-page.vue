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
              <div>
                <v-chip small close class="white--text" color="primary" v-for="(constraint, i) in query.constraints" :key="i" @click:close="constraintClosed">
                  {{ constraint.field.name }} = {{ constraint.expression }}&nbsp;
                </v-chip>
              </div>
            </v-layout>
          </v-card-text>
        </v-card>
        <v-layout column>
          <smt-field-panel v-for="(field,i) in $smt.fieldsList" :key="field.id" :fieldDescription="field" :fieldResults="results.fields[i]" v-on:add-constraint="addConstraint">
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

      // Add a custom aggregation operator for the chip tags
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

      let whereClause = ''
      if (this.query.constraints && this.query.constraints.length) {
        whereClause = ' WHERE '
        for (let i in that.query.constraints) {
          let c = that.query.constraints[i]
          if (c.operation === 'STRING_EQUAL') {
            whereClause += that.fId2AlaSql(c.field.id) + ' = "' + c.expression + '"'
          }
          if (i < that.query.constraints.length - 1) {
            whereClause += ' AND '
          }
        }
      }

      alasql.promise('SELECT COUNT(*) AS total FROM features' + whereClause).then(res => {
        that.results.summary.count = res[0].total
      })

      that.results.fields = that.$smt.fieldsList.map(function (e) { return { 'status': 'loading', 'data': [] } })
      for (let i in that.$smt.fieldsList) {
        let field = that.$smt.fieldsList[i]
        let fid = that.fId2AlaSql(field.id)
        if (field.widget === 'tags') {
          alasql.promise('SELECT VALUES_AND_COUNT(' + fid + ') AS tags FROM features' + whereClause).then(res => {
            that.results.fields[i] = {
              status: 'ok',
              data: res[0].tags
            }
          })
        }
        if (field.widget === 'date_range') {
          alasql.promise('SELECT MIN(DATE(' + fid + ')) AS dmin, MAX(DATE(' + fid + ')) AS dmax FROM features' + whereClause).then(res => {
            let start = new Date(res[0].dmin)
            let step = (res[0].dmax - res[0].dmin) / 10 + 0.00000001
            alasql.promise('SELECT COUNT(*) AS c FROM features' + whereClause + ' GROUP BY FLOOR((DATE(' + fid + ') - ?) / ?)', [start, step]).then(res2 => {
              let data = [['Date', 'Count']]
              for (let j in res2) {
                let d = new Date()
                d.setTime(start.getTime() + step * j)
                data.push([d, res2[j].c])
              }
              Vue.set(that.results.fields[i], 'status', 'ok')
              Vue.set(that.results.fields[i], 'data', data)
            })
          })
        }
      }
    },
    addConstraint: function (c) {
      this.query.constraints.push(c)
      this.refreshObservationGroups()
    },
    constraintClicked: function (i) {
      console.log('constraintClicked: ' + i)
    },
    constraintClosed: function (i) {
      console.log('constraintClosed: ' + i)
      this.query.constraints.splice(i, 1)
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
