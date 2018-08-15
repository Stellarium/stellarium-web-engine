// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

import _ from 'lodash'
import { swh } from '@/assets/sw_helpers.js'

export const nsh = {
  iconForObservingSetup: function (obsSetup) {
    if (!obsSetup) {
      return '/static/images/svg/equipments/unknown.svg'
    }
    switch (obsSetup.id ? obsSetup.id : obsSetup) {
      case 'binoculars_observation':
        return '/static/images/svg/equipments/binoculars.svg'
      case 'telescope_observation':
        return '/static/images/svg/equipments/telescope.svg'
      case 'eyes_observation':
        return '/static/images/svg/equipments/eyes.svg'
    }
    return '/static/images/svg/equipments/unknown.svg'
  },

  titleForObservingSetup: function (obsSetup) {
    if (!obsSetup) {
      return 'Unknown'
    }
    var obs = this.expandEquipmentInstance(obsSetup)
    var modelId
    if (obs.id === 'eyes_observation') {
      modelId = 'eyes'
    } else if (typeof obs.state.model === 'string') {
      modelId = obs.state.model
    } else {
      modelId = obs.state.model.id
    }
    var manuf = nsh.equipmentForId(modelId).manufacturer
    if (manuf === 'Generic') {
      return nsh.equipmentForId(modelId).name
    }
    return nsh.equipmentForId(modelId).manufacturer + ' - ' + nsh.equipmentForId(modelId).name
  },

  iconForObservation: function (obs) {
    if (obs && obs.target) {
      return swh.iconForSkySource(obs.target)
    } else {
      return swh.iconForSkySourceTypes(['reg'])
    }
  },

  equipments: [
    {
      id: 'none'
    },
    {
      id: 'base',
      schema: [
        {id: 'manufacturer', name: 'Manufacturer', type: 'text'},
        {id: 'name', name: 'Name', type: 'text'},
        {id: 'description', name: 'Description', type: 'text'}
      ]
    },
    {
      id: 'eyes',
      inherits: 'base',
      manufacturer: 'Generic',
      name: 'Naked Eyes',
      description: 'Human naked eyes'
    },
    {
      id: 'eyepiece',
      inherits: 'base',
      schema: [
        {id: 'focal_length', name: 'Focal Length', type: 'number', min: 10, max: 500, step: 1, unit: 'mm', defaultValue: 25},
        {id: 'afov', name: 'Apparent FOV', type: 'number', min: 1, max: 120, step: 1, unit: '°', defaultValue: 50}
      ]
    },
    {
      id: 'meade_ma9mm',
      inherits: 'eyepiece',
      manufacturer: 'Meade',
      name: 'MA 9mm',
      description: 'Modified Achromatic',
      focal_length: 9,
      afov: 40
    },
    {
      id: 'eyepiece_custom',
      inherits: 'eyepiece',
      manufacturer: 'Generic',
      name: 'Custom Eyepiece'
    },
    {
      id: 'barlow',
      inherits: 'base',
      schema: [
        {id: 'multiplier', name: 'Magnification Factor', type: 'number', min: 0.1, max: 10, step: 0.1, unit: 'x', defaultValue: 1}
      ]
    },
    {
      id: 'barlow_none',
      inherits: 'barlow',
      manufacturer: 'Generic',
      name: 'No Barlow',
      multiplier: 1.0
    },
    {
      id: 'barlow_custom',
      inherits: 'barlow',
      manufacturer: 'Generic',
      name: 'Custom Barlow'
    },
    {
      id: 'binoculars',
      inherits: 'base',
      schema: [
        {id: 'magnification', name: 'Magnification', type: 'number', min: 1, max: 30, step: 1, unit: 'x', defaultValue: 7},
        {id: 'aperture', name: 'Aperture', type: 'number', min: 10, max: 500, step: 1, unit: 'mm', defaultValue: 50},
        {id: 'fov', name: 'FOV', type: 'number', min: 0.1, max: 60, step: 0.1, unit: '°', defaultValue: 5}
      ]
    },
    {
      id: 'binoculars_custom',
      inherits: 'binoculars',
      manufacturer: 'Generic',
      name: 'Custom Binoculars'
    },
    {
      id: 'paralux_atlas_10x50',
      inherits: 'binoculars',
      manufacturer: 'Paralux',
      name: 'Atlas 10x50',
      magnification: 10,
      aperture: 50,
      fov: 6.98
    },
    {
      id: 'optical_tube',
      inherits: 'base',
      schema: [
        {id: 'aperture', name: 'Aperture', type: 'number', min: 10, max: 10000, step: 1, unit: 'mm', defaultValue: 114},
        {id: 'focal_length', name: 'Focal Length', type: 'number', min: 1, max: 50000, step: 1, unit: 'mm', defaultValue: 900}
      ]
    },
    {
      id: 'optical_tube_custom',
      inherits: 'optical_tube',
      manufacturer: 'Generic',
      name: 'Custom Optical Tube'
    },
    {
      id: 'optical_tube_meade_114_900_EQ1_B',
      inherits: 'optical_tube',
      manufacturer: 'Meade',
      name: '114/900 EQ1-B',
      type: 'Newtonian Reflector',
      aperture: 114,
      focal_length: 900
    },
    {
      id: 'optical_tube_skywatcher_MC_90_1250_skymax',
      inherits: 'optical_tube',
      manufacturer: 'Sky-Watcher',
      name: 'MC 90/1250 SkyMax',
      type: 'Maksutov-Cassegrain Reflector',
      aperture: 90,
      focal_length: 1250
    },
    {
      id: 'mount',
      inherits: 'base'
    },
    {
      id: 'mount_generic',
      inherits: 'mount',
      manufacturer: 'Generic',
      name: 'Generic Mount'
    },
    {
      id: 'mount_meade_EQ1_B',
      inherits: 'mount',
      manufacturer: 'Meade',
      name: 'EQ1-B'
    },
    {
      id: 'mount_skywatcher_star_discovery',
      inherits: 'mount',
      manufacturer: 'Sky-Watcher',
      name: 'Star Discovery'
    },
    {
      id: 'telescope_visual',
      inherits: 'base',
      schema: [
        {id: 'optical_tube', name: 'Optical Tube', type: 'equipment', choiceFilter: function (e) { return e.inherits === 'optical_tube' }, defaultValue: 'optical_tube_custom'},
        {id: 'eyepiece', name: 'Eyepiece', type: 'equipment', choiceFilter: function (e) { return e.inherits === 'eyepiece' }, defaultValue: 'eyepiece_custom'},
        {id: 'barlow', name: 'Barlow/Reducer', type: 'equipment', choiceFilter: function (e) { return e.inherits === 'barlow' }, defaultValue: 'barlow_none'},
        {id: 'mount', name: 'Mount', type: 'equipment', choiceFilter: function (e) { return e.inherits === 'mount' }, defaultValue: 'mount_generic'},
        {id: 'fov', name: 'FOV', type: 'function'}
      ]
    },
    {
      id: 'telescope_meade_114_900_EQ1_B',
      inherits: 'telescope_visual',
      optical_tube: 'optical_tube_meade_114_900_EQ1_B',
      mount: 'mount_meade_EQ1_B',
      manufacturer: 'Meade',
      name: '114/900 EQ1-B'
    },
    {
      id: 'telescope_visual_custom',
      inherits: 'telescope_visual',
      manufacturer: 'Generic',
      name: 'Custom Telescope'
    },
    {
      id: 'observation',
      schema: [
        {id: 'footprint', name: 'Footprint', type: 'function'}
      ]
    },
    {
      id: 'eyes_observation',
      inherits: 'observation',
      name: 'Naked Eyes',
      description: 'Observation with naked eyes',
      footprint: function (input) { return ['CAP', [input.ra, input.de], 60] },
      model: 'eyes'
    },
    {
      id: 'binoculars_observation',
      inherits: 'observation',
      name: 'Binoculars',
      description: 'Observation with binoculars',
      footprint: function (input) { return ['CAP', [input.ra, input.de], this.model.fov] },
      schema: [
        {id: 'model', name: 'Binoculars Model', type: 'equipment', choiceFilter: function (e) { return e.inherits === 'binoculars' }, defaultValue: 'binoculars_custom'}
      ]
    },
    {
      id: 'telescope_observation',
      inherits: 'observation',
      name: 'Telescope Observation (Visual)',
      description: 'Visual observation through a telescope',
      fov: function (input) { return this.model.eyepiece.afov / (this.model.optical_tube.focal_length / this.model.eyepiece.focal_length) / this.model.barlow.multiplier },
      footprint: function (input) { return ['CAP', [input.ra, input.de], this.fov()] },
      schema: [
        {id: 'model', name: 'Telescope Model', type: 'equipment', choiceFilter: function (e) { return e.inherits === 'telescope_visual' }, defaultValue: 'telescope_visual_custom'},
        {id: 'fov', name: 'FOV', type: 'function'}
      ]
    }
  ],
  // Get the equipment class for an equipment ID
  equipmentForId: function (id) {
    return this.equipments.find(function (l) { return l.id === id })
  },
  // Construct an instance of an equipment with class id
  // It returns an instance with a flattened schema and an initial state
  // containing default values
  constructEquipmentInstance: function (id) {
    if (!id) {
      return []
    }
    var that = this
    var getSchemaRecursive = function myself (eq) {
      var sc = (eq && eq.schema) ? eq.schema : []
      if (eq && eq.inherits) {
        return myself(that.equipmentForId(eq.inherits)).concat(sc)
      } else {
        return sc
      }
    }
    var getValueRecursive = function myself2 (key, eq) {
      if (eq && eq[key]) {
        return eq[key]
      }
      if (eq && eq.inherits) {
        return myself2(key, that.equipmentForId(eq.inherits))
      }
      return undefined
    }

    var sch = _.cloneDeep(getSchemaRecursive(that.equipmentForId(id)))
    var _state = {}
    for (var i in sch) {
      sch[i]['index'] = parseInt(i)
      var val = getValueRecursive(sch[i].id, that.equipmentForId(id))
      if (val) {
        sch[i]['fixedValue'] = val
      } else {
        if (sch[i].defaultValue) {
          _state[sch[i].id] = sch[i].defaultValue
        }
      }
    }
    return {id: id, schema: sch, state: _state}
  },
  // Simplify the passed equipment instance by removing
  // the schema. It can be re-generated by using the
  // expandEquipmentInstance function.
  // Returned value is {id: 'equipment_class_id', state: { .. tree containing all non-default values .. }}
  // Note that if the instance has an empty state, i.e. it use only
  // default values inferred from the schema, only the id is returned, i.e.
  // returned value 'my_id' is equivalent to {id: 'my_id', state: {}}
  simplifyEquipmentInstance: function (eq) {
    if (!eq.id) {
      return eq
    }
    if (Object.keys(eq.state).length === 0) {
      return eq.id
    }
    var res = {id: eq.id, state: eq.state}
    return JSON.parse(JSON.stringify(res))
  },
  // Do the reverse operation as what is done in simplifyEquipmentInstance
  expandEquipmentInstance: function (eq, includeFixedValues) {
    var inst = this.constructEquipmentInstance(eq.id ? eq.id : eq)
    _.merge(inst.state, eq.state ? eq.state : {})

    if (includeFixedValues) {
      for (let i in inst.schema) {
        let field = inst.schema[i]
        if (field.fixedValue) {
          inst.state[field.id] = field.fixedValue
        }
        if (field.type === 'equipment') {
          inst.state[field.id] = this.expandEquipmentInstance(inst.state[field.id], includeFixedValues)
        }
      }
      _.merge(inst, inst.state)
      delete inst.state
      delete inst.schema
    }
    return inst
  },
  fullEquipmentInstanceState: function (eq) {
    var inst = this.constructEquipmentInstance(eq.id ? eq.id : eq)
    _.merge(inst.state, eq.state ? eq.state : {})

    for (let i in inst.schema) {
      let field = inst.schema[i]
      if (field.fixedValue) {
        inst.state[field.id] = field.fixedValue
      }
      if (field.type === 'equipment') {
        inst.state[field.id] = this.fullEquipmentInstanceState(inst.state[field.id])
      }
    }
    _.merge(inst, inst.state)
    delete inst.state
    delete inst.schema
    return inst
  }
}
