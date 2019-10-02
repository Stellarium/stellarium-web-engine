// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>

  <v-col cols="12">
    <v-chip small class="white--text ma-1" color="secondary" v-for="(count, name) in fieldResultsData" :key="name" @click="chipClicked(name)">
      {{ name }}&nbsp;<span class="primary--text"> ({{ count }})</span>
    </v-chip>
  </v-col>

</template>

<script>

export default {
  data: function () {
    return {
    }
  },
  props: ['fieldResults'],
  methods: {
    chipClicked: function (name) {
      let constraint = { 'field': this.fieldResults.field, 'operation': (name === 'undefined' ? 'IS_UNDEFINED' : 'STRING_EQUAL'), 'expression': name, 'negate': false }
      this.$emit('add-constraint', constraint)
    }
  },
  computed: {
    fieldResultsData: function () {
      if (this.fieldResults) {
        return this.fieldResults.data
      }
      return {}
    }
  }
}
</script>

<style>
</style>
