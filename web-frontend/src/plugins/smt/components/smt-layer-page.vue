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
  <div style="height: 100%; display: flex; flex-flow: column;">
    <img :src="watermarkImage" style="position: fixed; left: 5px; bottom: 5px; opacity: 0.7;"></img>
    <smt-selection-info v-if="selectedFootprintData !== undefined" :selectionData="selectedFootprintData" @unselect="unselect()"></smt-selection-info>
    <smt-panel-root-toolbar></smt-panel-root-toolbar>
    <img v-if="dataLoadingImage && $store.state.SMT.status === 'loading'" :src="dataLoadingImage" style="position: absolute; bottom: calc(50% - 100px); right: 80px;"></img>
    <v-progress-circular v-if="query.refreshObservationsInSkyInProgress" size=160 width=10 indeterminate style="position: absolute; top: calc(50vh - 80px); right: calc(50vw + 200px - 80px); opacity: 0.2"></v-progress-circular>
    <v-card tile>
      <v-card-text>
        <div v-if="$store.state.SMT.status === 'ready'" class="display-1 text--primary"><v-progress-circular v-if="results.summary.count === undefined" size=18 indeterminate></v-progress-circular>{{ results.summary.count }} items</div>
        <div v-if="$store.state.SMT.status === 'loading'" class="display-1 text--primary">Loading data..</div>
        <div v-if="$store.state.SMT.status === 'ready' && constraintsToDisplay.length" class="mt-2">Constraints:</div>
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
    <div class="scroll-container">
      <v-container class="pa-0" fluid style="height: 100%">
        <v-container>
          <smt-field class="mb-2" v-for="fr in resultsFieldsToDisplay" :key="fr.field.id" :fieldDescription="fr.field" :fieldResults="fr" v-on:add-constraint="addConstraint" v-on:remove-constraint="removeConstraint" v-on:constraint-live-changed="constraintLiveChanged">
          </smt-field>
        </v-container>
      </v-container>
    </div>
  </div>
</template>

<script>
import SmtPanelRootToolbar from './smt-panel-root-toolbar.vue'
import SmtField from './smt-field.vue'
import SmtSelectionInfo from './smt-selection-info.vue'
import Vue from 'vue'
import Moment from 'moment'
import stringHash from 'string-hash'
import qe from '../query-engine'
import _ from 'lodash'

