// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
  <div class="tsearch">
    <skysource-search v-model="obsSkySource" floatingList="true"></skysource-search>
  </div>
</template>

<script>
import SkysourceSearch from '@/components/skysource-search.vue'
import swh from '@/assets/sw_helpers.js'

export default {
  data: function () {
    return {
      obsSkySource: undefined
    }
  },
  watch: {
    obsSkySource: function (ss) {
      if (!ss) {
        return
      }
      let obj = swh.skySource2SweObj(ss)
      if (!obj) {
        obj = this.$stel.createObj(ss.model, ss)
        this.$selectionLayer.add(obj)
      }
      if (!obj) {
        console.warning("Can't find object in SWE: " + ss.names[0])
        return
      }
      swh.setSweObjAsSelection(obj)
    }
  },
  components: { SkysourceSearch }
}
</script>

<style>
@media all and (min-width: 600px) {
  .tsearch {
    z-index: 2;
  }
}
</style>
