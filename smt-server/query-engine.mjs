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

import Database from 'better-sqlite3'
import _ from 'lodash'
import filtrex from 'filtrex'
import hash_sum from 'hash-sum'
import turf from '@turf/turf'
import assert from 'assert'
import geo_utils from './geojson-utils.mjs'

const HEALPIX_ORDER = 5

export default {
  fieldsList: undefined,
  baseHashKey: '',
  sqlFields: undefined,
  fcounter: 0,

  db: undefined,

  fId2AlaSql: function (fieldId) {
    return fieldId.replace(/\./g, '_')
  },

  postProcessSQLiteResult: function (res)  {
    for (const i in res) {
      const item = res[i]
      if (typeof item === 'string' && item.startsWith('__JSON'))
        res[i] = JSON.parse(item.substring(6))
    }
  },

  fType2AlaSql: function (fieldType) {
    if (fieldType === 'string') return 'TEXT'
    if (fieldType === 'date') return 'INT' // Dates are converted to unix time stamp
    if (fieldType === 'number') return 'NUMERIC'
    return 'JSON'
  },

  initDB: async function (fieldsList, baseHashKey) {
    let that = this
    console.log('Create Data Base')
    const db = new Database(':memory:');

    // Add a custom aggregation operator for the chip tags
    db.aggregate('VALUES_AND_COUNT', {
      start: () => {},
      step: (accumulator, value) => {
        if (!accumulator) accumulator = {}
        if (!value) value = '__undefined'
        accumulator[value] = (accumulator[value] !== undefined) ? accumulator[value] + 1 : 1
        return accumulator
      },
      result: accumulator => accumulator ? '__JSON' + JSON.stringify(accumulator) : undefined
    })

    db.aggregate('MIN_MAX', {
      start: undefined,
      step: (accumulator, value) => {
        if (!accumulator) return [value, value]
        return [Math.min(accumulator[0], value), Math.max(accumulator[1], value)]
      },
      result: accumulator => accumulator ? '__JSON' + JSON.stringify(accumulator) : undefined
    })

    db.aggregate('GEO_UNION', {
      start: undefined,
      step: (accumulator, value) => {
        if (!accumulator) {
          const feature = {
            "type": "Feature",
            "geometry": _.cloneDeep(value)
          }
          const shiftCenter = turf.pointOnFeature(feature).geometry.coordinates

          // Compute shift matrices
          const center = geojsonPointToVec3(shiftCenter)
          const mats = geo_utils.rotationMatsForShiftCenter(center)
          feature.m = mats.m
          feature.mInv = mats.mInv
          geo_utils.rotateGeojsonFeature(feature, feature.m)
          return feature
        }
        try {
          const f2 =  {
            "type": "Feature",
            "geometry": _.cloneDeep(value)
          }
          geo_utils.rotateGeojsonFeature(f2, accumulator.m)
          const union = turf.union(accumulator, f2)
          accumulator.geometry = union.geometry
          return accumulator
        } catch (err) {
          console.log('Error computing feature union: ' + err)
          return accumulator
        }
      },
      result: accumulator => {
        if (!accumulator)
          return undefined
        geo_utils.rotateGeojsonFeature(accumulator, accumulator.mInv)
        return '__JSON' + JSON.stringify(accumulator.geometry)
      }
    })

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
    let sqlFieldsAndTypes = fieldsList.map(f => that.fId2AlaSql(f.id) + ' ' + that.fType2AlaSql(f.type)).join(', ')

    let info = db.prepare('CREATE TABLE features (id TEXT, geometry TEXT, healpix_index INT, geogroup_id TEXT, properties TEXT, ' + sqlFieldsAndTypes + ')').run()
    db.prepare('CREATE INDEX idx_id ON features(id)').run()
    db.prepare('CREATE INDEX idx_healpix_index ON features(healpix_index)').run()
    db.prepare('CREATE INDEX idx_geogroup_id ON features(geogroup_id)').run()

    // Create an index on each field
    for (let i in that.sqlFields) {
      const field = that.sqlFields[i]
      db.prepare('CREATE INDEX idx_' + i + ' ON features(' + field + ')').run()
    }

    that.db = db
  },

  loadAllData: async function (jsonData) {
    console.log('Loading ' + jsonData.features.length + ' features')
    let that = this

    geo_utils.normalizeGeoJson(jsonData)

    // Insert all data
    let subFeatures = []
    turf.featureEach(jsonData, function (feature, featureIndex) {
      if (feature.geometry.type === 'MultiPolygon') {
        geo_utils.unionMergeMultiPolygon(feature)
      }
      feature.geogroup_id = _.get(feature.properties, 'fieldID', undefined) || _.get(feature.properties, 'SurveyName', '') + _.get(feature, 'id', '')
      feature.id = that.fcounter++
      subFeatures = subFeatures.concat(geo_utils.splitOnHealpixGrid(feature, HEALPIX_ORDER))
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
      feature.geometry = '__JSON' + JSON.stringify(feature.geometry)
      feature.properties = '__JSON' + JSON.stringify(feature.properties)
    }
    const insert = that.db.prepare('INSERT INTO features VALUES (@id, @geometry, @healpix_index, @geogroup_id, @properties, ' + that.sqlFields.map(f => '@' + f).join(',') + ')')
    const insertMany = that.db.transaction((features) => {
      for (const feature of features)
        insert.run(feature)
    })
    insertMany(subFeatures)
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
        return fid + ' = \'' + c.expression + '\''
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

    // Use a sub-query to ensure that we don't return all pieces of features
    // split on each helpix pixel as separate entries
    // In some case, when the query work on healpix_index, we do want to
    // consider each pieces as a separated entry, in such case the onSubFeatures
    // flag has to be defined.
    const fromClause = ' FROM ' + (q.onSubFeatures ? 'features' : '(SELECT * FROM features GROUP BY id)')

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
          let req = 'SELECT MIN(' + fid + ') AS dmin, MAX(' + fid + ') AS dmax ' + fromClause + wc
          const res = that.db.prepare(req).get()
          that.postProcessSQLiteResult(res)
          if (!res.dmin || !res.dmax) {
            // No results
            let data = {
              min: undefined,
              max: undefined,
              step: '%Y-%m-%d',
              table: [['Date', 'Count']]
            }
            let retd = {}
            retd[agOpt.out] = data
            return { q: q, res: [retd] }
          }
          let start = new Date(res.dmin)
          start.setHours(0, 0, 0, 0)
          // Switch to next day and truncate
          let stop = new Date(res.dmax + 1000 * 60 * 60 * 24)
          stop.setHours(0, 0, 0, 0)
          // Range in days
          let range = (stop - start) / (1000 * 60 * 60 * 24)
          let step = '%Y-%m-%d'
          if (range > 3 * 365) {
            step = '%Y'
          } else if (range > 3 * 30) {
            step = '%Y-%m'
          }

          let sqlQ = 'SELECT COUNT(*) AS c, STRFTIME(\'' + step + '\', ROUND(' + fid + '/1000), \'unixepoch\') AS d ' + fromClause + wc + ' GROUP BY STRFTIME(\'' + step + '\', ROUND(' + fid + '/1000), \'unixepoch\')'
          const res2 = that.db.prepare(sqlQ).all()
          let data = {
            min: start,
            max: stop,
            step: step,
            table: [['Date', 'Count']]
          }
          for (let j in res2) {
            that.postProcessSQLiteResult(res2[j])
            data.table.push([res2[j].d, res2[j].c])
          }
          let retd = {}
          retd[agOpt.out] = data
          return { q: q, res: [retd] }
        } else if (agOpt.operation === 'NUMBER_HISTOGRAM') {
          // Special case, do custom queries and return
          console.assert(q.aggregationOptions.length === 1)
          let fid = that.fId2AlaSql(agOpt.fieldId)
          let wc = (whereClause === '') ? ' WHERE ' + fid + ' IS NOT NULL' : whereClause + ' AND ' + fid + ' IS NOT NULL'
          let req = 'SELECT MIN(' + fid + ') AS dmin, MAX(' + fid + ') AS dmax ' + fromClause + wc
          let stmt = that.db.prepare(req)
          const res = stmt.get()
          that.postProcessSQLiteResult(res)
          if (res.dmin === res.dmax) {
            // No results
            let data = {
              min: res.dmin,
              max: res.dmax,
              step: undefined,
              table: [['Value', 'Count']]
            }
            let retd = {}
            retd[agOpt.out] = data
            return { q: q, res: [retd] }
          }
          let step = (res.dmax - res.dmin) / 10

          let sqlQ = 'SELECT COUNT(*) AS c, ROUND((' + fid + ' - ' + res.dmin + ') / ' + step + ') * ' + step + ' AS d ' + fromClause + wc + ' GROUP BY ROUND((' + fid + ' - ' + res.dmin + ') / ' + step + ')'
          const res2 = that.db.prepare(sqlQ).all()
          let data = {
            min: res.dmin,
            max: res.dmax,
            step: step,
            table: [['Value', 'Count']]
          }
          for (let j in res2) {
            that.postProcessSQLiteResult(res2[j])
            data.table.push([res2[j].d, res2[j].c])
          }
          let retd = {}
          retd[agOpt.out] = data
          return { q: q, res: [retd] }
        } else {
          throw new Error('Unsupported aggregation operation: ' + agOpt.operation)
        }
      }
    }

    let sqlStatement = selectClause + fromClause + whereClause
    const res = that.db.prepare(sqlStatement).all()
    for (let i in res) {
      that.postProcessSQLiteResult(res[i])
    }
    return { q: q, res: res }
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
    return `hips_tile_format = geojson\nhips_order = 2\nhips_order_min = 1` +
        '\nhips_tile_width = 400\nobs_title = SMT Geojson'
  },

  getHipsTile: function (queryHash, order, tileId) {
    const that = this
    // Can be 0: lowest details, only plain healpix tiles shapes
    //        1: medium details, the union of all footprints per tile
    //        2: high details, the union of all footprints having the same geo_groupid
    let LOD_LEVEL = 2

    if (order === -1) {
      // Special case for Allsky
      tileId = -1
    } else if (order === 1) {
      LOD_LEVEL = 0
    }

    const q = _.cloneDeep(this.hashToQuery[queryHash])
    if (!q)
      return undefined
    const healPixScale = (tileId === -1) ? 1 : Math.pow(4, HEALPIX_ORDER - order)
    const queryTileId = tileId * healPixScale
    q.constraints.push({
      field: {id: 'healpix_index'},
      operation: 'NUMBER_RANGE',
      expression: [queryTileId, queryTileId + healPixScale - 1],
      negate: false
    })
    const whereClause = this.constraints2SQLWhereClause(q.constraints)

    // Construct the SQL SELECT clause matching the given aggregate options
    let selectClause = 'SELECT '
    selectClause += this.fieldsList.filter(f => f.widget !== 'tags').map(f => that.fId2AlaSql(f.id)).map(k => 'MIN_MAX(' + k + ') as ' + k).join(', ')
    selectClause += ', ' + this.fieldsList.filter(f => f.widget === 'tags').map(f => that.fId2AlaSql(f.id)).map(k => 'VALUES_AND_COUNT(' + k + ') as ' + k).join(', ')
    selectClause += ', COUNT(*) as c, healpix_index ' + (LOD_LEVEL === 0 ? '' : ', geogroup_id, geometry ') + 'FROM features '
    let sqlStatement = selectClause + whereClause + ' GROUP BY ' + (LOD_LEVEL > 1 ? 'healpix_index, geogroup_id, SurveyName' : 'healpix_index, SurveyName')
    const stmt = that.db.prepare(sqlStatement)
    let res = stmt.all()
    res = res.filter(f => f.c)
    const geojson = {
      type: 'FeatureCollection',
      features: []
    }
    for (const item of res) {
      that.postProcessSQLiteResult(item)
      const feature = {
        geometry: LOD_LEVEL === 0 ? geo_utils.getHealpixCornerFeature(HEALPIX_ORDER, item.healpix_index).geometry : item.geometry,
        type: 'Feature',
        properties: item,
        geogroup_id: item.geogroup_id,
        healpix_index: item.healpix_index,
        geogroup_size: item.c
      }
      delete feature.properties.geometry
      delete feature.properties.geogroup_id
      delete feature.properties.healpix_index
      delete feature.properties.c
      geojson.features.push(feature)
    }
    return geojson.features.length ? geojson : undefined
  }

}
