// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>
  <div style="height: 100%;">
    <smt-panel-root-toolbar></smt-panel-root-toolbar>
    <div class="scroll-container">
      <v-container class="pa-0" fluid style="height: 100%">
        <v-card tile>
          <v-card-text>
            <div v-if="$store.state.SMT.status === 'ready'" class="display-1 text--primary">{{ results.summary.count }} items</div>
            <div v-if="$store.state.SMT.status === 'loading'" class="display-1 text--primary">Loading data..</div>
            <div v-if="$store.state.SMT.status === 'ready'" class="mt-2">Constraints:</div>
            <v-row no-gutters>
              <div v-for="(constraint, i) in constraintsToDisplay" :key="i" style="text-align: center;" class="pa-1">
                <div class="caption white--text">{{ constraint.field.name }}</div>
                <v-chip small class="white--text" :close="constraint.closable" :disabled="!constraint.closable" color="primary" @click="constraintClicked(i)" @click:close="constraintClosed(i)">
                <div :style="{ minWidth: constraint.closable ? 60 : 82 + 'px' }">{{ printConstraint(constraint) }}</div>
                </v-chip>
              </div>
            </v-row>
          </v-card-text>
        </v-card>
        <v-container>
          <smt-field class="mb-2" v-for="fr in resultsFieldsToDisplay" :key="fr.field.id" :fieldDescription="fr.field" :fieldResults="fr" v-on:add-constraint="addConstraint" v-on:remove-constraint="removeConstraint">
          </smt-field>
        </v-container>
      </v-container>
    </div>
  </div>
</template>

<script>
import SmtPanelRootToolbar from './smt-panel-root-toolbar.vue'
import SmtField from './smt-field.vue'
import alasql from 'alasql'
import Vue from 'vue'
import Moment from 'moment'

