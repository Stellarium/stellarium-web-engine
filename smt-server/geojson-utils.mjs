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

import _ from 'lodash'
import healpix from '@hscmap/healpix'
import turf from '@turf/turf'
import assert from 'assert'
import glMatrix from 'gl-matrix'

// Use native JS array in glMatrix lib
glMatrix.glMatrix.setMatrixArrayType(Array)

const D2R = Math.PI / 180
const R2D = 180 / Math.PI
const HEALPIX_ORDER = 5
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


const healpixCornerFeatureCache = {}

export default {
  // Return the area of the feature in steradiant
  featureArea: function (feature) {
    return turf.area(feature) / (1000 * 1000) * 4 * Math.PI / 509600000
  },

  // Try to merge elements from a MultiPolygon into a single Polygon
  // This is useful if elements from a multipolygon overlap
  unionMergeMultiPolygon: function (feature) {
    assert(feature.geometry.type === 'MultiPolygon')
    let merged
    try {
      merged = turf.flattenReduce(feature, function (previousValue, currentFeature, featureIndex, multiFeatureIndex) {
        if (!previousValue)
          return currentFeature
        return turf.union(previousValue, currentFeature)
      })
    } catch (err) {
      // Can't merge this multipolygon, just give up
      // TODO try with rotation ?
      console.log('Error computing feature union: ' + err)
      return
    }
    if (merged.geometry.type === 'Polygon') {
      feature.geometry = merged.geometry
    }
  },

  normalizeGeoJson: function (geoData) {
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
  },

  rotateGeojsonFeature: function (feature, m) {
    turf.coordEach(feature, p => {rotateGeojsonPoint(p, m)})
  },

  getHealpixCornerFeature: function (order, pix) {
    const cacheKey = '' + order + '_' + pix
    if (cacheKey in healpixCornerFeatureCache)
      return healpixCornerFeatureCache[cacheKey]
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
    this.normalizeGeoJson(hppixel)
    healpixCornerFeatureCache[cacheKey] = hppixel
    return hppixel
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

  // Split the given feature in pieces on the healpix grid
  // Returns a list of features each with 'healpix_index' set to the matching
  // index.
  splitOnHealpixGrid: function (feature, order) {
    const that = this
    let area = that.featureArea(feature)
    // For large footprints, we return -1 so that it goes in the AllSky order
    if (area * STERADIAN_TO_DEG2 > 25) {
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
      const intersection = that.intersectionRobust(feature, hppixel, center)
      if (intersection === null) return
      const f = _.cloneDeep(feature)
      f['healpix_index'] = pix
      f.geometry = intersection.geometry
      ret.push(f)
    })
    if (ret.length === 0) {
    }
    return ret
  },

  intersectionRobust: function (feature1, feature2, shiftCenter) {
    const that = this
    assert(feature2.geometry.type === 'Polygon')
    if (feature1.geometry.type === 'MultiPolygon') {
      const featuresCollection = turf.flatten(feature1)
      let featuresIntersected = []
      turf.featureEach(featuresCollection, f => { const inte = that.intersectionRobust(f, feature2, shiftCenter); if (inte) featuresIntersected.push(inte) })
      if (featuresIntersected.length === 0) return null
      const combinedFc = turf.combine(turf.featureCollection(featuresIntersected))
      assert(combinedFc.features.length === 1)
      const f = _.cloneDeep(feature1)
      f.geometry = combinedFc.features[0].geometry
      return f
    }

    assert(feature1.geometry.type === 'Polygon')
    let q = glMatrix.quat.create()
    const center = geojsonPointToVec3(shiftCenter)
    glMatrix.quat.rotationTo(q, glMatrix.vec3.fromValues(center[0], center[1], center[2]), glMatrix.vec3.fromValues(1, 0, 0))
    const m = glMatrix.mat3.create()
    const mInv = glMatrix.mat3.create()
    glMatrix.mat3.fromQuat(m, q)
    glMatrix.mat3.invert(mInv, m)

    const f1 = _.cloneDeep(feature1)
    const f2 = _.cloneDeep(feature2)
    this.rotateGeojsonFeature(f1, m)
    this.rotateGeojsonFeature(f2, m)
    let res = null
    try {
      res = turf.intersect(f1, f2)
      if (res) this.rotateGeojsonFeature(res, mInv)
    } catch (err) {
      console.log('Error computing feature intersection: ' + err)
      res = null
    }
    return res
  }
}
