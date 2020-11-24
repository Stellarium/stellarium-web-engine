// Stellarium Web - Copyright (c) 2020 - Stellarium Labs SAS
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.
//
// This file is part of the Survey Monitoring Tool plugin, which received
// funding from the Centre national d'études spatiales (CNES).

import alasql from 'alasql'
import _ from 'lodash'
import filtrex from 'filtrex'
import hash_sum from 'hash-sum'
import healpix from '@hscmap/healpix'
import turf from '@turf/turf'
import assert from 'assert'
import glMatrix from 'gl-matrix'

// Use native JS array in glMatrix lib
glMatrix.glMatrix.setMatrixArrayType(Array)

const D2R = Math.PI / 180
const R2D = 180 / Math.PI
const HEALPIX_ORDER = 1
const STERADIAN_TO_DEG2 = (180 / Math.PI) * (180 / Math.PI)

const crossAntimeridian = function (feature) {
  let left = false
  let right = false

  turf.coordEach(feature, function (coord) {
    if (left && right) return
    let lng = coord[0]
    if (lng > 180) lng -= 360
    right = right || (lng >= 90)
    left = left || (lng <= -90)
  })
  return left && right
}

const normalizeGeoJson = function (geoData) {
  // Normalize coordinates between -180;180
  turf.coordEach(geoData, function(coord) {
    if (coord[0] > 180) coord[0] -= 360
  })

  // Shift polygons crossing antimeridian
  turf.geomEach(geoData, function(geom) {
    if (!crossAntimeridian(geom)) return
    turf.coordEach(geom, function(coord) {
      if (coord[0] < 0) coord[0] += 360
    })
  })
}

const geojsonPointToVec3 = function (p) {
  const theta = (90 - p[1]) * D2R
  const phi = (p[0] < 0 ? p[0] + 360 : p[0]) * D2R
  assert((theta >= 0) && (theta <= Math.PI))
  assert(phi >= 0 && phi <= 2 * Math.PI)
  return healpix.ang2vec(theta, phi)
}

const vec3TogeojsonPoint = function (v) {
  const p = healpix.vec2ang(v)
  let lat = 90 - p.theta * R2D
  let lon = p.phi * R2D
  if (lon > 180) lon -= 360
  if (lon < -180) lon += 360
  assert((lat >= -90) && (lat <= 90))
  assert((lon >= -180) && (lon <= 180))
  return [lon, lat]
}

const rotateGeojsonPoint = function (p, m) {
  const v = geojsonPointToVec3(p)
  glMatrix.vec3.transformMat3(v, v, m)
  const p2 = vec3TogeojsonPoint(v)
  p[0] = p2[0]
  p[1] = p2[1]
}

const rotateGeojsonFeature = function (feature, m) {
  turf.coordEach(feature, p => {rotateGeojsonPoint(p, m)})
}

const intersectionRobust = function (feature1, feature2, shiftCenter) {
  let q = glMatrix.quat.create()
  const center = geojsonPointToVec3(shiftCenter)
  glMatrix.quat.rotationTo(q, glMatrix.vec3.fromValues(center[0], center[1], center[2]), glMatrix.vec3.fromValues(1, 0, 0))
  const m = glMatrix.mat3.create()
  const mInv = glMatrix.mat3.create()
  glMatrix.mat3.fromQuat(m, q)
  glMatrix.mat3.invert(mInv, m)

  const f1 = _.cloneDeep(feature1)
  const f2 = _.cloneDeep(feature2)
  rotateGeojsonFeature(f1, m)
  rotateGeojsonFeature(f2, m)
  let res = null
  try {
    if (f1.geometry.type === 'MultiPolygon') {
      const intPoly = []
      for (let i = 0; i < f1.geometry.coordinates.length; ++i) {
          const poly = turf.polygon(f1.geometry.coordinates[i])
          const intersection = turf.intersect(poly, f2)
          if (intersection && intersection.geometry.type === 'Polygon') intPoly.push(intersection.geometry.coordinates)
      }
      if (intPoly.length) {
        f1.geometry.coordinates = intPoly
        res = f1
      }
    } else {
      res = turf.intersect(f1, f2)
    }
    if (res) rotateGeojsonFeature(res, mInv)
  } catch (err) {
    console.log('Error computing feature intersection: ' + err)
    res = null
  }
  return res
}

// Return the area of the feature in steradiant
const featureArea (feature) {
  return turf.area(feature) / (1000 * 1000) * 4 * Math.PI / 509600000
}

