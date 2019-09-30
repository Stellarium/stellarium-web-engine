// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>
  <div style="height: 100%;">
    <smt-panel-root-toolbar></smt-panel-root-toolbar>
    <div class="scroll-container">
      <v-container fluid style="height: 100%">
        <v-card>
          <v-card-text>
            <div class="display-1 text--primary">{{ results.summary.count }} items</div>
            <div class="mt-2">Constraints:</div>
            <v-row no-gutters>
              <div v-for="(constraint, i) in constraintsToDisplay" :key="i" style="text-align: center;" class="pa-1">
                <div class="caption white--text">{{ constraint.field.name }}</div>
                <v-chip small class="white--text" :close="constraint.closable" :disabled="!constraint.closable" color="primary" @click:close="constraintClosed(i)">
                <div :style="{ minWidth: constraint.closable ? 60 : 82 + 'px' }">{{ printConstraint(constraint) }}</div>
                </v-chip>
              </div>
            </v-row>
          </v-card-text>
        </v-card>
        <v-layout column>
          <smt-field-panel v-for="fr in resultsFieldsToDisplay" :key="fr.field.id" :fieldDescription="fr.field" :fieldResults="fr" v-on:add-constraint="addConstraint">
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
import Moment from 'moment'

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
        fields: [],
        implicitConstraints: [],
        geojsonObj: undefined
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
    formatDate: function (d) {
      return new Moment(d).format('YYYY-MM-DD')
    },
    printConstraint: function (c) {
      if (c.field.widget === 'date_range') return this.formatDate(c.expression[0]) + ' - ' + this.formatDate(c.expression[1])
      return c.expression
    },
    refreshObservationGroups: function () {
      let that = this

      if (this.$store.state.SMT.status !== 'ready') {
        return
      }

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

      // Compute the WHERE clause to be used in following queries
      let whereClause = ''
      let constraintsIds = []
      if (this.query.constraints && this.query.constraints.length) {
        whereClause = ' WHERE '
        for (let i in that.query.constraints) {
          let c = that.query.constraints[i]
          if (c.operation === 'STRING_EQUAL') {
            whereClause += that.fId2AlaSql(c.field.id) + ' = "' + c.expression + '"'
          } else if (c.operation === 'DATE_RANGE') {
            let fid = that.fId2AlaSql(c.field.id)
            whereClause += ' DATE(' + fid + ') >= DATE("' + c.expression[0] + '")'
            whereClause += ' AND DATE(' + fid + ') <= DATE("' + c.expression[1] + '")'
          }
          if (i < that.query.constraints.length - 1) {
            whereClause += ' AND '
          }
          constraintsIds.push(c.field.id)
        }
      }

      // Reset all fields values
      that.results.fields = that.$smt.fieldsList.map(function (e) { return { 'status': 'loading', 'data': [] } })
      that.results.implicitConstraints = []

      alasql.promise('SELECT COUNT(*) AS total FROM features' + whereClause).then(res => {
        that.results.summary.count = res[0].total
      })

      // Create the geojson results of the query
      alasql.promise('SELECT geometry FROM features' + whereClause).then(res => {
        let geojson = {
          type: 'FeatureCollection',
          features: []
        }
        for (let i in res) {
          geojson.features.push({
            geometry: res[i].geometry,
            type: 'Feature',
            properties: {}
          })
        }

        if (that.geojsonObj) {
          that.$observingLayer.remove(that.geojsonObj)
          that.geojsonObj.destroy()
        }

        that.geojsonObj = that.$stel.createObj('geojson', { data: geojson })
        that.$observingLayer.add(that.geojsonObj)
      })

      // And recompute them
      for (let i in that.$smt.fieldsList) {
        let field = that.$smt.fieldsList[i]
        let fid = that.fId2AlaSql(field.id)
        if (field.widget === 'tags') {
          let req = 'SELECT VALUES_AND_COUNT(' + fid + ') AS tags FROM features' + whereClause
          alasql.promise(req).then(res => {
            that.results.fields[i] = {
              field: field,
              status: 'ok',
              data: res[0].tags
            }
            // Fill the implicit constraints list, i.e. the tags where only one value remains
            if (!constraintsIds.includes(field.id) && Object.keys(res[0].tags).length === 1) {
              that.results.implicitConstraints.push({ field: field, expression: Object.keys(res[0].tags)[0], closable: false })
            }
          }, err => {
            console.log(err)
          })
        }
        if (field.widget === 'date_range') {
          let req = 'SELECT MIN(DATE(' + fid + ')) AS dmin, MAX(DATE(' + fid + ')) AS dmax FROM features' + whereClause
          alasql.promise(req).then(res => {
            let start = new Date(res[0].dmin)
            start.setHours(0, 0, 0, 0)
            // Switch to next day and truncate
            let stop = new Date(res[0].dmax + 1000 * 60 * 60 * 24)
            stop.setHours(0, 0, 0, 0)
            // Range in days
            let range = (stop - start) / (1000 * 60 * 60 * 24)
            let step = 'DAY'
            if (range > 3 * 365) {
              step = 'YEAR'
            } else if (range > 3 * 30) {
              step = 'MONTH'
            }

            alasql.promise('SELECT COUNT(*) AS c, DATE(FIRST(' + fid + ')) AS d FROM features' + whereClause + ' GROUP BY ' + step + ' (' + fid + ')').then(res2 => {
              let data = {
                min: start,
                max: stop,
                step: step,
                table: [['Date', 'Count']]
              }
              for (let j in res2) {
                let d = res2[j].d
                d.setHours(0, 0, 0, 0)
                if (step === 'MONTH') d.setDate(0)
                if (step === 'YEAR') d.setMonth(0)
                data.table.push([d, res2[j].c])
              }
              Vue.set(that.results.fields[i], 'field', field)
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
      this.query.constraints.splice(i, 1)
      this.refreshObservationGroups()
    }
  },
  watch: {
    '$store.state.SMT.status': function () {
      this.refreshObservationGroups()
    }
  },
  computed: {
    // Return real and implicit constraints to display in GUI
    constraintsToDisplay: function () {
      if (this.$store.state.SMT.status !== 'ready') {
        return []
      }

      let res = this.query.constraints.slice()
      for (let i in res) {
        res[i]['closable'] = true
      }
      // Add implicit constraints (when only a unique tag value match the query)
      res = res.concat(this.results.implicitConstraints)
      return res
    },
    // Return fields with relevant information to display in GUI
    // Tags field with only one value are suppressed (already displayed as implicit constraints)
    resultsFieldsToDisplay: function () {
      if (this.$store.state.SMT.status !== 'ready') {
        return []
      }
      let res = []
      for (let i in this.results.fields) {
        let rf = this.results.fields[i]
        if (!rf.field) continue
        if (rf.field.widget === 'tags' && Object.keys(rf.data).length === 1) continue
        res.push(rf)
      }
      return res
    }
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
