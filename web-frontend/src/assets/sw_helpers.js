// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

import Vue from 'vue'
import _ from 'lodash'
import axios from 'axios'
import scriptjs from 'scriptjs'
import hex2dec from 'hex2dec'
import sweWasmModule from '@/assets/js/stellarium-web-engine.wasm'
import * as sweJsModule from '@/assets/js/stellarium-web-engine.js'

import NoctuaSkyClient from '@/assets/noctuasky-client'

var DDDate = Date
DDDate.prototype.getJD = function () {
  return (this.getTime() / 86400000) + 2440587.5
}

DDDate.prototype.setJD = function (jd) {
  this.setTime((jd - 2440587.5) * 86400000)
}

DDDate.prototype.getMJD = function () {
  return this.getJD() - 2400000.5
}

DDDate.prototype.setMJD = function (mjd) {
  this.setJD(mjd + 2400000.5)
}

export const swh = {
  initStelWebEngine: function (store, canvasElem, callBackOnDone) {
    let onNoctuaSkyStateChanged = function (path, value) {
      if (path === '') {
        store.commit('replaceNoctuaSkyState', value)
        return
      }
      let tree = store.state.noctuaSky
      _.set(tree, path, value)
      store.commit('replaceNoctuaSkyState', tree)
    }
    // Didn't find a better way to load stellarium-web-engine.js..
    // Note that "import StelWebEngine from '@/assets/js/stellarium-web-engine.js'" would work
    // if it was an ES6 module with "export default StelWebEngine"
    scriptjs(['https://cdnjs.cloudflare.com/ajax/libs/jquery/3.1.1/jquery.min.js', sweJsModule], function () {
      NoctuaSkyClient.init(process.env.NOCTUASKY_API_SERVER, onNoctuaSkyStateChanged).then(res => {
        // eslint-disable-next-line
        let lstel = StelWebEngine({
          wasmFile: sweWasmModule,
          canvas: canvasElem,
          res: ['http://stelladata.noctua-software.com/surveys/stars/info.json'],
          onReady: function () {
            store.commit('replaceStelWebEngine', lstel.getTree())
            lstel.onValueChanged(function (path, value) {
              let tree = store.state.stel
              _.set(tree, path, value)
              store.commit('replaceStelWebEngine', tree)
            })
            Vue.prototype.$stel = lstel
            Vue.prototype.$selectionLayer = lstel.createLayer({id: 'slayer', z: 50, visible: true})
            Vue.prototype.$observingLayer = lstel.createLayer({id: 'obslayer', z: 40, visible: true})
            callBackOnDone()
          }
        })
      })
    })
  },

  addSelectedObjectExtraButtons: function (bt) {
    for (let i in this.selectedObjectExtraButtons) {
      if (this.selectedObjectExtraButtons[i].id === bt.id) {
        return
      }
    }
    this.selectedObjectExtraButtons.push(bt)
  },

  selectedObjectExtraButtons: [
  ],

  monthNames: ['January', 'February', 'March', 'April', 'May', 'June',
    'July', 'August', 'September', 'October', 'November', 'December'
  ],

  astroConstants: {
    // Light time for 1 au in s
    ERFA_AULT: 499.004782,
    // Seconds per day
    ERFA_DAYSEC: 86400.0,
    // Days per Julian year
    ERFA_DJY: 365.25,
    // Astronomical unit in m
    ERFA_DAU: 149597870000
  },

  iconForSkySourceTypes: function (skySourceTypes) {
    // Array sorted by specificity, i.e. the most generic names at the end
    const iconForType = {
      // Stars
      'Pec?': 'star',
      '**?': 'double_star',
      '**': 'double_star',
      'V*': 'variable_star',
      'V*?': 'variable_star',
      '*': 'star',

      // Candidates
      'As?': 'group_of_stars',
      'SC?': 'group_of_galaxies',
      'Gr?': 'group_of_galaxies',
      'C?G': 'group_of_galaxies',
      'G?': 'galaxy',

      // Multiple objects
      'reg': 'region_defined_in_the_sky',
      'SCG': 'group_of_galaxies',
      'ClG': 'group_of_galaxies',
      'GrG': 'group_of_galaxies',
      'IG': 'interacting_galaxy',
      'PaG': 'pair_of_galaxies',
      'C?*': 'open_galactic_cluster',
      'Gl?': 'globular_cluster',
      'GlC': 'globular_cluster',
      'OpC': 'open_galactic_cluster',
      'Cl*': 'open_galactic_cluster',
      'As*': 'group_of_stars',
      'mul': 'multiple_objects',

      // Interstellar matter
      'PN?': 'planetary_nebula',
      'PN': 'planetary_nebula',
      'SNR': 'planetary_nebula',
      'SR?': 'planetary_nebula',
      'ISM': 'interstellar_matter',

      // Galaxies
      'PoG': 'part_of_galaxy',
      'QSO': 'quasar',
      'G': 'galaxy',

      'dso': 'deep_sky',

      // Solar System
      'Asa': 'artificial_satellite',
      'Moo': 'moon',
      'Sun': 'sun',
      'Pla': 'planet',
      'DPl': 'planet',
      'Com': 'comet',
      'MPl': 'minor_planet',
      'SSO': 'minor_planet'
    }
    for (let i in skySourceTypes) {
      if (skySourceTypes[i] in iconForType) {
        return '/static/images/svg/target_types/' + iconForType[skySourceTypes[i]] + '.svg'
      }
    }
    return '/static/images/svg/target_types/unknown.svg'
  },

  iconForSkySource: function (skySource) {
    return swh.iconForSkySourceTypes(skySource.types)
  },

  iconForObservation: function (obs) {
    if (obs && obs.target) {
      return this.iconForSkySource(obs.target)
    } else {
      return this.iconForSkySourceTypes(['reg'])
    }
  },

  cleanupOneSkySourceName: function (name) {
    const greek = {
      'alf': 'α',
      'bet': 'β',
      'gam': 'γ',
      'del': 'δ',
      'eps': 'ε',
      'zet': 'ζ',
      'eta': 'η',
      'tet': 'θ',
      'iot': 'ι',
      'kap': 'κ',
      'lam': 'λ',
      'mu': 'μ',
      'mu.': 'μ',
      'nu': 'ν',
      'nu.': 'ν',
      'xi': 'ξ',
      'xi.': 'ξ',
      'omi': 'ο',
      'pi': 'π',
      'pi.': 'π',
      'rho': 'ρ',
      'sig': 'σ',
      'tau': 'τ',
      'ups': 'υ',
      'phi': 'φ',
      'chi': 'χ',
      'psi': 'ψ',
      'ome': 'ω'
    }

    if (name.startsWith('* ')) {
      let ll = name.substring(2, 5).trim()
      if (ll in greek) {
        name = greek[ll] + name.substring(name[5] === '0' ? 6 : 5)
      } else {
        name = name.substring(2)
      }
    }

    if (name.startsWith('V* ')) {
      let ll = name.substring(3, 6).trim().toLowerCase()
      if (ll in greek) {
        name = greek[ll] + ' ' + name.substring(name[6] === '0' ? 7 : 6)
      } else {
        name = name.substring(3)
      }
    }
    name = name.replace(/^Cl /, '')
    name = name.replace(/^Cl\* /, '')
    name = name.replace(/^NAME /, '')
    name = name.replace(/^\*\* /, '')
    return name
  },

  sortedNamesForSkySource: function (skySource) {
    // Return a dict with 3 lists of cleaned up names sorted by importance
    if (!skySource || !skySource.names) {
      return ['?']
    }
    return skySource.names.map(this.cleanupOneSkySourceName)
  },

  nameForSkySource: function (skySource) {
    if (!skySource || !skySource.names) {
      return '?'
    }
    return this.cleanupOneSkySourceName(skySource.names[0])
  },

  nameForSkySourceType: function (skySourceType) {
    const nameForType = {
      '*': 'Star',
      '**': 'Double or multiple star',
      '**?': 'Physical Binary Candidate',
      '*i*': 'Star in double system',
      '*iA': 'Star in Association',
      '*iC': 'Star in Cluster',
      '*iN': 'Star in Nebula',
      '..?': 'Candidate objects',
      '?': 'Object of unknown nature',
      'AB*': 'Asymptotic Giant Branch Star (He-burning)',
      'AB?': 'Asymptotic Giant Branch Star candidate',
      'AG?': 'Possible Active Galaxy Nucleus',
      'AGN': 'Active Galaxy Nucleus',
      'ALS': 'Absorption Line system',
      'AM*': 'CV of AM Her type (polar)',
      'Ae*': 'Herbig Ae/Be star',
      'Ae?': 'Possible Herbig Ae/Be Star',
      'Al*': 'Eclipsing binary of Algol type (detached)',
      'Amo': 'Amor Asteroid',
      'Apo': 'Apollo Asteroid',
      'As*': 'Association of Stars',
      'As?': 'Possible Association of Stars',
      'Asa': 'Artifical Satellite',
      'Ate': 'Aten Asteroid',
      'Ati': 'Atira Asteroid',
      'BAL': 'Broad Absorption Line system',
      'BD*': 'Brown Dwarf (M<0.08solMass)',
      'BD?': 'Brown Dwarf Candidate',
      'BH?': 'Black Hole Candidate',
      'BL?': 'Possible BL Lac',
      'BLL': 'BL Lac - type object',
      'BNe': 'Bright Nebula',
      'BS*': 'Blue Straggler Star',
      'BS?': 'Candidate blue Straggler Star',
      'BY*': 'Variable of BY Dra type',
      'Be*': 'Be Star',
      'Be?': 'Possible Be Star',
      'BiC': 'Brightest galaxy in a Cluster (BCG)',
      'Bla': 'Blazar',
      'Bz?': 'Possible Blazar',
      'C*': 'Carbon Star',
      'C*?': 'Possible Carbon Star',
      'C?*': 'Possible (open) star cluster',
      'C?G': 'Possible Cluster of Galaxies',
      'CGG': 'Compact Group of Galaxies',
      'CGb': 'Cometary Globule',
      'CH*': 'Star with envelope of CH type',
      'CH?': 'Possible Star with envelope of CH type',
      'CV*': 'Cataclysmic Variable Star',
      'CV?': 'Cataclysmic Binary Candidate',
      'Ce*': 'Cepheid variable Star',
      'Ce?': 'Possible Cepheid',
      'Cl*': 'Cluster of Stars',
      'ClG': 'Cluster of Galaxies',
      'Cld': 'Cloud',
      'Com': 'Comet',
      'DLA': 'Damped Ly-alpha Absorption Line system',
      'DN*': 'Dwarf Nova',
      'DNe': 'Dark Cloud (nebula)',
      'DOA': 'Distant Object Asteroid',
      'DPl': 'Dwarf Planet',
      'DQ*': 'CV DQ Her type (intermediate polar)',
      'EB*': 'Eclipsing binary',
      'EB?': 'Eclipsing Binary Candidate',
      'EP*': 'Star showing eclipses by its planet',
      'ERO': 'Extremely Red Object',
      'El*': 'Ellipsoidal variable Star',
      'Em*': 'Emission-line Star',
      'EmG': 'Emission-line galaxy',
      'EmO': 'Emission Object',
      'Er*': 'Eruptive variable Star',
      'FIR': 'Far-IR source (\xce\xbb >= 30 \xc2\xb5m)',
      'FU*': 'Variable Star of FU Ori type',
      'Fl*': 'Flare Star',
      'G': 'Galaxy',
      'G?': 'Possible Galaxy',
      'GNe': 'Galactic Nebula',
      'GWE': 'Gravitational Wave Event',
      'GiC': 'Galaxy in Cluster of Galaxies',
      'GiG': 'Galaxy in Group of Galaxies',
      'GiP': 'Galaxy in Pair of Galaxies',
      'Gl?': 'Possible Globular Cluster',
      'GlC': 'Globular Cluster',
      'Gr?': 'Possible Group of Galaxies',
      'GrG': 'Group of Galaxies',
      'H2G': 'HII Galaxy',
      'HB*': 'Horizontal Branch Star',
      'HB?': 'Possible Horizontal Branch Star',
      'HH': 'Herbig-Haro Object',
      'HI': 'HI (21cm) source',
      'HII': 'HII region',
      'HS*': 'Hot subdwarf',
      'HS?': 'Hot subdwarf candidate',
      'HV*': 'High-velocity Star',
      'HVC': 'High-velocity Cloud',
      'HX?': 'High-Mass X-ray binary Candidate',
      'HXB': 'High Mass X-ray Binary',
      'Hil': 'Hilda Asteroid',
      'Hun': 'Hungaria Asteroid',
      'HzG': 'Galaxy with high redshift',
      'IG': 'Interacting Galaxies',
      'IR': 'Infra-Red source',
      'ISM': 'Interstellar matter',
      'Ir*': 'Variable Star of irregular type',
      'JTA': 'Jupiter Trojan Asteroid',
      'LI?': 'Possible gravitationally lensed image',
      'LIN': 'LINER-type Active Galaxy Nucleus',
      'LLS': 'Lyman limit system',
      'LM*': 'Low-mass star (M<1solMass)',
      'LM?': 'Low-mass star candidate',
      'LP*': 'Long-period variable star',
      'LP?': 'Long Period Variable candidate',
      'LS?': 'Possible gravitational lens System',
      'LSB': 'Low Surface Brightness Galaxy',
      'LX?': 'Low-Mass X-ray binary Candidate',
      'LXB': 'Low Mass X-ray Binary',
      'Le?': 'Possible gravitational lens',
      'LeG': 'Gravitationally Lensed Image of a Galaxy',
      'LeI': 'Gravitationally Lensed Image',
      'LeQ': 'Gravitationally Lensed Image of a Quasar',
      'Lev': '(Micro)Lensing Event',
      'LyA': 'Ly alpha Absorption Line system',
      'MBA': 'Main Belt Asteroid',
      'MGr': 'Moving Group',
      'MPl': 'Minor Planet',
      'Mas': 'Maser',
      'Mi*': 'Variable Star of Mira Cet type',
      'Mi?': 'Mira candidate',
      'MoC': 'Molecular Cloud',
      'Moo': 'Moon',
      'N*': 'Confirmed Neutron Star',
      'N*?': 'Neutron Star Candidate',
      'NEO': 'Near Earth Object',
      'NIR': 'Near-IR source (\xce\xbb < 10 \xc2\xb5m)',
      'NL*': 'Nova-like Star',
      'No*': 'Nova',
      'No?': 'Nova Candidate',
      'OH*': 'OH/IR star',
      'OH?': 'Possible Star with envelope of OH/IR type',
      'OVV': 'Optically Violently Variable object',
      'OpC': 'Open Cluster',
      'Or*': 'Variable Star of Orion Type',
      'PM*': 'High proper-motion Star',
      'PN': 'Planetary Nebula',
      'PN?': 'Possible Planetary Nebula',
      'PaG': 'Pair of Galaxies',
      'Pe*': 'Peculiar Star',
      'Pec?': 'Possible Peculiar Star',
      'Pho': 'Phocaea Asteroid',
      'Pl': 'Extra-solar Confirmed Planet',
      'Pl?': 'Extra-solar Planet Candidate',
      'Pla': 'Planet',
      'PoC': 'Part of Cloud',
      'PoG': 'Part of a Galaxy',
      'Psr': 'Pulsar',
      'Pu*': 'Pulsating variable Star',
      'Q?': 'Possible Quasar',
      'QSO': 'Quasar',
      'RB?': 'Possible Red Giant Branch star',
      'RC*': 'Variable Star of R CrB type',
      'RC?': 'Variable Star of R CrB type candiate',
      'RG*': 'Red Giant Branch star',
      'RI*': 'Variable Star with rapid variations',
      'RNe': 'Reflection Nebula',
      'RR*': 'Variable Star of RR Lyr type',
      'RR?': 'Possible Star of RR Lyr type',
      'RS*': 'Variable of RS CVn type',
      'RV*': 'Variable Star of RV Tau type',
      'Rad': 'Radio-source',
      'Ro*': 'Rotationally variable Star',
      'S*': 'S Star',
      'S*?': 'Possible S Star',
      'SB*': 'Spectroscopic binary',
      'SBG': 'Starburst Galaxy',
      'SC?': 'Possible Supercluster of Galaxies',
      'SCG': 'Supercluster of Galaxies',
      'SFR': 'Star forming region',
      'SN*': 'SuperNova',
      'SN?': 'SuperNova Candidate',
      'SNR': 'SuperNova Remnant',
      'SR?': 'SuperNova Remnant Candidate',
      'SSO': 'Solar System Object',
      'SX*': 'Variable Star of SX Phe type (subdwarf)',
      'St*': 'Stellar Stream',
      'Sun': 'Sun',
      'Sy*': 'Symbiotic Star',
      'Sy1': 'Seyfert 1 Galaxy',
      'Sy2': 'Seyfert 2 Galaxy',
      'Sy?': 'Symbiotic Star Candidate',
      'SyG': 'Seyfert Galaxy',
      'TT*': 'T Tau-type Star',
      'TT?': 'T Tau star Candidate',
      'ULX': 'Ultra-luminous X-ray source',
      'UV': 'UV-emission source',
      'UX?': 'Ultra-luminous X-ray candidate',
      'V*': 'Variable Star',
      'V*?': 'Star suspected of Variability',
      'WD*': 'White Dwarf',
      'WD?': 'White Dwarf Candidate',
      'WR*': 'Wolf-Rayet Star',
      'WR?': 'Possible Wolf-Rayet Star',
      'WU*': 'Eclipsing binary of W UMa type (contact binary)',
      'WV*': 'Variable Star of W Vir type',
      'X': 'X-ray source',
      'XB*': 'X-ray Binary',
      'XB?': 'X-ray binary Candidate',
      'Y*?': 'Young Stellar Object Candidate',
      'Y*O': 'Young Stellar Object',
      'ZZ*': 'Pulsating White Dwarf',
      'a2*': 'Variable Star of alpha2 CVn type',
      'bC*': 'Variable Star of beta Cep type',
      'bCG': 'Blue compact Galaxy',
      'bL*': 'Eclipsing binary of beta Lyr type (semi-detached)',
      'blu': 'Blue object',
      'bub': 'Bubble',
      'cC*': 'Classical Cepheid (delta Cep type)',
      'cir': 'CircumStellar matter',
      'cm': 'centimetric Radio-source',
      'cor': 'Dense core',
      'dS*': 'Variable Star of delta Sct type',
      'err': 'Not an object (error, artefact, ...)',
      'ev': 'transient event',
      'gB': 'gamma-ray Burst',
      'gD*': 'Variable Star of gamma Dor type',
      'gLS': 'Gravitational Lens System (lens+images)',
      'gLe': 'Gravitational Lens',
      'gam': 'gamma-ray source',
      'glb': 'Globule (low-mass dark cloud)',
      'grv': 'Gravitational Source',
      'mAL': 'metallic Absorption Line system',
      'mR': 'metric Radio-source',
      'mm': 'millimetric Radio-source',
      'mul': 'Composite object',
      'of?': 'Outflow candidate',
      'out': 'Outflow',
      'pA*': 'Post-AGB Star (proto-PN)',
      'pA?': 'Post-AGB Star Candidate',
      'pr*': 'Pre-main sequence Star',
      'pr?': 'Pre-main sequence Star Candidate',
      'rB': 'radio Burst',
      'rG': 'Radio Galaxy',
      'red': 'Very red source',
      'reg': 'Region defined in the sky',
      's*b': 'Blue supergiant star',
      's*r': 'Red supergiant star',
      's*y': 'Yellow supergiant star',
      's?b': 'Possible Blue supergiant star',
      's?r': 'Possible Red supergiant star',
      's?y': 'Possible Yellow supergiant star',
      'sg*': 'Evolved supergiant star',
      'sg?': 'Possible Supergiant star',
      'sh': 'HI shell',
      'smm': 'sub-millimetric source',
      'sr*': 'Semi-regular pulsating Star',
      'su*': 'Sub-stellar object',
      'sv?': 'Semi-regular variable candidate',
      'vid': 'Underdense region of the Universe'
    }
    let res = nameForType[skySourceType]
    return res || 'Unknown Type'
  },

  nameForGalaxyMorpho: function (morpho) {
    const galTab = {
      'E': 'Elliptical',
      'SB': 'Barred Spiral',
      'SAB': 'Intermediate Spiral',
      'SA': 'Spiral',
      'S0': 'Lenticular',
      'S': 'Spiral',
      'Im': 'Irregular',
      'dSph': 'Dwarf Spheroidal',
      'dE': 'Dwarf Elliptical'
    }
    for (let morp in galTab) {
      if (morpho.startsWith(morp)) {
        return galTab[morp]
      }
    }
    return ''
  },

  // Return the list of FOV in degree which are adapted to observe this object
  fovsForSkySource: function (ss) {
    switch (ss.model) {
      case 'star':
        return [20, 2]
      case 'dso':
        let dimx = 'dimx' in ss.model_data ? ss.model_data.dimx : 5
        let dimy = 'dimy' in ss.model_data ? ss.model_data.dimy : 5
        return [20, Math.max(dimx, dimy) * 8 / 60]
      case 'minor_planet':
        return [20, 1 / 60]
      case 'planet':
        return [20, 10 / 60, 1 / 60]
      default:
        return [20]
    }
  },

  // Convert a decimal representation of a uint64 value to Hexadecimal
  // Everything is done as strings as 64 bit ints don't work in Javascript
  nsidDecimalToHex: function (nsidDecimal) {
    return hex2dec.decToHex(nsidDecimal).substr(2)
  },

  // Get data for a SkySource from wikipedia
  getSkySourceSummaryFromWikipedia: function (ss) {
    let title
    if (ss.model === 'planet') {
      title = ss.names[0]
      if (title === 'mercury') {
        title = 'mercury_(planet)'
      }
    }
    if (ss.model === 'dso') {
      for (let i in ss.names) {
        if (ss.names[i].startsWith('M ')) {
          title = 'Messier_' + ss.names[i].substr(2)
          break
        }
        if (ss.names[i].startsWith('NGC ')) {
          title = ss.names[i]
          break
        }
        if (ss.names[i].startsWith('IC ')) {
          title = ss.names[i]
          break
        }
      }
    }
    if (ss.model === 'star') {
      for (let i in ss.names) {
        if (ss.names[i].startsWith('* ')) {
          title = this.cleanupOneSkySourceName(ss.names[i])
        }
      }
    }
    if (!title) return Promise.reject(new Error("Can't find wikipedia compatible name"))

    return axios.get('https://en.wikipedia.org/w/api.php?action=query&redirects&prop=extracts&exintro&exlimit=1&exchars=300&format=json&origin=*&titles=' + title,
      {headers: { 'Content-Type': 'application/json; charset=UTF-8' }})
      .then(response => {
        return response.data
      })
  },

  getGeolocation: function (vueInstance) {
    console.log('Getting geolocalization')
    var that = vueInstance

    if (navigator.geolocation) {
      return new Promise((resolve, reject) => {
        navigator.geolocation.getCurrentPosition(function (position) {
          var pos = {
            lat: position.coords.latitude,
            lng: position.coords.longitude,
            accuracy: position.coords.accuracy
          }
          resolve(pos)
        }, function () {
          reject(new Error('Error getting location from browser'))
        }, { enableHighAccuracy: true })
      })
    }

    // No HTML5 Geolocalization support, try with GEOIP
    console.log('Browser don\'t support geolocation, try from GeoIP')
    return that.$jsonp('https://geoip-db.com/jsonp', {callbackName: 'callback'})
      .then(location => {
        var pos = {
          lat: location.latitude,
          lng: location.longitude,
          accuracy: 50000
        }
        return pos
      }, err => { console.log(err) })
  },

  geoCodePosition: function (pos) {
    console.log('Geocoding Position: ' + JSON.stringify(pos))
    /* eslint-disable no-undef */
    var geocoder = new google.maps.Geocoder()
    var loc = {
      shortName: (pos.accuracy > 500 ? 'Near ' : '') + 'Lat ' + pos.lat.toFixed(3) + '° Lon ' + pos.lng.toFixed(3) + '°',
      country: 'Unknown',
      lng: pos.lng,
      lat: pos.lat,
      alt: pos.alt ? pos.alt : 0,
      accuracy: pos.accuracy,
      streetAddress: ''
    }
    return new Promise((resolve, reject) => {
      window.gm_authFailure = function () {
        // This happens when the map API is not usable for some reasons
        console.log('Google maps service failed to geocode, fallback to just position')
        resolve(loc)
      }
      geocoder.geocode({ 'location': {lat: pos.lat, lng: pos.lng} }, function (results, status) {
        if (status === 'OK') {
          if (results.length > 0) {
            let localityFound = false
            for (let c of results[0].address_components) {
              if (c.types.includes('locality')) {
                localityFound = true
                loc.shortName = (pos.accuracy > 500 ? 'Near ' : '') + c.short_name
              }
              if (c.types.includes('postal_town') && !localityFound) {
                loc.shortName = (pos.accuracy > 500 ? 'Near ' : '') + c.short_name
              }
              if (c.types.includes('neighborhood') && !localityFound) {
                loc.shortName = (pos.accuracy > 500 ? 'Near ' : '') + c.short_name
              }
              if (c.types.includes('administrative_area_level_2') && !localityFound) {
                loc.shortName = (pos.accuracy > 500 ? 'Near ' : '') + c.short_name
              }
              if (c.types.includes('country')) {
                loc.country = c.long_name
              }
            }
            if (pos.accuracy < 50) {
              for (let c of results[0].address_components) {
                if (c.types.includes('street_number')) {
                  loc.streetAddress = c.short_name
                }
                if (c.types.includes('route')) {
                  loc.streetAddress = loc.streetAddress + ' ' + c.short_name
                }
              }
            }
            resolve(loc)
          } else {
            console.log('Geocoder returned nothing')
            resolve(loc)
          }
        } else {
          console.log('Geocoder failed due to: ' + status)
          resolve(loc)
        }
      })
    })
  },

  getDistanceFromLatLonInM: function (lat1, lon1, lat2, lon2) {
    var deg2rad = function (deg) {
      return deg * (Math.PI / 180)
    }
    var R = 6371000 // Radius of the earth in m
    var dLat = deg2rad(lat2 - lat1)
    var dLon = deg2rad(lon2 - lon1)
    var a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
            Math.cos(deg2rad(lat1)) * Math.cos(deg2rad(lat2)) *
            Math.sin(dLon / 2) * Math.sin(dLon / 2)
    var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a))
    var d = R * c // Distance in m
    return d
  }
}
