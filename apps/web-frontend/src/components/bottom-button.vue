// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <div class="bottom-button" :class="{on: toggled}">
    <a v-on:click='clicked'>
      <img :src="img" :alt="img_alt"/>
    </a>
    <div class="hint">{{label}}</div>
  </div>
</template>

<style>
  .bottom-button {
    width: 42px;
    height: 42px;
    position: relative;
    display: inline-block;
    user-select: none;
  }
  .bottom-button img {
    width: 100%;
    height: 100%;
    filter: opacity(0.5);
  }
  .bottom-button.on img {
    filter: opacity(1);
  }
  .bottom-button .hint {
    display: none;
    position: absolute;
    bottom: 100%;
    color: white;
    width: 200px;
  }
  .bottom-button:hover .hint {
    display: block;
  }
</style>

<script>
export default {
  name: 'bottom-button',
  props: ['label', 'img', 'toggled', 'img_alt'],
  methods: {
    clicked: function () {
      var b = !this.toggled
      this.$emit('clicked', b)
    }
  }
}
</script>
