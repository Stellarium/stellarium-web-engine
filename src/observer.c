/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "observer.h"
#include "swe.h"

// Simple xor hash function.
static uint32_t hash_xor(uint32_t v, const char *data, int len)
{
    int i;
    // Only works on 4 bytes aligned data, either of size exactly equal to
    // 1 byte, or with a size that is a multiple of 4 bytes.
    assert((uintptr_t)data % 4 == 0);
    assert(len == 1 || len % 4 == 0);
    if (len == 1) return v ^ *data;
    for (i = 0; i < len; i += 4) {
        v ^= *(uint32_t*)(data + i);
    }
    return v;
}

static void update_matrices(observer_t *obs)
{
    eraASTROM *astrom = &obs->astrom;
    // We work with 3x3 matrices, so that we can use the erfa functions.
    double rm2v[3][3];  // Rotate from mount to view.
    double ro2m[3][3];  // Rotate from observed to mount.
    double ro2v[3][3];  // Rotate from observed to view.
    double ri2h[3][3];  // Equatorial J2000 (ICRF) to horizontal.
    double rh2i[3][3];  // Horizontal to Equatorial J2000 (ICRF).
    double ri2v[3][3];  // Equatorial J2000 (ICRF) to view.
    double ri2e[3][3];  // Equatorial J2000 (ICRF) to ecliptic.
    double re2i[3][3];  // Eclipic to Equatorial J2000 (ICRF).
    double rc2v[3][3];
    double view_rot[3][3];

    quat_to_mat3(obs->mount_quat, ro2m);
    mat3_set_identity(rm2v);
    // r2gl changes the coordinate from z up to y up orthonomal.
    double r2gl[3][3] = {{0, 0,-1},
                         {1, 0, 0},
                         {0, 1, 0}};
    mat3_rz(obs->roll, rm2v, rm2v);
    mat3_rx(-obs->pitch, rm2v, rm2v);
    mat3_ry(obs->yaw, rm2v, rm2v);
    mat3_mul(rm2v, r2gl, rm2v);
    mat3_mul(rm2v, ro2m, ro2v);

    // Extra rotation for screen center offset.
    assert(!isnan(obs->view_offset_alt));
    mat3_set_identity(view_rot);
    mat3_rx(obs->view_offset_alt, view_rot, view_rot);
    mat3_mul(view_rot, ro2v, ro2v);

    // Compute rotation matrix from CIRS to horizontal.
    mat3_set_identity(ri2h);
    // Earth rotation.
    mat3_rz(astrom->eral, ri2h, ri2h);
    // Polar motion.
    double rpl[3][3] = {{1, 0, astrom->xpl},
                        {0, 1, astrom->ypl},
                        {astrom->xpl, astrom->ypl, 1}};
    mat3_mul(ri2h, rpl, ri2h);
    // Cartesian -HA,Dec to Cartesian Az,El (S=0,E=90).
    mat3_ry(-obs->phi + M_PI / 2, ri2h, ri2h);
    double rsx[3][3] = {{-1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    mat3_mul(ri2h, rsx, ri2h);
    mat3_transpose(ri2h, ri2h);

    // Also store its inverse.
    mat3_invert(ri2h, rh2i);
    mat3_mul(ro2v, ri2h, ri2v);

    // Equatorial to ecliptic
    mat3_set_identity(re2i);
    mat3_rx(eraObl80(DJM0, obs->ut1), re2i, re2i);
    mat3_invert(re2i, ri2e);

    // ICRF to view (ignoring refraction).
    mat3_transpose(astrom->bpn, rc2v);
    mat3_mul(ri2h, rc2v, rc2v);
    mat3_mul(ro2v, rc2v, rc2v);

    // Copy all
    mat3_copy(ro2m, obs->ro2m);
    mat3_copy(ro2v, obs->ro2v);
    mat3_invert(obs->ro2v, obs->rv2o);
    mat3_copy(ri2h, obs->ri2h);
    mat3_copy(rh2i, obs->rh2i);
    mat3_copy(ri2v, obs->ri2v);
    mat3_copy(ri2e, obs->ri2e);
    mat3_copy(re2i, obs->re2i);
    mat3_copy(rc2v, obs->rc2v);
}

static void observer_compute_hash(observer_t *obs, uint64_t* hash_partial,
                                  uint64_t* hash)
{
    uint32_t v = 1;
    #define H(a) v = hash_xor(v, (const char*)&obs->a, sizeof(obs->a))
    H(elong);
    H(phi);
    H(hm);
    H(horizon);
    H(pressure);
    H(refraction);
    *hash_partial = v;
    H(mount_quat);
    H(pitch);
    H(yaw);
    H(roll);
    H(view_offset_alt);
    H(tt);
    #undef H
    *hash = v;
}

static void correct_speed_of_light(double pv[2][3]) {
    double ldt = vec3_norm(pv[0]) * DAU / LIGHT_YEAR_IN_METER * DJY;
    vec3_addk(pv[0], pv[1], -ldt, pv[0]);
}


static void update_nutation_precession_mat(observer_t *obs)
{
    // XXX: we can maybe optimize this, since eraPn00a is very slow!
    double dpsi, deps, epsa, rb[3][3], rp[3][3], rbp[3][3], rn[3][3],
           rbpn[3][3];
    eraPn00a(obs->tt, DJM0, &dpsi, &deps, &epsa, rb, rp, rbp, rn, rbpn);
    mat3_mul(rn, rp, obs->rnp);

}

void observer_update(observer_t *obs, bool fast)
{
    double utc1, utc2, ut11, ut12, tai1, tai2;
    double dt, dut1 = 0;
    double p[3] = {0};

    uint64_t hash, hash_partial;
    observer_compute_hash(obs, &hash_partial, &hash);
    // Check if we have computed accurate positions already
    if (hash == obs->hash_accurate)
        return;
    // Check if we have computed 'fast' positions already
    if (fast && hash == obs->hash)
        return;
    fast = fast && hash_partial == obs->hash_partial &&
            fabs(obs->last_accurate_update - obs->tt) < 1.0;

    // Compute UT1 and UTC time.
    if (obs->last_update != obs->tt) {
        dt = deltat(obs->tt);
        dut1 = 0;
        eraTtut1(DJM0, obs->tt, dt, &ut11, &ut12);
        eraTttai(DJM0, obs->tt, &tai1, &tai2);
        eraTaiutc(tai1, tai2, &utc1, &utc2);
        obs->ut1 = ut11 - DJM0 + ut12;
        obs->utc = utc1 - DJM0 + utc2;
    }

    if (fast) {
        if (obs->last_update != obs->tt) {
            eraAper13(DJM0, obs->ut1, &obs->astrom);
            eraPvu(obs->tt - obs->last_update, obs->earth_pvh, obs->earth_pvh);
            eraPvu(obs->tt - obs->last_update, obs->earth_pvb, obs->earth_pvb);
        }
    } else {
        eraApco13(DJM0, obs->utc, dut1,
                obs->elong, obs->phi,
                obs->hm,
                0, 0,
                obs->refraction ? obs->pressure : 0,
                15,       // Temperature (dec C)
                0.5,      // Relative humidity (0-1)
                0.55,     // Effective color (micron),
                &obs->astrom,
                &obs->eo);
        // Update earth position.
        eraEpv00(DJM0, obs->tt, obs->earth_pvh, obs->earth_pvb);
        eraCp(obs->astrom.eb, obs->obs_pvb[0]);
        vec3_mul(ERFA_DC, obs->astrom.v, obs->obs_pvb[1]);
        eraPvmpv(obs->obs_pvb, obs->earth_pvb, obs->obs_pvg);
        // Update refraction constants.
        refraction_prepare(obs->pressure, 15, 0.5, &obs->refa, &obs->refb);
    }
    eraPvmpv(obs->earth_pvb, obs->earth_pvh, obs->sun_pvb);

    if (fast && obs->last_update != obs->tt) {
        // Update observer geocentric position obs_pvg. We can't use eraPvu here
        // as the movement is a rotation about the earth center and can't
        // be approximated by a linear velocity  on a 24h time span
        double theta = eraEra00(DJM0, obs->ut1);
        eraPvtob(obs->elong, obs->phi, obs->hm, 0, 0, 0, theta, obs->obs_pvg);
        // Rotate from CIRS to ICRF
        eraTrxp(obs->astrom.bpn, obs->obs_pvg[0], obs->obs_pvg[0]);
        eraTrxp(obs->astrom.bpn, obs->obs_pvg[1], obs->obs_pvg[1]);
        // Set pos back in AU
        eraSxp(1. / DAU, obs->obs_pvg[0], obs->obs_pvg[0]);
        // Set speed back in AU / day
        eraSxp(ERFA_DAYSEC / DAU, obs->obs_pvg[1], obs->obs_pvg[1]);

        // Compute the observer's barycentric position
        eraPvppv(obs->earth_pvb, obs->obs_pvg, obs->obs_pvb);
    }

    update_matrices(obs);
    if (!fast) update_nutation_precession_mat(obs);

    // Compute sun's apparent position in observer reference frame
    eraPvmpv(obs->sun_pvb, obs->obs_pvb, obs->sun_pvo);
    // Correct in one shot space motion, annual & diurnal abberrations
    correct_speed_of_light(obs->sun_pvo);

    obs->last_update = obs->tt;
    obs->hash_partial = hash_partial;
    obs->hash = hash;
    if (!fast) {
        obs->hash_accurate = hash;
        obs->last_accurate_update = obs->tt;
    }

    // Compute pointed at constellation.
    eraS2c(obs->yaw, obs->pitch, p);
    mat3_mul_vec3(obs->rh2i, p, p);
    find_constellation_at(p, obs->cst);
}

static int observer_init(obj_t *obj, json_value *args)
{
    observer_t*  obs = (observer_t*)obj;
    quat_set_identity(obs->mount_quat);
    observer_compute_hash(obs, &obs->hash_partial, &obs->hash_accurate);
    obs->hash = obs->hash_accurate;
    return 0;
}

static obj_t *observer_clone(const obj_t *obj)
{
    observer_t *ret;
    ret = (observer_t*)obj_create("observer", NULL, NULL);
    // Copy all except obj attributes.
    memcpy(((char*)ret) + sizeof(obj_t), ((char*)obj) + sizeof(obj_t),
           sizeof(*ret) - sizeof(obj_t));
    return &ret->obj;
}

static void observer_on_timeattr_changed(obj_t *obj, const attribute_t *attr)
{
    // Make sure that the TT is synced.
    observer_t *obs = (observer_t*)obj;
    double ut11, ut12, tai1, tai2, tt1, tt2, dt = 0;
    if (strcmp(attr->name, "utc") == 0) {
        // First compute UTC -> TAI -> TT (no deltaT involved)
        eraUtctai(DJM0, obs->utc, &tai1, &tai2);
        eraTaitt(tai1, tai2, &tt1, &tt2);
        obs->tt = tt1 - DJM0 + tt2;

        dt = deltat(obs->utc);
        eraTtut1(tt1, tt2, dt, &ut11, &ut12);
        obs->ut1 = ut11 - DJM0 + ut12;
    }
    if (strcmp(attr->name, "ut1") == 0) {
        dt = deltat(obs->ut1);
        eraUt1tt(DJM0, obs->ut1, dt, &tt1, &tt2);
        obs->tt = tt1 - DJM0 + tt2;
    }
    module_changed(obj, "tt");
    module_changed(obj, "utc");
}

// Expose azalt vector to js.
static json_value *observer_get_azalt(obj_t *obj, const attribute_t *attr,
                                 const json_value *args)
{
    observer_t *obs = (observer_t*)obj;
    double v[3];
    eraS2c(obs->yaw, obs->pitch, v);
    return args_value_new(TYPE_V3, v);
}


static obj_klass_t observer_klass = {
    .id = "observer",
    .size = sizeof(observer_t),
    .flags = OBJ_IN_JSON_TREE,
    .init = observer_init,
    .clone = observer_clone,
    .attributes = (attribute_t[]) {
        PROPERTY(longitude, TYPE_ANGLE, MEMBER(observer_t, elong)),
        PROPERTY(latitude, TYPE_ANGLE, MEMBER(observer_t, phi)),
        PROPERTY(elevation, TYPE_FLOAT, MEMBER(observer_t, hm)),
        PROPERTY(refraction, TYPE_BOOL, MEMBER(observer_t, refraction)),
        PROPERTY(tt, TYPE_MJD, MEMBER(observer_t, tt),
                 .on_changed = observer_on_timeattr_changed),
        PROPERTY(ut1, TYPE_MJD, MEMBER(observer_t, ut1),
                 .on_changed = observer_on_timeattr_changed),
        PROPERTY(utc, TYPE_MJD, MEMBER(observer_t, utc),
                 .on_changed = observer_on_timeattr_changed),
        PROPERTY(pitch, TYPE_ANGLE, MEMBER(observer_t, pitch)),
        PROPERTY(yaw, TYPE_ANGLE, MEMBER(observer_t, yaw)),
        PROPERTY(roll, TYPE_ANGLE, MEMBER(observer_t, roll)),
        PROPERTY(view_offset_alt, TYPE_ANGLE,
                 MEMBER(observer_t, view_offset_alt)),
        PROPERTY(azalt, TYPE_V3, .fn = observer_get_azalt),
        {}
    },
};
OBJ_REGISTER(observer_klass)

