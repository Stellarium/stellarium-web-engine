// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

<template>

<v-row no-gutters>
  <v-col cols="12">
    <h3 class="pt-3 line_right">{{ fieldDescription.name }}</h3>
  </v-col>
  <smt-field-chips v-if="isTags" :fieldResults="fieldResults" v-on:add-constraint="addConstraint"></smt-field-chips>
  <smt-field-date-range v-if="isDateRange" :fieldResults="fieldResults" v-on:add-constraint="addConstraint"></smt-field-date-range>
</v-row>

</template>

<script>
import SmtFieldChips from './smt-field-chips.vue'
import SmtFieldDateRange from './smt-field-date-range.vue'

export default {
  data: function () {
    return {
    }
  },
  props: ['fieldDescription', 'fieldResults'],
  methods: {
    addConstraint: function (c) {
      this.$emit('add-constraint', c)
    }
  },
  computed: {
    isTags: function () {
      return this.fieldDescription && this.fieldDescription.widget === 'tags' && this.fieldResults
    },
    isDateRange: function () {
      return this.fieldDescription && this.fieldDescription.widget === 'date_range' && this.fieldResults
    }
  },
  components: { SmtFieldChips, SmtFieldDateRange }
}
</script>

<style>
.line_right {
  overflow: hidden;
}

.line_right:after {
  background-color: #444;
  content: "";
  display: inline-block;
  height: 1px;
  position: relative;
  vertical-align: middle;
  width: 100%;
}

.line_right:after {
  left: 20px;
  margin-right: -100%;
}
</style>
