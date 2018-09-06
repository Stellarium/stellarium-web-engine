// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

<div class="secondary" style="position:absolute; width: 100%; height: 100%;">
  <v-container style="width: 100%; height: 100%;">
    <v-layout column align-center style="width: 100%; height: 100%;">
      <div class="display-3"><img src="@/assets/images/logo.svg" width="92" height="92" alt="Stellarium Web Logo"/> Stellarium<sup>Web</sup></div>
      <div v-if="$store.state.wasmSupport" style="margin: auto;">
        <div style="display:flex; justify-content: center;">
          <p class="grey--text">Loading Stellarium<sup>Web</sup>, the online Planetarium</p>
        </div>
        <div style="display:flex; justify-content: center;">
          <v-progress-circular indeterminate v-bind:size="70" v-bind:width="7"  class="grey--text"/>
        </div>
      </div>
      <v-card v-else style="margin: auto;">
        <v-card-title primary-title>
          <div class="headline"><v-icon large>error</v-icon> It seems that your browser cannot load Web Assembly!</div>
        </v-card-title>
        <v-card-text>
          <v-layout column align-center style="width: 100%; height: 100%;">
            <p class="grey--text">Web assembly is necessary for Stellarium Web to display the night sky. Please upgrade your web browser and try again!</p>
            <p>In the meantime, you can try the <a href="https://stellarium.org">desktop version</a>, or read the project's <a href="/news">news</a>!</p>
          </v-layout>
        </v-card-text>
      </v-card>
    </v-layout>
  </v-container>
</div>

</template>

<script>
export default {
}
</script>

<style>
</style>