const turboSrgb = [[48, 18, 59], [50, 21, 67], [51, 24, 74], [52, 27, 81], [53, 30, 88], [54, 33, 95], [55, 36, 102], [56, 39, 109], [57, 42, 115], [58, 45, 121], [59, 47, 128], [60, 50, 134], [61, 53, 139], [62, 56, 145], [63, 59, 151], [63, 62, 156], [64, 64, 162], [65, 67, 167], [65, 70, 172], [66, 73, 177], [66, 75, 181], [67, 78, 186], [68, 81, 191], [68, 84, 195], [68, 86, 199], [69, 89, 203], [69, 92, 207], [69, 94, 211], [70, 97, 214], [70, 100, 218], [70, 102, 221], [70, 105, 224], [70, 107, 227], [71, 110, 230], [71, 113, 233], [71, 115, 235], [71, 118, 238], [71, 120, 240], [71, 123, 242], [70, 125, 244], [70, 128, 246], [70, 130, 248], [70, 133, 250], [70, 135, 251], [69, 138, 252], [69, 140, 253], [68, 143, 254], [67, 145, 254], [66, 148, 255], [65, 150, 255], [64, 153, 255], [62, 155, 254], [61, 158, 254], [59, 160, 253], [58, 163, 252], [56, 165, 251], [55, 168, 250], [53, 171, 248], [51, 173, 247], [49, 175, 245], [47, 178, 244], [46, 180, 242], [44, 183, 240], [42, 185, 238], [40, 188, 235], [39, 190, 233], [37, 192, 231], [35, 195, 228], [34, 197, 226], [32, 199, 223], [31, 201, 221], [30, 203, 218], [28, 205, 216], [27, 208, 213], [26, 210, 210], [26, 212, 208], [25, 213, 205], [24, 215, 202], [24, 217, 200], [24, 219, 197], [24, 221, 194], [24, 222, 192], [24, 224, 189], [25, 226, 187], [25, 227, 185], [26, 228, 182], [28, 230, 180], [29, 231, 178], [31, 233, 175], [32, 234, 172], [34, 235, 170], [37, 236, 167], [39, 238, 164], [42, 239, 161], [44, 240, 158], [47, 241, 155], [50, 242, 152], [53, 243, 148], [56, 244, 145], [60, 245, 142], [63, 246, 138], [67, 247, 135], [70, 248, 132], [74, 248, 128], [78, 249, 125], [82, 250, 122], [85, 250, 118], [89, 251, 115], [93, 252, 111], [97, 252, 108], [101, 253, 105], [105, 253, 102], [109, 254, 98], [113, 254, 95], [117, 254, 92], [121, 254, 89], [125, 255, 86], [128, 255, 83], [132, 255, 81], [136, 255, 78], [139, 255, 75], [143, 255, 73], [146, 255, 71], [150, 254, 68], [153, 254, 66], [156, 254, 64], [159, 253, 63], [161, 253, 61], [164, 252, 60], [167, 252, 58], [169, 251, 57], [172, 251, 56], [175, 250, 55], [177, 249, 54], [180, 248, 54], [183, 247, 53], [185, 246, 53], [188, 245, 52], [190, 244, 52], [193, 243, 52], [195, 241, 52], [198, 240, 52], [200, 239, 52], [203, 237, 52], [205, 236, 52], [208, 234, 52], [210, 233, 53], [212, 231, 53], [215, 229, 53], [217, 228, 54], [219, 226, 54], [221, 224, 55], [223, 223, 55], [225, 221, 55], [227, 219, 56], [229, 217, 56], [231, 215, 57], [233, 213, 57], [235, 211, 57], [236, 209, 58], [238, 207, 58], [239, 205, 58], [241, 203, 58], [242, 201, 58], [244, 199, 58], [245, 197, 58], [246, 195, 58], [247, 193, 58], [248, 190, 57], [249, 188, 57], [250, 186, 57], [251, 184, 56], [251, 182, 55], [252, 179, 54], [252, 177, 54], [253, 174, 53], [253, 172, 52], [254, 169, 51], [254, 167, 50], [254, 164, 49], [254, 161, 48], [254, 158, 47], [254, 155, 45], [254, 153, 44], [254, 150, 43], [254, 147, 42], [254, 144, 41], [253, 141, 39], [253, 138, 38], [252, 135, 37], [252, 132, 35], [251, 129, 34], [251, 126, 33], [250, 123, 31], [249, 120, 30], [249, 117, 29], [248, 114, 28], [247, 111, 26], [246, 108, 25], [245, 105, 24], [244, 102, 23], [243, 99, 21], [242, 96, 20], [241, 93, 19], [240, 91, 18], [239, 88, 17], [237, 85, 16], [236, 83, 15], [235, 80, 14], [234, 78, 13], [232, 75, 12], [231, 73, 12], [229, 71, 11], [228, 69, 10], [226, 67, 10], [225, 65, 9], [223, 63, 8], [221, 61, 8], [220, 59, 7], [218, 57, 7], [216, 55, 6], [214, 53, 6], [212, 51, 5], [210, 49, 5], [208, 47, 5], [206, 45, 4], [204, 43, 4], [202, 42, 4], [200, 40, 3], [197, 38, 3], [195, 37, 3], [193, 35, 2], [190, 33, 2], [188, 32, 2], [185, 30, 2], [183, 29, 2], [180, 27, 1], [178, 26, 1], [175, 24, 1], [172, 23, 1], [169, 22, 1], [167, 20, 1], [164, 19, 1], [161, 18, 1], [158, 16, 1], [155, 15, 1], [152, 14, 1], [149, 13, 1], [146, 11, 1], [142, 10, 1], [139, 9, 2], [136, 8, 2], [133, 7, 2], [129, 6, 2], [126, 5, 2], [122, 4, 3]]
var turboSrgbNormalized
const mapColor = function (v) {
  if (!turboSrgbNormalized) {
    turboSrgbNormalized = turboSrgb.map(c => [c[0] / 255, c[1] / 255, c[2] / 255]).map(c => { const m = Math.max(Math.max(c[0], c[1]), c[2]); return [c[0] / m, c[1] / m, c[2] / m] })
  }
  return turboSrgbNormalized[Math.floor(v * 255)]
}

