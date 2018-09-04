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

export default {
  data: function () {
    return {
      obsSkySource: undefined
    }
  },
  watch: {
    obsSkySource: function (v) {
      if (!v) {
        return
      }
      if (v.model === 'tle_satellite') {
        v.id = 'NORAD ' + v.model_data.norad_number
      } else {
        v.id = v.names[0]
      }
      // First check if the object already exists in SWE
      let ss = this.$stel.getObjByNSID(v.nsid)
      if (!ss) {
        ss = this.$stel.getObj(v.id)
      }
      if (ss) {
        ss.update()
      } else {
        ss = this.$stel.createObj(v.model, v)
        ss.update()
        this.$selectionLayer.add(ss)
      }
      this.$store.commit('setSelectedObject', v)
      this.$stel.core.selection = ss

      this.$stel.core.lock = ss
      this.$stel.core.lock.update()
      this.$stel.core.lookat(this.$stel.core.lock.azalt, 1.0)
    }
  },
  components: { SkysourceSearch }
}
</script>

<style>
@media all and (min-width: 600px) {
  .tsearch {
    z-index: 2;
    margin-left: -200px;
  }
}
</style>
