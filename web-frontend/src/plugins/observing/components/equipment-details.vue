// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

  <v-container fluid grid-list-md>
    <v-layout row wrap>
      <template v-for="field in equipmentInstance.schema">
        <template v-if="(field.type==='number' || field.type==='text') && field.id!=='description' && field.id!=='name' && field.id!=='manufacturer'">
          <v-flex xs3 justify-end style="color: #dddddd; min-height: 46px; border-radius: 2px;"><div style="padding-top: 12px; padding-left: 35px">{{ field.name }}</div></v-flex>
          <v-flex xs3 v-if="field.fixedValue" style="font-weight: 500"><div style="align-items: center; padding: 12px 16px; background: #484848;">{{ field.fixedValue }}{{ field.unit }}</div></v-flex>
          <v-flex xs3 v-else>
            <v-text-field light solo :type="field.type" :value="equipmentInstance.state[field.id]" :suffix="field.unit" @input="onChangeValue($event, field.index)"></v-text-field>
          </v-flex>
        </template>
        <template v-if="field.type==='equipment'">
          <v-flex xs3><div style="border-style: solid; border-width: 1px 0px 1px 0px; border-color: #525252; font-size: 16px; height: 49px; padding-top: 11px; padding-left: 10px; background-color: #484848; color: #dddddd">{{ field.name }}</div></v-flex>
          <v-flex xs9>
            <v-select editable :disabled="field.fixedValue !== undefined" :items="choiceForEquipmentFilter(field)" item-text="name" item-value="val" @change="onChangeEquipment" :value="valueForField(field)"></v-select>
          </v-flex>
          <v-flex xs1></v-flex>
          <v-flex xs11 style="margin-top: -25px; margin-bottom: 25px;">
            <equipment-details :index="field.index" :instance="field.fixedValue ? field.fixedValue : equipmentInstance.state[field.id]" v-on:equipmentChanged="onSubEquipmentChanged"></equipment-details>
          </v-flex>
        </template>
      </template>
    </v-layout>
  </v-container>

</template>

<script>

import NoctuaSkyClient from '@/assets/noctuasky-client'
import EquipmentDetails from './equipment-details.vue'
import _ from 'lodash'

export default {
  name: 'EquipmentDetails',
  components: { EquipmentDetails },
  data: function () {
    return {
      equipmentInstance: NoctuaSkyClient.equipments.expandEquipmentInstance(this.instance)
    }
  },
  props: {
    'instance': { default: 'none' },
    'index': {default: 0}
  },
  methods: {
    selectItemForEquipment: function (field, e) {
      return { name: (e.manufacturer !== 'Generic') ? e.manufacturer + ' - ' + e.name : e.name, val: JSON.stringify({ id: field.id, index: field.index, val: e.id }) }
    },
    valueForField: function (field) {
      if (field.fixedValue) {
        return this.selectItemForEquipment(field, NoctuaSkyClient.equipments.get(field.fixedValue))
      }
      if (!this.equipmentInstance.state[field.id]) {
        return undefined
      }
      return this.selectItemForEquipment(field, NoctuaSkyClient.equipments.get(this.equipmentInstance.state[field.id].id ? this.equipmentInstance.state[field.id].id : this.equipmentInstance.state[field.id]))
    },
    choiceForEquipmentFilter: function (field) {
      if (!field.choiceFilter) {
        return []
      }
      var that = this
      var f = function (e) { return that.selectItemForEquipment(field, e) }
      return NoctuaSkyClient.state.equipments.filter(field.choiceFilter).map(f)
    },
    onChangeEquipment: function (v) {
      var p = JSON.parse(v)
      var tmp = _.cloneDeep(this.equipmentInstance)
      tmp.state[tmp.schema[p.index].id] = p.val
      this.equipmentInstance = tmp
      this.$emit('equipmentChanged', this.index, NoctuaSkyClient.equipments.simplifyEquipmentInstance(this.equipmentInstance))
    },
    onChangeValue: function (v, idx) {
      if (this.equipmentInstance.schema[idx].type === 'number') {
        v = +v
      }
      this.equipmentInstance.state[this.equipmentInstance.schema[idx].id] = v
      this.$emit('equipmentChanged', this.index, NoctuaSkyClient.equipments.simplifyEquipmentInstance(this.equipmentInstance))
    },
    onSubEquipmentChanged: function (idx, inst) {
      // Called when a sub equipment change it's state
      this.equipmentInstance.state[this.equipmentInstance.schema[idx].id] = {id: inst.id, state: inst.state}
      this.$emit('equipmentChanged', this.index, NoctuaSkyClient.equipments.simplifyEquipmentInstance(this.equipmentInstance))
    }
  },
  watch: {
    instance: function (newValue) {
      this.equipmentInstance = NoctuaSkyClient.equipments.expandEquipmentInstance(newValue)
    }
  },
  computed: {
  }
}
</script>

<style>

</style>