export default {
  data: function () {
    return {
      query: {
        constraints: [],
        liveConstraint: undefined,
        count: 0,
        refreshObservationsInSkyInProgress: false
      },
      editedConstraint: undefined,
      results: {
        summary: {
          count: 0
        },
        fields: [],
        implicitConstraints: []
      },
      selectedFootprintData: undefined
    }
  },
  created: function () {
    this.geojsonObj = undefined
    this.livefilterData = []
  },
  methods: {
    thumbClicked: function (obs) {
      this.$router.push('/p/observations/' + obs.id)
    },
    formatDate: function (d) {
      return new Moment(d).format('YYYY-MM-DD')
    },
    printConstraint: function (c) {
      if (c.field.widget === 'date_range') return this.formatDate(c.expression[0]) + ' - ' + this.formatDate(c.expression[1])
      if (c.field.widget === 'number_range') return '' + c.expression[0] + ' - ' + c.expression[1]
      return c.expression
    },
    refreshObservationsInSky: function () {
      const that = this
      that.query.refreshObservationsInSkyInProgress = true
      const q2 = {
        constraints: this.query.constraints,
        qid: this.query.count
      }
      qe.queryVisual(q2).then(res => {
        that.geojsonObj = that.$observingLayer.add('geojson-survey', {
          path: 'http://localhost:3000/hips/' + res
          // Optional:
          // max_fov: 30,
          // min_fov: 10
        })
        that.refreshGeojsonLiveFilter()
        that.query.refreshObservationsInSkyInProgress = false
      })
    },
    refreshAllFields: function () {
      const that = this

      // Compute the WHERE clause to be used in following queries
      const queryConstraintsEdited = this.query.constraints.filter(c => !this.isEdited(c))
      const constraintsIds = that.query.constraints.map(c => c.field.id)

      // And recompute all fields
      for (const i in that.$smt.fields) {
        const field = that.$smt.fields[i]
        const edited = that.editedConstraint && that.editedConstraint.field.id === field.id
        if (field.widget === 'tags') {
          const q = {
            constraints: edited ? queryConstraintsEdited : this.query.constraints,
            groupingOptions: [{ operation: 'GROUP_ALL' }],
            aggregationOptions: [{ operation: 'VALUES_AND_COUNT', fieldId: field.id, out: 'tags' }]
          }
          qe.query(q).then(res => {
            let tags = res.res[0].tags ? res.res[0].tags : {}
            tags = Object.keys(tags).map(function (key) {
              const closable = that.query.constraints.filter(c => c.field.id === field.id && c.expression === key).length !== 0
              return { name: key, count: tags[key], closable: closable }
            })
            Vue.set(that.results.fields, i, {
              field: field,
              status: 'ok',
              edited: edited,
              data: tags
            })
            // Fill the implicit constraints list, i.e. the tags where only one value remains
            if (!constraintsIds.includes(field.id) && res.res[0].tags && Object.keys(res.res[0].tags).length === 1) {
              that.results.implicitConstraints.push({ field: field, expression: Object.keys(res.res[0].tags)[0], closable: false })
            }
          }, err => {
            console.log(err)
          })
        }
        if (field.widget === 'date_range') {
          const q = {
            constraints: this.query.constraints,
            groupingOptions: [{ operation: 'GROUP_ALL' }],
            aggregationOptions: [{ operation: 'DATE_HISTOGRAM', fieldId: field.id, out: 'dh' }]
          }
          qe.query(q).then(res => {
            Vue.set(that.results.fields[i], 'field', field)
            Vue.set(that.results.fields[i], 'status', 'ok')
            Vue.set(that.results.fields[i], 'edited', edited)
            Vue.set(that.results.fields[i], 'data', res.res[0].dh)
          })
        }
        if (field.widget === 'number_range') {
          const q = {
            constraints: this.query.constraints,
            groupingOptions: [{ operation: 'GROUP_ALL' }],
            aggregationOptions: [{ operation: 'NUMBER_HISTOGRAM', fieldId: field.id, out: 'nh' }]
          }
          qe.query(q).then(res => {
            Vue.set(that.results.fields[i], 'field', field)
            Vue.set(that.results.fields[i], 'status', 'ok')
            Vue.set(that.results.fields[i], 'edited', edited)
            Vue.set(that.results.fields[i], 'data', res.res[0].nh)
          })
        }
      }
    },
    refreshObservationGroups: function () {
      const that = this

      if (this.$store.state.SMT.status !== 'ready') {
        return
      }

      // Cleanup previous query and states
      this.liveConstraint = undefined
      that.livefilterData = []
      // Suppress previous geojson results
      if (that.geojsonObj) {
        that.$observingLayer.remove(that.geojsonObj)
        that.geojsonObj.destroy()
        that.geojsonObj = undefined
      }

      // Reset all fields values
      that.results.fields = that.$smt.fields.map(function (e) { return { status: 'loading', data: {} } })
      that.results.implicitConstraints = []

      this.query.count++
      const q1 = {
        constraints: this.query.constraints,
        groupingOptions: [{ operation: 'GROUP_ALL' }],
        aggregationOptions: [{ operation: 'COUNT', out: 'total' }]
      }
      that.results.summary.count = undefined
      qe.query(q1).then(res => {
        that.results.summary.count = res.res[0].total
      })
      that.refreshAllFields()
      that.refreshObservationsInSky()
    },
    refreshGeojsonLiveFilter: function () {
      const that = this
      if (!that.geojsonObj) return
      let colorAssignedSqlField
      if (that.$smt.colorAssignedField) {
        colorAssignedSqlField = qe.fId2AlaSql(that.$smt.colorAssignedField)
      }
      let liveConstraintSql
      const lc = that.liveConstraint
      if (lc && lc.field.widget === 'date_range' && lc.operation === 'DATE_RANGE') {
        liveConstraintSql = qe.fId2AlaSql(lc.field.id)
      }
      if (lc && lc.field.widget === 'number_range' && lc.operation === 'NUMBER_RANGE') {
        liveConstraintSql = qe.fId2AlaSql(lc.field.id)
      }
      that.geojsonObj.filter = function (feature) {
        // if (do_not_show) return false
        // if (do_not_modify) return true
        if (liveConstraintSql) {
          const v = _.get(feature.properties, liveConstraintSql)
          if (v === undefined || v < lc.expression[0] || v > lc.expression[1]) {
            return false
          }
        }

        if (feature.colorDone) return true
        let c = [1, 0.3, 0.3, 0.3]
        if (colorAssignedSqlField) {
          let cstring = _.get(feature.properties, colorAssignedSqlField)
          if (!cstring) cstring = ''
          c = mapColor(stringHash(cstring) / 4294967295)
          c[3] = 0.3
        }
        feature.colorDone = true
        return {
          fill: c,
          stroke: [1, 0, 0, 0],
          visible: true,
          blink: feature.selected
        }
      }
    },
    addConstraint: function (c) {
      this.query.constraints = this.query.constraints.filter(cons => {
        if (cons.field.widget === 'date_range' && cons.field.id === c.field.id) return false
        if (cons.field.widget === 'number_range' && cons.field.id === c.field.id) return false
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
    constraintLiveChanged: function (c) {
      this.liveConstraint = c
      this.refreshGeojsonLiveFilter()
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
    },
    unselect: function () {
      this.selectedFootprintData = undefined
    }
  },
  watch: {
    '$store.state.SMT.status': function () {
      this.refreshObservationGroups()
    },
    selectedFootprintData: function () {
      // refresh the geojson live filter to make the selected object blink
      const selectedIds = this.selectedFootprintData ? this.selectedFootprintData.map(e => e.id) : []
      for (const item of this.livefilterData) {
        const selected = selectedIds.includes(item.id)
        if (item.selected !== selected) {
          item.selected = selected
          item.colorDone = undefined
        }
      }
      this.refreshGeojsonLiveFilter()
    }
  },
  computed: {
    watermarkImage: function () {
      if (this.$store.state.SMT.watermarkImage) return process.env.BASE_URL + 'plugins/smt/data/' + this.$store.state.SMT.watermarkImage
      return ''
    },
    dataLoadingImage: function () {
      if (this.$store.state.SMT.dataLoadingImage) return process.env.BASE_URL + 'plugins/smt/data/' + this.$store.state.SMT.dataLoadingImage
      return ''
    },
    // Return real and implicit constraints to display in GUI
    constraintsToDisplay: function () {
      if (this.$store.state.SMT.status !== 'ready') {
        return []
      }

      let res = this.query.constraints.slice()
      for (const i in res) {
        res[i].closable = true
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
      const res = []
      for (const i in this.results.fields) {
        const rf = this.results.fields[i]
        if (!rf.field) continue
        if (this.isEdited(rf)) {
          res.push(rf)
          continue
        }
        // Don't display tags widget when only one value remains (already displayed as implicit constraints)
        if (rf.field.widget === 'tags' && rf.data && rf.data.filter(tag => tag.closable === false).length <= 1) continue
        // Don't display date range if the range is <= 24h
        if (rf.field.widget === 'date_range' && rf.data) {
          if (rf.data.max === undefined || rf.data.min === undefined) continue
          const dt = rf.data.max - rf.data.min
          if (dt <= 1000 * 60 * 60 * 24) continue
        }
        if (rf.field.widget === 'number_range' && rf.data) {
          if (rf.data.max === rf.data.min) continue
        }
        res.push(rf)
      }
      return res
    }
  },
  mounted: function () {
    this.refreshObservationGroups()
    const that = this
    // Manage geojson features selection
    that.$stel.on('click', e => {
      if (!that.geojsonObj) return false
      // Get the list of features indices at click position
      const res = that.geojsonObj.queryRenderedFeatureIds(e.point)
      if (!res.length) {
        that.selectedFootprintData = undefined
        return false
      }
      const ids = res.map(i => that.livefilterData[i].id)
      const q = {
        constraints: [{ field: { id: 'id', type: 'number' }, operation: 'IN', expression: ids, negate: false }],
        projectOptions: {
          id: 1,
          properties: 1
        }
      }
      qe.query(q).then(qres => {
        if (!qres.res.length) {
          that.selectedFootprintData = undefined
          return
        }
        that.selectedFootprintData = qres.res
      })
      return true
    })
  },
  components: { SmtPanelRootToolbar, SmtField, SmtSelectionInfo }
}
</script>

<style>
.scroll-container {
  flex: 1 1 auto;
  overflow-y: auto;
  backface-visibility: hidden;
}
ul {
  padding-left: 30px;
}
</style>