export default {
  fieldsList: undefined,
  baseHashKey: '',
  sqlFields: undefined,
  fcounter: 0,

  fId2AlaSql: function (fieldId) {
    return fieldId.replace(/\./g, '_')
  },

  fType2AlaSql: function (fieldType) {
    if (fieldType === 'string') return 'STRING'
    if (fieldType === 'date') return 'INT' // Dates are converted to unix time stamp
    return 'JSON'
  },

  initDB: async function (fieldsList, baseHashKey) {
    // Add a custom aggregation operator for the chip tags
    alasql.aggr.VALUES_AND_COUNT = function (value, accumulator, stage) {
      if (stage === 1) {
        let ac = {}
        ac[value] = 1
        return ac
      } else if (stage === 2) {
        accumulator[value] = (accumulator[value] !== undefined) ? accumulator[value] + 1 : 1
        return accumulator
      } else if (stage === 3) {
        return accumulator
      }
    }

    alasql.aggr.MIN_MAX = function (value, accumulator, stage) {
      // console.log('MIN_MAX ' + value + ' ' + accumulator + ' ' + stage)
      if (stage === 1) {
        return [value, value]
      } else if (stage === 2) {
        return [Math.min(accumulator[0], value), Math.max(accumulator[1], value)]
      }
      return accumulator
    }

    let that = this

    console.log('Create Data Base')
    that.fieldsList = fieldsList
    that.baseHashKey = baseHashKey
    for (let i in fieldsList) {
      if (fieldsList[i].computed) {
        let options = {
          extraFunctions: { date2unix: function (dstr) { return new Date(dstr).getTime() } },
          customProp: (path, unused, obj) => _.get(obj, path, undefined)
        }
        fieldsList[i].computed_compiled = filtrex.compileExpression(fieldsList[i].computed, options)
      }
    }
    that.sqlFields = fieldsList.map(f => that.fId2AlaSql(f.id))
    let sqlFieldsAndTypes = that.sqlFields.map(f => f + ' ' + that.fType2AlaSql(f.type)).join(', ')
    await alasql.promise('CREATE TABLE features (id STRING, geometry JSON, healpix_index INT, geogroup_id STRING, properties JSON, ' + sqlFieldsAndTypes + ')')
    await alasql.promise('CREATE INDEX idx_id ON features(id)')
    await alasql.promise('CREATE INDEX idx_healpix_index ON features(healpix_index)')
    await alasql.promise('CREATE INDEX idx_geogroup_id ON features(geogroup_id)')

    // Create an index on each field
    for (let i in that.sqlFields) {
      let field = that.sqlFields[i]
      await alasql.promise('CREATE INDEX idx_' + i + ' ON features(' + field + ')')
    }
  },

  computeCentroidHealpixIndex: function (feature, order) {
    let center = turf.centroid(feature)
    assert(center)
    center = center.geometry.coordinates
    if (center[0] < 0) center[0] += 360
    const theta = (90 - center[1]) * D2R
    const phi = center[0] * D2R
    assert((theta >= 0) && (theta <= Math.PI))
    assert(phi >= 0 && phi <= 2 * Math.PI)
    return healpix.ang2pix_nest(1 << order, theta, phi)
  },

  healpixCornerFeatureCache: {},
  getHealpixCornerFeature: function (order, pix) {
    const cacheKey = '' + order + '_' + pix
    if (cacheKey in this.healpixCornerFeatureCache)
      return this.healpixCornerFeatureCache[cacheKey]
    const mod = function(v, n) {
      return ( v + n ) % n
    }
    let corners = healpix.corners_nest(1 << order, pix)
    corners = corners.map(c => { return vec3TogeojsonPoint(c) })
    for (let i = 0; i < corners.length; ++i) {
      if (Math.abs(corners[i][1]) > 89.9999999) {
        corners[i][0] = corners[mod(i + 1, corners.length)][0]
        corners.splice(i, 0, [corners[mod(i - 1, corners.length)][0], corners[i][1]])
        break
      }
    }
    corners.push(_.cloneDeep(corners[0]))
    let hppixel = turf.polygon([corners])
    normalizeGeoJson(hppixel)
    this.healpixCornerFeatureCache[cacheKey] = hppixel
    return hppixel
  },

  // Split the given feature in pieces on the healpix grid
  // Returns a list of features each with 'healpix_index' set to the matching
  // index.
  splitOnHealpixGrid: function (feature, order) {
    const that = this
    let area = featureArea(feature)
    // For large footprints, we return -1 so that it goes in the AllSky order
    if (area > healpix.nside2pixarea(1 << order)) {
      feature['healpix_index'] = -1
      return [feature]
    }
    let center = turf.centroid(feature)
    assert(center)
    center = center.geometry.coordinates
    let radius = 0
    turf.coordEach(feature, (coord) => {
      const d = turf.distance(coord, center, {units: 'radians'})
      if (d > radius) radius = d
    })

    const v = geojsonPointToVec3(center)
    const ret = []
    healpix.query_disc_inclusive_nest(1 << order, v, radius, function (pix) {
      let hppixel = that.getHealpixCornerFeature(order, pix)
      const intersection = intersectionRobust(feature, hppixel, center)
      if (intersection === null) return
      const f = _.cloneDeep(feature)
      f['healpix_index'] = pix
      f.geometry = intersection.geometry
      ret.push(f)
    })
    if (ret.length === 0) {
      console.log(JSON.stringify(feature))
    }
    return ret
  },

  loadAllData: async function (jsonData) {
    console.log('Loading ' + jsonData.features.length + ' features')
    let that = this

//    for (let pix=0; pix < 12 * (2 << HEALPIX_ORDER); pix++) {
//      let hppixel = that.getHealpixCornerFeature(HEALPIX_ORDER, pix)
//      console.log(JSON.stringify(hppixel) + ',')
//    }
//    process.exit()

    normalizeGeoJson(jsonData)

    // Insert all data
    let subFeatures = []
    turf.featureEach(jsonData, function (feature, featureIndex) {
      feature.geogroup_id = _.get(feature.properties, 'fieldID', undefined) || _.get(feature.properties, 'SurveyName', '') + _.get(feature, 'id', '')
      feature.id = that.fcounter++
      subFeatures = subFeatures.concat(that.splitOnHealpixGrid(feature, HEALPIX_ORDER))
    })
    for (let feature of subFeatures) {
      for (let i = 0; i < that.fieldsList.length; ++i) {
        const field = that.fieldsList[i]
        let d
        if (field.computed_compiled) {
          d = field.computed_compiled(feature.properties)
          if (isNaN(d)) d = undefined
        } else {
          d = _.get(feature.properties, field.id, undefined)
          if (d !== undefined && field.type === 'date') {
            d = new Date(d).getTime()
          }
        }
        feature[that.sqlFields[i]] = d
      }
    }
    await alasql.promise('SELECT * INTO features FROM ?', [subFeatures])
  },

  loadGeojson: function (url) {
    let that = this
    return fetch(url).then(function (response) {
      if (!response.ok) {
        throw response.body
      }
      return response.json().then(jsonData => {
        return that.loadAllData(jsonData).then(_ => {
          return jsonData.length
        })
      }, err => { throw err })
    }, err => {
      throw err
    })
  },

  // Construct the SQL WHERE clause matching the given constraints
  constraints2SQLWhereClause: function (constraints) {
    let that = this
    if (!constraints || constraints.length === 0) {
      return ''
    }
    // Group constraints by field. We do a OR between constraints for the same field, AND otherwise.
    let groupedConstraints = {}
    for (let i in constraints) {
      let c = constraints[i]
      if (c.field.id in groupedConstraints) {
        groupedConstraints[c.field.id].push(c)
      } else {
        groupedConstraints[c.field.id] = [c]
      }
    }
    groupedConstraints = Object.values(groupedConstraints)

    // Convert one contraint to a SQL string
    let c2sql = function (c) {
      let fid = that.fId2AlaSql(c.field.id)
      if (c.operation === 'STRING_EQUAL') {
        return fid + ' = "' + c.expression + '"'
      } else if (c.operation === 'INT_EQUAL') {
        return fid + ' = ' + c.expression
      } else if (c.operation === 'IS_UNDEFINED') {
        return fid + ' IS NULL'
      } else if (c.operation === 'DATE_RANGE') {
        return '( ' + fid + ' IS NOT NULL AND ' + fid + ' >= ' + c.expression[0] +
          ' AND ' + fid + ' <= ' + c.expression[1] + ')'
      } else if (c.operation === 'NUMBER_RANGE') {
        return '( ' + fid + ' IS NOT NULL AND ' + fid + ' >= ' + c.expression[0] +
          ' AND ' + fid + ' <= ' + c.expression[1] + ')'
      } else if (c.operation === 'IN') {
        if (c.field.type === 'string') {
          return '( ' + fid + ' IN (' + c.expression.map(e => "'" + e + "'").join(', ') + ') )'
        } else {
          return '( ' + fid + ' IN (' + c.expression.map(e => '' + e).join(', ') + ') )'
        }
      } else {
        throw new Error('Unsupported query operation: ' + c.operation)
      }
    }

    let whereClause = ' WHERE '
    for (let i in groupedConstraints) {
      let gCons = groupedConstraints[i]
      if (gCons.length > 1) whereClause += ' ('
      for (let j in gCons) {
        whereClause += c2sql(gCons[j])
        if (j < gCons.length - 1) {
          whereClause += ' OR '
        }
      }
      if (gCons.length > 1) whereClause += ') '
      if (i < groupedConstraints.length - 1) {
        whereClause += ' AND '
      }
    }
    return whereClause
  },

  // Query the engine
  query: function (q) {
    let that = this
    let whereClause = this.constraints2SQLWhereClause(q.constraints)
    if (q.limit && Number.isInteger(q.limit)) {
      whereClause += ' LIMIT ' + q.limit
    }
    if (q.skip && Number.isInteger(q.skip)) {
      whereClause += ' FETCH ' + q.skip
    }

    // Construct the SQL SELECT clause matching the given aggregate options
    let selectClause = 'SELECT '
    if (q.projectOptions) {
      selectClause += Object.keys(q.projectOptions).map(k => that.fId2AlaSql(k)).join(', ')
    }
    if (q.aggregationOptions) {
      // We can't do much more than group all using SQL language
      console.assert(q.groupingOptions.length === 1 && q.groupingOptions[0].operation === 'GROUP_ALL')
      for (let i in q.aggregationOptions) {
        let agOpt = q.aggregationOptions[i]
        console.assert(agOpt.out)
        if (agOpt.operation === 'COUNT') {
          selectClause += 'COUNT(*) as ' + agOpt.out
        } else if (agOpt.operation === 'VALUES_AND_COUNT') {
          selectClause += 'VALUES_AND_COUNT(' + that.fId2AlaSql(agOpt.fieldId) + ') as ' + agOpt.out
        } else if (agOpt.operation === 'DATE_HISTOGRAM') {
          // Special case, do custom queries and return
          console.assert(q.aggregationOptions.length === 1)
          let fid = that.fId2AlaSql(agOpt.fieldId)
          let wc = (whereClause === '') ? ' WHERE ' + fid + ' IS NOT NULL' : whereClause + ' AND ' + fid + ' IS NOT NULL'
          let req = 'SELECT MIN(' + fid + ') AS dmin, MAX(' + fid + ') AS dmax FROM features' + wc
          return alasql.promise(req).then(res => {
            if (res[0].dmin === undefined || res[0].dmax === undefined) {
              // No results
              let data = {
                min: undefined,
                max: undefined,
                step: 'DAY',
                table: [['Date', 'Count']]
              }
              let retd = {}
              retd[agOpt.out] = data
              return { q: q, res: [retd] }
            }
            let start = new Date(res[0].dmin)
            start.setHours(0, 0, 0, 0)
            // Switch to next day and truncate
            let stop = new Date(res[0].dmax + 1000 * 60 * 60 * 24)
            stop.setHours(0, 0, 0, 0)
            // Range in days
            let range = (stop - start) / (1000 * 60 * 60 * 24)
            let step = 'DAY'
            if (range > 3 * 365) {
              step = 'YEAR'
            } else if (range > 3 * 30) {
              step = 'MONTH'
            }

            let sqlQ = 'SELECT COUNT(*) AS c, FIRST(' + fid + ') AS d FROM features ' + wc + ' GROUP BY ' + step + ' (' + fid + ')'
            return alasql.promise(sqlQ).then(res2 => {
              let data = {
                min: start,
                max: stop,
                step: step,
                table: [['Date', 'Count']]
              }
              for (let j in res2) {
                let d = new Date(res2[j].d)
                d.setHours(0, 0, 0, 0)
                if (step === 'MONTH') d.setDate(0)
                if (step === 'YEAR') d.setMonth(0)
                data.table.push([d, res2[j].c])
              }
              let retd = {}
              retd[agOpt.out] = data
              return { q: q, res: [retd] }
            })
          })
        } else if (agOpt.operation === 'NUMBER_HISTOGRAM') {
          // Special case, do custom queries and return
          console.assert(q.aggregationOptions.length === 1)
          let fid = that.fId2AlaSql(agOpt.fieldId)
          let wc = (whereClause === '') ? ' WHERE ' + fid + ' IS NOT NULL' : whereClause + ' AND ' + fid + ' IS NOT NULL'
          let req = 'SELECT MIN(' + fid + ') AS dmin, MAX(' + fid + ') AS dmax FROM features' + wc
          return alasql.promise(req).then(res => {
            if (res[0].dmin === res[0].dmax) {
              // No results
              let data = {
                min: res[0].dmin,
                max: res[0].dmax,
                step: undefined,
                table: [['Value', 'Count']]
              }
              let retd = {}
              retd[agOpt.out] = data
              return { q: q, res: [retd] }
            }
            let step = (res[0].dmax - res[0].dmin) / 10

            let sqlQ = 'SELECT COUNT(*) AS c, ROUND((FIRST(' + fid + ') - ' + res[0].dmin + ') / ' + step + ') * ' + step + ' AS d FROM features ' + wc + ' GROUP BY ROUND((' + fid + ' - ' + res[0].dmin + ') / ' + step + ')'
            return alasql.promise(sqlQ).then(res2 => {
              let data = {
                min: res[0].dmin,
                max: res[0].dmax,
                step: step,
                table: [['Value', 'Count']]
              }
              for (let j in res2) {
                data.table.push([res2[j].d, res2[j].c])
              }
              let retd = {}
              retd[agOpt.out] = data
              return { q: q, res: [retd] }
            })
          })
        } else {
          throw new Error('Unsupported aggregation operation: ' + agOpt.operation)
        }
      }
    }
    selectClause += ' FROM features'
    let sqlStatement = selectClause + whereClause
    return alasql.promise(sqlStatement).then(res => { return { q: q, res: res } })
  },

  // Contain the query settings (constraints) linked to a previous query
  hashToQuery: {},

  // Query the engine
  queryVisual: function (q) {
    // Inject a key unique to each revision of the input data
    // this ensure the hash depends on query + data content
    q.baseHashKey = this.baseHashKey
    const hash = hash_sum(q)
    this.hashToQuery[hash] = q
    return hash
  },

  getHipsProperties: function (queryHash) {
    return `hips_tile_format = geojson\nhips_order = ${HEALPIX_ORDER}\nhips_order_min = ${HEALPIX_ORDER}`
  },

  getHipsTile: function (queryHash, order, tileId) {
    if (order === -1) {
      // Special case for Allsky
      tileId = -1
    } else {
      assert(order == HEALPIX_ORDER)
    }
    const that = this
    const q = _.cloneDeep(this.hashToQuery[queryHash])
    if (!q)
      return undefined
    q.constraints.push({
      field: {id: 'healpix_index'},
      operation: 'INT_EQUAL',
      expression: tileId,
      negate: false
    })
    const whereClause = this.constraints2SQLWhereClause(q.constraints)

    const projectOptions = {
    }
    for (const i in this.fieldsList) {
      projectOptions[this.fieldsList[i].id] = 1
    }
    // Construct the SQL SELECT clause matching the given aggregate options
    let selectClause = 'SELECT '
    selectClause += this.fieldsList.filter(f => f.widget !== 'tags').map(f => that.fId2AlaSql(f.id)).map(k => 'MIN_MAX(' + k + ') as ' + k).join(', ')
    selectClause += ', ' + this.fieldsList.filter(f => f.widget === 'tags').map(f => that.fId2AlaSql(f.id)).map(k => 'VALUES_AND_COUNT(' + k + ') as ' + k).join(', ')
    selectClause += ', COUNT(*) as c, geogroup_id, FIRST(geometry) as geometry FROM features '
    let sqlStatement = selectClause + whereClause + ' GROUP BY geogroup_id'
    return alasql.promise(sqlStatement).then(function (res) {
      res = res.filter(f => f.c)
      const geojson = {
        type: 'FeatureCollection',
        features: []
      }
      for (const item of res) {
        const feature = {
          geometry: item.geometry,
          type: 'Feature',
          properties: item,
          geogroup_id: item.geogroup_id,
          geogroup_size: item.c
        }
        delete feature.properties.geometry
        delete feature.properties.geogroup_id
        delete feature.properties.c
        geojson.features.push(feature)
      }
      return geojson.features.length ? geojson : undefined
    })
  }

}
