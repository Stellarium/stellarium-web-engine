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
    <v-chip small class="white--text ma-1" :close="tag.closable" :color="tag.closable ? 'primary' : 'secondary'" v-for="(tag, i) in fieldResultsData" :key="i" @click="chipClicked(tag.name)" @click:close="chipClosed(tag.name)">
      <span v-if="tag.name === '__undefined'"><i>Undefined</i></span><span v-else>{{ tag.name }}</span>&nbsp;<span :class="tag.closable ? 'white--text' : 'primary--text'"> ({{ tag.count }})</span>
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
      const constraint = { field: this.fieldResults.field, operation: (name === '__undefined' ? 'IS_UNDEFINED' : 'STRING_EQUAL'), expression: name, negate: false }
      this.$emit('add-constraint', constraint)
    },
    chipClosed: function (name) {
      const constraint = { field: this.fieldResults.field, operation: (name === '__undefined' ? 'IS_UNDEFINED' : 'STRING_EQUAL'), expression: name, negate: false }
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
