// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>

  <v-col cols="12">
    <v-chip small class="white--text ma-1" :close="tag.closable" :color="tag.closable ? 'primary' : 'secondary'" v-for="(tag, i) in fieldResultsData" :key="i" @click="chipClicked(tag.name)" @click:close="chipClosed(tag.name)">
      {{ tag.name }}&nbsp;<span :class="tag.closable ? 'white--text' : 'primary--text'"> ({{ tag.count }})</span>
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
      if (this.fieldResults.data.filter(tag => tag.name === name && tag.closable).length > 0) return
      const constraint = { field: this.fieldResults.field, operation: (name === 'undefined' ? 'IS_UNDEFINED' : 'STRING_EQUAL'), expression: name, negate: false }
      this.$emit('add-constraint', constraint)
    },
    chipClosed: function (name) {
      const constraint = { field: this.fieldResults.field, operation: (name === 'undefined' ? 'IS_UNDEFINED' : 'STRING_EQUAL'), expression: name, negate: false }
      this.$emit('remove-constraint', constraint)
    }
  },
  computed: {
    fieldResultsData: function () {
      if (this.fieldResults) {
        return this.fieldResults.data
      }
      return []
    }
  }
}
</script>

<style>
</style>
