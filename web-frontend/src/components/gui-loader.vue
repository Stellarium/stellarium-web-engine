// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

<div class="secondary" style="position:absolute; width: 100%; height: 100%; display:flex; justify-content: center; align-items: center;">
  <div>
    <div style="display:flex; justify-content: center;">
      <p class="grey--text">Loading planetarium</p>
    </div>
    <div style="display:flex; justify-content: center;">
      <v-progress-circular indeterminate v-bind:size="70" v-bind:width="7"  class="grey--text"/>
    </div>
  </div>
</div>

</template>

<script>
export default {
}
</script>

<style>
</style>
