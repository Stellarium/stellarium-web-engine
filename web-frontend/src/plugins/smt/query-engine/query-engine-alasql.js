// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

import alasql from 'alasql'
import _ from 'lodash'
import filtrex from 'filtrex'

export default {
  fieldsList: undefined,
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

  initDB: async function (fieldsList) {
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

    let that = this
    console.log('Create Data Base')
    that.fieldsList = fieldsList
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
    await alasql.promise('CREATE TABLE features (id INT, geometry JSON, properties JSON, ' + sqlFieldsAndTypes + ')')

    await alasql.promise('CREATE INDEX idx_id ON features(id)')
    // Create an index on each field
    for (let i in that.sqlFields) {
      let field = that.sqlFields[i]
      await alasql.promise('CREATE INDEX idx_' + i + ' ON features(' + field + ')')
    }
  },

  loadAllData: async function (jsonData) {
    console.log('Loading ' + jsonData.features.length + ' features')
    let that = this

    // Insert all data
    for (let feature of jsonData.features) {
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
      feature.id = that.fcounter++
    }
    await alasql.promise('SELECT * INTO features FROM ?', [jsonData.features])
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
      throw err.response.body
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
      } else if (c.operation === 'IS_UNDEFINED') {
        return fid + ' IS NULL'
      } else if (c.operation === 'DATE_RANGE') {
        return '( ' + fid + ' IS NOT NULL AND ' + fid + ' >= ' + c.expression[0] +
          ' AND ' + fid + ' <= ' + c.expression[1] + ')'
      } else if (c.operation === 'NUMBER_RANGE') {
        return '( ' + fid + ' IS NOT NULL AND ' + fid + ' >= ' + c.expression[0] +
          ' AND ' + fid + ' <= ' + c.expression[1] + ')'
      } else if (c.operation === 'IN') {
        return '( ' + fid + ' IN (' + c.expression.map(e => '' + e).join(', ') + ') )'
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
              return [retd]
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
  }
}
