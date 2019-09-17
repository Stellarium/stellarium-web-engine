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
          <v-flex xs12 v-for="(field,i) in $smt.fieldsList" :key="field.id">
            {{ field.name }}
            {{ results.fields[i] }}
          </v-flex>
        </v-layout>
      </v-container>
    </div>
  </div>
</template>

<script>
import SmtPanelRootToolbar from './smt-panel-root-toolbar.vue'
import alasql from 'alasql'

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
          console.log(value)
          accumulator[value] = (accumulator[value] !== undefined) ? accumulator[value] + 1 : 1
          return accumulator
        } else if (stage === 3) {
          return accumulator
        }
      }

      for (let i in that.$smt.fieldsList) {
        let field = that.$smt.fieldsList[i]
        let fid = that.fId2AlaSql(field.id)
        alasql.promise('SELECT VALUES_AND_COUNT(' + fid + ') AS tags FROM features').then(res => {
          console.log(fid + ': ' + i + ' ')
          console.log(res)
          that.results.fields[i] = res[0].tags
        })
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
  components: { SmtPanelRootToolbar }
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
