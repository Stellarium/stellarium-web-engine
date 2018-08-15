// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
<v-card flat>
  <v-card-title>
      <v-flex xs10>
        Observation with
        <v-btn-toggle v-model="instanceId">
          <v-btn flat value="eyes_observation">
            Eyes
          </v-btn>
          <v-btn flat value="binoculars_observation">
            Binoculars
          </v-btn>
          <v-btn flat value="telescope_observation">
            Telescope
          </v-btn>
        </v-btn-toggle>
      </v-flex>
      <v-flex xs2>
        <v-btn @click.native.stop="onClickOK()">OK</v-btn>
      </v-flex>
   </v-card-title>
   <v-divider></v-divider>
   <v-card-text style="height: 450px;">
     <equipment-details style="padding-left: 24px" :instance="returnedInstance" v-on:equipmentChanged="onSubEquipmentChanged"></equipment-details>
   </v-card-text>
</v-card>
</template>

<script>

import NoctuaSkyClient from '@/assets/noctuasky-client'
import EquipmentDetails from './equipment-details.vue'

export default {
  components: { EquipmentDetails },
  data: function () {
    return {
      returnedInstance: this.instance
    }
  },
  props: {
    'instance': { default: 'eyes_observation' }
  },
  methods: {
    onSubEquipmentChanged: function (index, inst) {
      this.returnedInstance = inst
    },
    onClickOK: function () {
      this.$emit('change', this.returnedInstance)
    }
  },
  watch: {
    instance: function (newValue) {
      this.returnedInstance = newValue
    }
  },
  computed: {
    observingSetupDescription: function () {
      return NoctuaSkyClient.equipments.get(this.instanceId).description
    },
    instanceId: {
      get: function () {
        return this.returnedInstance.id ? this.returnedInstance.id : this.returnedInstance
      },
      set: function (v) {
        this.returnedInstance = v
      }
    }
  }
}
</script>

<style>
</style>
