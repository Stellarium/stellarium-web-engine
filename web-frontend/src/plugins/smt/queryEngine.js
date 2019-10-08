// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

import alasql from 'alasql'
import _ from 'lodash'

export default {
  fieldsList: undefined,
  sqlFields: undefined,

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
    that.sqlFields = fieldsList.map(f => that.fId2AlaSql(f.id))
    let sqlFieldsAndTypes = that.sqlFields.map(f => f + ' ' + that.fType2AlaSql(f.type)).join(', ')
    await alasql.promise('CREATE TABLE features (geometry JSON, properties JSON, ' + sqlFieldsAndTypes + ')')

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
    let req = alasql.compile('INSERT INTO features VALUES (?, ?, ' + that.fieldsList.map(f => '?').join(', ') + ')')
    for (let feature of jsonData.features) {
      let arr = [feature.geometry, feature.properties]
      for (let i in that.fieldsList) {
        let d = _.get(feature.properties, that.fieldsList[i].id, undefined)
        if (d !== undefined && that.fieldsList[i].type === 'date') {
          d = new Date(d).getTime()
        }
        arr.push(d)
      }
      await req.promise(arr)
    }
  },

  // Construct the SQL WHERE clause matching the given constraints
  constraints2SQLWhereClause: function (constraints) {
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
      let fid = this.fId2AlaSql(c.field.id)
      if (c.operation === 'STRING_EQUAL') {
        return fid + ' = "' + c.expression + '"'
      } else if (c.operation === 'IS_UNDEFINED') {
        return fid + ' IS NULL'
      } else if (c.operation === 'DATE_RANGE') {
        return '( ' + fid + ' IS NOT NULL AND ' + fid + ' >= ' + c.expression[0] +
          ' AND ' + fid + ' <= ' + c.expression[1] + ')'
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
  query: function (query) {
    let that = this
    let whereClause = this.constraints2SQLWhereClause(query.constraints)

    // Construct the SQL SELECT clause matching the given aggregate options
    let selectClause = 'SELECT '
    if (query.projectOptions) {
      selectClause += Object.keys(query.projectOptions).map(k => that.fId2AlaSql(k)).join(', ')
    }
    selectClause += 'FROM features'
    return alasql.promise(selectClause + whereClause)
  }
}
