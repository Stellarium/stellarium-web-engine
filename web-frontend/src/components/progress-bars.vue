// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <div>
    <div class="tfaders" v-for="bar in progressBars" v-bind:key="bar.id">
      <transition name="fade">
        <div class="tfader" v-if="bar.value != bar.total">
          <span class="text-caption" style="right: 4px; position: relative;">{{ bar.label }}</span>
          <v-progress-circular :rotate="-90" size=18 :value="bar.value / bar.total * 100"></v-progress-circular>
        </div>
      </transition>
    </div>
  </div>
</template>

<script>

export default {
  name: 'progress-bars',
  data: function () {
    return {
    }
  },
  computed: {
    progressBars: function () {
      return this.$store.state.stel.progressbars
    }
  }
}
</script>

<style>
.tfaders {
  text-align: right;
  opacity: 0.5;
  user-select: none;
}

.fade-enter-active, .fade-leave-active {
  transition: opacity .5s;
}
.fade-enter, .fade-leave-to {
  opacity: 0;
}
</style>