export default {
  data: function () {
    return {
      query: {
        constraints: []
      },
      editedConstraint: undefined,
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
      return fieldId.replace(/properties\./g, '').replace(/\./g, '_')
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
    constraints2SQLWhereClause: function (constraints) {
      let that = this
      // Construct the SQL WHERE clause matching the given constraints
      if (!constraints || constraints.length === 0) {
        return ''
      }
      // Group constraints by field. We do a OR between constraints for the same field, AND otherwise.
      let groupedConstraints = {}
      for (let i in constraints) {
        let c = constraints[i]
        if (c.field.id in groupedConstraints) {
          groupedConstraints[c.field.id].push(c)
        } else {
          groupedConstraints[c.field.id] = [c]
        }
      }
      groupedConstraints = Object.values(groupedConstraints)

      // Convert one contraint to a SQL string
      let c2sql = function (c) {
        let fid = that.fId2AlaSql(c.field.id)
        if (c.operation === 'STRING_EQUAL') {
          return fid + ' = "' + c.expression + '"'
        } else if (c.operation === 'IS_UNDEFINED') {
          return fid + ' IS NULL'
        } else if (c.operation === 'DATE_RANGE') {
          return '( ' + fid + ' IS NOT NULL AND ' + fid + ' >= ' + c.expression[0] +
            ' AND ' + fid + ' <= ' + c.expression[1] + ')'
        }
      }

      let whereClause = ' WHERE '
      for (let i in groupedConstraints) {
        let gCons = groupedConstraints[i]
        for (let j in gCons) {
          whereClause += c2sql(gCons[j])
          if (j < gCons.length - 1) {
            whereClause += ' OR '
          }
        }
        if (i < groupedConstraints.length - 1) {
          whereClause += ' AND '
        }
      }
      return whereClause
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
      let whereClause = that.constraints2SQLWhereClause(this.query.constraints)
      let whereClauseEdited = that.constraints2SQLWhereClause(this.query.constraints.filter(c => !this.isEdited(c)))
      let constraintsIds = that.query.constraints.map(c => c.field.id)

      // Reset all fields values
      that.results.fields = that.$smt.fieldsList.map(function (e) { return { 'status': 'loading', 'data': {} } })
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

        if (geojson.features.length === 0) {
          return
        }
        that.geojsonObj = that.$stel.createObj('geojson')
        that.geojsonObj.setData(geojson)
        that.$observingLayer.add(that.geojsonObj)
        that.geojsonObj.filterAll(idx => {
          // if (do_not_show) return false
          // if (do_not_modify) return true
          return {
            fill: [1, 0.3, 0.3, 0.30],
            stroke: [0, 1, 0, 0],
            visible: true
          }
        })
      })

      // And recompute them
      for (let i in that.$smt.fieldsList) {
        let field = that.$smt.fieldsList[i]
        let fid = that.fId2AlaSql(field.id)
        let edited = that.editedConstraint && that.editedConstraint.field.id === field.id
        let wc = edited ? whereClauseEdited : whereClause
        if (field.widget === 'tags') {
          let req = 'SELECT VALUES_AND_COUNT(' + fid + ') AS tags FROM features' + wc
          alasql.promise(req).then(res => {
            let tags = res[0].tags ? res[0].tags : {}
            tags = Object.keys(tags).map(function (key) {
              let closable = that.query.constraints.filter(c => c.field.id === field.id && c.expression === key).length !== 0
              return { name: key, count: tags[key], closable: closable }
            })
            that.results.fields[i] = {
              field: field,
              status: 'ok',
              edited: edited,
              data: tags
            }
            // Fill the implicit constraints list, i.e. the tags where only one value remains
            if (!constraintsIds.includes(field.id) && res[0].tags && Object.keys(res[0].tags).length === 1) {
              that.results.implicitConstraints.push({ field: field, expression: Object.keys(res[0].tags)[0], closable: false })
            }
          }, err => {
            console.log(err)
          })
        }
        if (field.widget === 'date_range') {
          let req = 'SELECT MIN(' + fid + ') AS dmin, MAX(' + fid + ') AS dmax FROM features' + wc
          alasql.promise(req).then(res => {
            if (res[0].dmin === undefined || res[0].dmax === undefined) {
              // No results
              let data = {
                min: 0,
                max: 1,
                step: 'DAY',
                table: [['Date', 'Count']]
              }
              Vue.set(that.results.fields[i], 'field', field)
              Vue.set(that.results.fields[i], 'status', 'ok')
              Vue.set(that.results.fields[i], 'edited', edited)
              Vue.set(that.results.fields[i], 'data', data)
              return
            }
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

            alasql.promise('SELECT COUNT(*) AS c, FIRST(' + fid + ') AS d FROM features' + wc + ' GROUP BY ' + step + ' (' + fid + ')').then(res2 => {
              let data = {
                min: start,
                max: stop,
                step: step,
                table: [['Date', 'Count']]
              }
              for (let j in res2) {
                let d = new Date(res2[j].d)
                d.setHours(0, 0, 0, 0)
                if (step === 'MONTH') d.setDate(0)
                if (step === 'YEAR') d.setMonth(0)
                data.table.push([d, res2[j].c])
              }
              Vue.set(that.results.fields[i], 'field', field)
              Vue.set(that.results.fields[i], 'status', 'ok')
              Vue.set(that.results.fields[i], 'edited', edited)
              Vue.set(that.results.fields[i], 'data', data)
            })
          })
        }
      }
    },
    addConstraint: function (c) {
      this.query.constraints = this.query.constraints.filter(cons => {
        if (cons.field.widget === 'date_range' && cons.field.id === c.field.id) return false
        return true
      })
      this.query.constraints.push(c)
      this.editedConstraint = c
      this.refreshObservationGroups()
    },
    removeConstraint: function (c) {
      this.query.constraints = this.query.constraints.filter(cons => {
        if (cons.field.id === c.field.id && cons.expression === c.expression && cons.operation === c.operation) return false
        return true
      })
      this.refreshObservationGroups()
    },
    constraintClicked: function (i) {
      this.editedConstraint = this.query.constraints[i]
      this.refreshObservationGroups()
    },
    constraintClosed: function (i) {
      this.query.constraints.splice(i, 1)
      this.refreshObservationGroups()
    },
    isEdited: function (c) {
      return this.editedConstraint && c.field.id === this.editedConstraint.field.id
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
    resultsFieldsToDisplay: function () {
      if (this.$store.state.SMT.status !== 'ready') {
        return []
      }
      let res = []
      for (let i in this.results.fields) {
        let rf = this.results.fields[i]
        if (!rf.field) continue
        if (this.isEdited(rf)) {
          res.push(rf)
          continue
        }
        // Don't display tags widget when only one value remains (already displayed as implicit constraints)
        if (rf.field.widget === 'tags' && rf.data && rf.data.filter(tag => tag.closable === false).length <= 1) continue
        // Don't display date range if the range is <= 24h
        if (rf.field.widget === 'date_range' && rf.data) {
          let dt = rf.data.max - rf.data.min
          if (dt <= 1000 * 60 * 60 * 24) continue
        }
        res.push(rf)
      }
      return res
    }
  },
  mounted: function () {
    this.refreshObservationGroups()
  },
  components: { SmtPanelRootToolbar, SmtField }
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
