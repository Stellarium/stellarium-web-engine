// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

import Vue from 'vue'
import _ from 'lodash'
import StelWebEngine from '@/assets/js/stellarium-web-engine.js'
import Moment from 'moment'
import { i18n } from '../plugins/i18n.js'
import langs from '../plugins/langs.js'

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

const swh = {
  initStelWebEngine: function (store, wasmFile, canvasElem, callBackOnDone, onBeforeRendering) {
    let lstel = StelWebEngine({
      wasmFile: wasmFile,
      canvas: canvasElem,
      onReady: function (Module) {
        Module.onBeforeRendering = onBeforeRendering
        // Add all data sources.
        var baseUrl = 'https://cors-anywhere.herokuapp.com/https://stellarium.sfo2.cdn.digitaloceanspaces.com/'

        // Bundled stars (just the very bright ones)
        lstel.addDataSource({url: 'asset://stars', type: 'hips'})
        // Gaia stars tiles
        lstel.addDataSource({url: baseUrl + 'surveys/gaia/v1?v=2019-02-11T05:34Z', type: 'hips'})

        lstel.addDataSource({url: baseUrl + 'skycultures/v1/western', type: 'skyculture'})
        lstel.addDataSource({url: baseUrl + 'surveys/dso/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'landscapes/v1/guereins', type: 'landscape'})

        // MPC data
        lstel.addDataSource({url: 'asset://mpcorb.dat', type: 'mpc_asteroids'})
        lstel.addDataSource({url: baseUrl + 'mpc/v1/CometEls.txt', type: 'mpc_comets'})
        // Artificial Satellites
        lstel.addDataSource({url: baseUrl + 'skysources/v1/tle_satellite.jsonl.gz', type: 'jsonl/sat'})

        // Background images
        lstel.addDataSource({url: baseUrl + 'surveys/dss/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/milkyway/v1', type: 'hips'})

        // All planetary images
        lstel.addDataSource({url: baseUrl + 'surveys/sso/callisto/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/default/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/europa/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/ganymede/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/io/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/jupiter/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/mars/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/mercury/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/moon/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/moon-normal/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/neptune/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/saturn/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/sun/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/uranus/v1', type: 'hips'})
        lstel.addDataSource({url: baseUrl + 'surveys/sso/venus/v1', type: 'hips'})

        store.commit('replaceStelWebEngine', lstel.getTree())
        lstel.onValueChanged(function (path, value) {
          let tree = store.state.stel
          _.set(tree, path, value)
          store.commit('replaceStelWebEngine', tree)
        })
        Vue.prototype.$stel = lstel
        Vue.prototype.$selectionLayer = lstel.createLayer({id: 'slayer', z: 50, visible: true})
        Vue.prototype.$observingLayer = lstel.createLayer({id: 'obslayer', z: 40, visible: true})
        Vue.prototype.$skyHintsLayer = lstel.createLayer({id: 'skyhintslayer', z: 38, visible: true})
        callBackOnDone()
      }
    })
  },

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
      'SSO': 'minor_planet',

      'Con': 'constellation'
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

  nameForSkySourceType: function (otype, lang) {
    let $stel = Vue.prototype.$stel
    let res = $stel.otypeToStr(otype, lang)
    return res || 'Unknown Type'
  },

  nameForGalaxyMorpho: function (morpho) {
    const galTab = {
      'E': i18n.t('galaxy_type.elliptical'),
      'SB': i18n.t('galaxy_type.bar_spiral'),
      'SAB': i18n.t('galaxy_type.int_spiral'),
      'SA': i18n.t('galaxy_type.spiral'),
      'S0': i18n.t('galaxy_type.lenticular'),
      'S': i18n.t('galaxy_type.spiral'),
      'Im': i18n.t('galaxy_type.irregular'),
      'dSph': i18n.t('galaxy_type.dwf_spheroidal'),
      'dE': i18n.t('galaxy_type.dwf_elliptical')
    }
    for (let morp in galTab) {
      if (morpho.startsWith(morp)) {
        return galTab[morp]
      }
    }
    return ''
  },

  getShareLink: function (context) {
    let link = 'https://stellarium-web.org/'
    if (context.$store.state.selectedObject) {
      link += 'skysource/' + context.$store.state.selectedObject.short_name
    }
    link += '?'
    link += 'fov=' + (context.$store.state.stel.fov * 180 / Math.PI).toPrecision(5)
    let d = new Date()
    d.setMJD(context.$stel.core.observer.utc)
    link += '&date=' + new Moment(d).utc().format()
    link += '&lat=' + (context.$stel.core.observer.latitude * 180 / Math.PI).toFixed(2)
    link += '&lng=' + (context.$stel.core.observer.longitude * 180 / Math.PI).toFixed(2)
    link += '&elev=' + context.$stel.core.observer.elevation
    if (!context.$store.state.selectedObject) {
      link += '&az=' + (context.$stel.core.observer.yaw * 180 / Math.PI).toPrecision(5)
      link += '&alt=' + (context.$stel.core.observer.pitch * 180 / Math.PI).toPrecision(5)
    }
    return link
  },

  // Return a SweObj matching a passed sky source JSON object if it's already instanciated in SWE
  skySource2SweObj: function (ss) {
    if (!ss || !ss.model) {
      return undefined
    }
    let $stel = Vue.prototype.$stel
    let obj
    if (ss.model === 'tle_satellite') {
      let id = 'NORAD ' + ss.model_data.norad_number
      obj = $stel.getObj(id)
    } else if (ss.model === 'constellation' && ss.model_data.iau_abbreviation) {
      let id = 'CST ' + ss.model_data.iau_abbreviation
      obj = $stel.getObj(id)
    } else if (ss.model === 'mpc_comet') {
      let id = ss.short_name
      obj = $stel.getObj(id)
    } else if (ss.model === 'jpl_sso') {
      let id = ss.short_name
      obj = $stel.getObj(id)
    }
    if (!obj) {
      obj = $stel.getObj(ss.names[0])
    }
    if (!obj && ss.names[0].startsWith('Gaia DR2 ')) {
      let gname = ss.names[0].replace(/^Gaia DR2 /, 'GAIA ')
      obj = $stel.getObj(gname)
    }
    if (obj === null) return undefined
    return obj
  },

  lookupSkySourceByName: function (name) {
    return fetch(process.env.NOCTUASKY_API_SERVER + '/api/v1/skysources/name/' + name)
      .then(function (response) {
        if (!response.ok) {
          throw response.body
        }
        return response.json()
      }, err => {
        throw err.response.body
      })
  },

  querySkySources: function (str, limit) {
    if (!limit) {
      limit = 10
    }
    return fetch(process.env.NOCTUASKY_API_SERVER + '/api/v1/skysources/?q=' + str + '&limit=' + limit)
      .then(function (response) {
        if (!response.ok) {
          throw response.body
        }
        return response.json()
      }, err => {
        throw err.response.body
      })
  },

  sweObj2SkySource: function (obj) {
    let $stel = Vue.prototype.$stel
    let names = obj.designations()
    let that = this

    if (!names || !names.length) {
      throw new Error("Can't find object without names")
    }

    // Several artifical satellites share the same common name, so we use
    // the unambiguous NORAD number instead
    for (let j in names) {
      if (names[j].startsWith('NORAD ')) {
        let tmpName = names[0]
        names[0] = names[j]
        names[j] = tmpName
      }
    }

    let printErr = function (err, n) {
      let gaiaName
      for (let i in names) {
        if (names[i].startsWith('GAIA')) {
          gaiaName = names[i]
        }
      }
      if (gaiaName) {
        console.log('Generate Gaia object info from StelWebEngine object')
        let radecICRS = $stel.c2s(obj.getInfo('radec'))
        let raICRS = $stel.anp(radecICRS[0])
        let decICRS = $stel.anpm(radecICRS[1])
        let ss = {
          model: 'star',
          types: ['*'],
          names: [obj.designations()[0].replace(/^GAIA /, 'Gaia DR2 ')],
          modelData: {
            Vmag: obj.getInfo('vmag'),
            ra: raICRS * 180 / Math.PI,
            de: decICRS * 180 / Math.PI
          }
        }
        return ss
      }
      console.log(err)
      console.log("Couldn't find skysource for name: " + n)
      throw err
    }

    return that.lookupSkySourceByName(names[0]).then(res => {
      return res
    }, err => {
      if (names.length === 1) return printErr(err, names[0])
      return that.lookupSkySourceByName(names[1]).then(res => {
        return res
      }, err => {
        if (names.length === 2) return printErr(err, names[1])
        return that.lookupSkySourceByName(names[2]).then(res => {
          return res
        }, err => {
          return printErr(err, names[2])
        })
      })
    })
  },

  setSweObjAsSelection: function (obj) {
    let $stel = Vue.prototype.$stel
    $stel.core.selection = obj
    $stel.pointAndLock(obj)
  },

  // Get data for a SkySource from wikipedia
  getSkySourceSummaryFromWikipedia: function (ss) {
    let title
    if (ss.model === 'jpl_sso') {
      title = ss.short_name.toLowerCase()
      if (title === 'sun' || title === 'moon') {
        title = title.replace(title, i18n.t(`planets.${title}`))
      } else {
        if (['mercury', 'venus', 'earth', 'mars', 'jupiter', 'saturn', 'neptune', 'pluto'].indexOf(title) > -1) {
          title = title + '_(planet)'
        }
        if (ss.types[0] === 'Moo') {
          title = title + '_(moon)'
        }
      }
    }
    if (ss.model === 'mpc_asteroid') {
      title = ss.short_name
    }
    if (ss.model === 'constellation') {
      title = ss.short_name + '_(constellation)'
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
      if (ss.short_name !== undefined) {
        title = ss.short_name
      } else {
        for (let i in ss.names) {
          if (ss.names[i].startsWith('NAME')) {
            title = this.cleanupOneSkySourceName(ss.names[i])
          }
        }
      }
      if (title === undefined) {
        for (let i in ss.names) {
          if (ss.names[i].startsWith('* ')) {
            title = this.cleanupOneSkySourceName(ss.names[i])
          }
        }
      }
    }
    if (!title) return Promise.reject(new Error("Can't find wikipedia compatible name"))

    var language = langs.language()

    return fetch('https://' + language + '.wikipedia.org/w/api.php?action=query&redirects&prop=extracts&exintro&exlimit=1&exchars=300&format=json&origin=*&titles=' + title,
      {headers: { 'Content-Type': 'application/json; charset=UTF-8' }})
      .then(response => {
        return response.json()
      })
  },

  getGeolocation: function () {
    console.log('Getting geolocalization')

    // First get geoIP location, to use as fallback
    return Vue.jsonp('https://freegeoip.stellarium.org/json/', {callbackName: 'callback'})
      .then(location => {
        var pos = {
          lat: location.latitude,
          lng: location.longitude,
          accuracy: 20000
        }
        console.log('GeoIP localization: ' + JSON.stringify(pos))
        return pos
      }, err => {
        console.log(err)
      }).then(geoipPos => {
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
              console.log('Could not get location from browser, use fallback from GeoIP')
              // No HTML5 Geolocalization support, return geoip fallback values
              if (geoipPos) {
                resolve(geoipPos)
              } else {
                reject(new Error('Cannot detect position'))
              }
            }, { enableHighAccuracy: true })
          })
        }
      })
  },

  delay: function (t, v) {
    return new Promise(function (resolve) {
      setTimeout(resolve.bind(null, v), t)
    })
  },

  geoCodePosition: function (pos) {
    console.log('Geocoding position...')
    var loc = {
      short_name: (pos.accuracy > 500 ? 'Near ' : '') + 'Lat ' + pos.lat.toFixed(3) + '° Lon ' + pos.lng.toFixed(3) + '°',
      country: 'Unknown',
      lng: pos.lng,
      lat: pos.lat,
      alt: pos.alt ? pos.alt : 0,
      accuracy: pos.accuracy,
      street_address: ''
    }
    return fetch('https://nominatim.openstreetmap.org/reverse?format=jsonv2&lat=' + pos.lat + '&lon=' + pos.lng,
      {headers: { 'Content-Type': 'application/json; charset=UTF-8' }}).then(response => {
      if (response.ok) {
        return response.json().then(res => {
          let city = res.address.city ? res.address.city : (res.address.village ? res.address.village : res.name)
          loc.short_name = pos.accuracy > 500 ? 'Near ' + city : city
          loc.country = res.address.country
          if (pos.accuracy < 50) {
            loc.street_address = res.address.road ? res.address.road : res.display_name
          }
          return loc
        })
      } else {
        console.log('Geocoder failed due to: ' + response.statusText)
        return loc
      }
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
  },

  // Look for the next time starting from now on when the night Sky is visible
  // i.e. when sun is more than 10 degree below horizon.
  // If no such time was found (e.g. in a northern country in summer),
  // we default to current time.
  getTimeAfterSunset: function (stel) {
    let sun = stel.getObj('Sun')
    let obs = stel.observer.clone()
    let utc = Math.floor(obs.utc * 24 * 60 / 5) / (24 * 60 / 5)
    let i
    for (i = 0; i < 24 * 60 / 5 + 1; i++) {
      obs.utc = utc + 1.0 / (24 * 60) * (i * 5)
      let sunRadec = sun.getInfo('RADEC', obs)
      let azalt = stel.convertFrame(obs, 'ICRF', 'OBSERVED', sunRadec)
      let alt = stel.anpm(stel.c2s(azalt)[1])
      if (alt < -13 * Math.PI / 180) {
        break
      }
    }
    if (i === 0 || i === 24 * 60 / 5 + 1) {
      return stel.observer.utc
    }
    return obs.utc
  },

  // Get the list of circumpolar stars in a given magnitude range
  //
  // Arguments:
  //   obs      - An observer.
  //   maxMag   - The maximum magnitude above which objects are discarded.
  //   filter   - a function called for each object returning false if the
  //              object must be filtered out.
  //
  // Return:
  //   An array SweObject. It is the responsibility of the caller to properly
  //   destroy all the objects of the list when they are not needed, by calling
  //   obj.destroy() on each of them.
  //
  // Example code:
  //   // Return all cicumpolar stars between mag -2 and 4
  //   let res = swh.getCircumpolarStars(this.$stel.observer, -2, 4)
  //   // Do something with the stars
  //   console.log(res.length)
  //   // Destroy the objects (don't forget this line!)
  //   res.map(e => e.destroy())
  getCircumpolarStars: function (obs, minMag, maxMag) {
    let $stel = Vue.prototype.$stel
    let filter = function (obj) {
      if (obj.getInfo('vmag', obs) <= minMag) {
        return false
      }
      let posJNOW = $stel.convertFrame(obs, 'ICRF', 'JNOW', obj.getInfo('radec'))
      let radecJNOW = $stel.c2s(posJNOW)
      let decJNOW = $stel.anpm(radecJNOW[1])
      if (obs.latitude >= 0) {
        return decJNOW >= Math.PI / 2 - obs.latitude
      } else {
        return decJNOW <= -Math.PI / 2 + obs.latitude
      }
    }
    return $stel.core.stars.listObjs(obs, maxMag, filter)
  },

  circumpolarMask: undefined,
  showCircumpolarMask: function (obs, show) {
    if (show === undefined) {
      show = true
    }
    let layer = Vue.prototype.$skyHintsLayer
    let $stel = Vue.prototype.$stel
    if (this.circumpolarMask) {
      layer.remove(this.circumpolarMask)
      this.circumpolarMask = undefined
    }
    if (show) {
      let diam = 2.0 * Math.PI - Math.abs(obs.latitude) * 2
      let shapeParams = {
        pos: [0, 0, obs.latitude > 0 ? -1 : 1, 0],
        frame: $stel.FRAME_JNOW,
        size: [diam, diam],
        color: [0.1, 0.1, 0.1, 0.8],
        border_color: [0.1, 0.1, 0.6, 1]
      }
      this.circumpolarMask = layer.add('circle', shapeParams)
    }
  }
}

export default swh
