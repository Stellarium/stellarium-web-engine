// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>

<v-flex xs12>
  <h3 class="pt-3">{{ fieldDescription.name }}</h3>
  <div v-if="isTags">
    <v-chip small class="white--text" color="secondary" v-for="(count, name) in fieldResults.data" :key="name">
      {{ name }}&nbsp;<span class="primary--text"> ({{ count }})</span>
    </v-chip>
  </div>
  <div v-if="isDateRange">
  {{ fieldResults }}
<!--    <v-sparkline
          :labels="fieldResults.data.labels"
          :value="fieldResults.data.values"
          color="white"
          line-width="2"
          padding="16"></v-sparkline>-->
  </div>
</v-flex>

</template>

<script>

export default {
  data: function () {
    return {
    }
  },
  props: ['fieldDescription', 'fieldResults'],
  methods: {
  },
  computed: {
    isTags: function () {
      return this.fieldDescription && this.fieldDescription.widget === 'tags' && this.fieldResults
    },
    isDateRange: function () {
      return this.fieldDescription && this.fieldDescription.widget === 'date_range' && this.fieldResults
    }
  }
}
</script>

<style>
</style>
