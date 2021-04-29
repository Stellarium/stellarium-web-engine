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
#include "algos/utctt.h"

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
    double rdir[3][3];
    double ro2v[3][3];  // Rotate from observed to view.
    double ri2h[3][3];  // Equatorial J2000 (ICRF) to horizontal.
    double rh2i[3][3];  // Horizontal to Equatorial J2000 (ICRF).
    double ri2v[3][3];  // Equatorial J2000 (ICRF) to view.
    double ri2e[3][3];  // Equatorial J2000 (ICRF) to ecliptic.
    double re2i[3][3];  // Eclipic to Equatorial J2000 (ICRF).
    double rc2v[3][3];
    double view_rot[3][3];
    // r2gl changes the coordinate from z up to y up orthonomal.
    const double r2gl[3][3] = {{ 0, 0, -1},
                               {-1, 0,  0},
                               { 0, 1,  0}};

    const double flip_y[3][3] = {{1,  0, 0},
                                 {0, -1, 0},
                                 {0,  0, 1}};

    mat3_set_identity(rdir);
    mat3_rx(obs->roll, rdir, rdir);
    mat3_ry(obs->pitch, rdir, rdir);
    mat3_rz(-obs->yaw, rdir, rdir);

    if (mat3_det(obs->ro2m) > 0)
        mat3_product(ro2v, 4, r2gl, flip_y, rdir, obs->ro2m);
    else
        mat3_product(ro2v, 3, r2gl, rdir, obs->ro2m);

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
    eraEcm06(DJM0, obs->tt, re2i);
    mat3_invert(re2i, ri2e);

    // ICRF to view (ignoring refraction).
    mat3_transpose(astrom->bpn, rc2v);
    mat3_mul(ri2h, rc2v, rc2v);
    mat3_mul(ro2v, rc2v, rc2v);

    // Copy all
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
    H(space);
    *hash_partial = v;
    H(ro2m);
    H(pitch);
    H(yaw);
    H(roll);
    H(view_offset_alt);
    H(tt);
    if (obs->space) H(obs_pvg);
    #undef H
    *hash = v;
}

static void correct_speed_of_light(double pv[2][3]) {
    double ldt = vec3_norm(pv[0]) * DAU2M / LIGHT_YEAR_IN_METER * DJY;
    vec3_addk(pv[0], pv[1], -ldt, pv[0]);
}


static void update_nutation_precession_mat(observer_t *obs)
{
    // XXX: we can maybe optimize this, since eraPn00a is very slow!
    double dpsi, deps, epsa, rb[3][3], rp[3][3], rbp[3][3], rn[3][3],
           rbpn[3][3];
    eraPn00a(DJM0, obs->tt, &dpsi, &deps, &epsa, rb, rp, rbp, rn, rbpn);
    mat3_mul(rn, rp, obs->rnp);

}

static void observer_update_fast(observer_t *obs)
{
    double dut1, theta, pvg[2][3];

    // Compute UT1 and UTC time.
    if (!obs->space && obs->last_update == obs->tt) goto end;

    obs->utc = tt2utc(obs->tt, &dut1);
    obs->ut1 = obs->utc + dut1 / ERFA_DAYSEC;

    if (!obs->space)
        eraAper13(DJM0, obs->ut1, &obs->astrom);
    eraPvu(obs->tt - obs->last_update, obs->earth_pvh, obs->earth_pvh);
    eraPvu(obs->tt - obs->last_update, obs->earth_pvb, obs->earth_pvb);

    if (!obs->space) {
        // Update observer geocentric position obs_pvg. We can't use eraPvu
        // here as the movement is a rotation about the earth center and can't
        // be approximated by a linear velocity  on a 24h time span
        theta = eraEra00(DJM0, obs->ut1);
        eraPvtob(obs->elong, obs->phi, obs->hm, 0, 0, 0, theta, obs->obs_pvg);
        // Rotate from CIRS to ICRF
        eraTrxp(obs->astrom.bpn, obs->obs_pvg[0], obs->obs_pvg[0]);
        eraTrxp(obs->astrom.bpn, obs->obs_pvg[1], obs->obs_pvg[1]);
        // Set pos back in AU
        eraSxp(DM2AU, obs->obs_pvg[0], obs->obs_pvg[0]);
        // Set speed back in AU / day
        eraSxp(ERFA_DAYSEC * DM2AU, obs->obs_pvg[1], obs->obs_pvg[1]);
    } else {
        vec3_mul(DAU2M, obs->obs_pvg[0], pvg[0]);
        vec3_mul(DAU2M / ERFA_DAYSEC, obs->obs_pvg[1], pvg[1]);
        eraApcs(DJM0, obs->tt, pvg, obs->earth_pvb, obs->earth_pvh[0],
                &obs->astrom);
    }

    // Compute the observer's barycentric position
    eraPvppv(obs->earth_pvb, obs->obs_pvg, obs->obs_pvb);

end:
    update_matrices(obs);
    eraPvmpv(obs->earth_pvb, obs->earth_pvh, obs->sun_pvb);
    // Compute sun's apparent position in observer reference frame
    eraPvmpv(obs->sun_pvb, obs->obs_pvb, obs->sun_pvo);
    // Correct in one shot space motion, annual & diurnal abberrations
    correct_speed_of_light(obs->sun_pvo);
}

static void observer_update_full(observer_t *obs)
{
    double dut1, r[3][3], x, y, theta, s, sp, pvg[2][3];

    // Compute UT1 and UTC time.
    if (obs->last_update != obs->tt) {
        obs->utc = tt2utc(obs->tt, &dut1);
        obs->ut1 = obs->utc + dut1 / ERFA_DAYSEC;
    }

    // This is similar to a single call to eraApco13, except we handle
    // the time conversion ourself, since erfa doesn't support dates
    // before year -4800.
    eraPnm06a(DJM0, obs->tt, r); // equinox based BPN matrix.
    eraBpn2xy(r, &x, &y); // Extract CIP X,Y.
    s = eraS06(DJM0, obs->tt, x, y); // Obtain CIO locator s.
    // XXX: should be obs->ut1 here!  But it break the unit tests for now.
    theta = eraEra00(DJM0, obs->utc); // Earth rotation angle.
    sp = eraSp00(DJM0, obs->tt); // TIO locator s'.

    eraEpv00(DJM0, obs->tt, obs->earth_pvh, obs->earth_pvb);

    if (!obs->space) {
        eraApco(DJM0, obs->tt, obs->earth_pvb, obs->earth_pvh[0], x, y, s,
                theta, obs->elong, obs->phi, obs->hm, 0, 0, sp, 0, 0,
                &obs->astrom);
    } else {
        vec3_mul(DAU2M, obs->obs_pvg[0], pvg[0]);
        vec3_mul(DAU2M / ERFA_DAYSEC, obs->obs_pvg[1], pvg[1]);
        eraApcs(DJM0, obs->tt, pvg, obs->earth_pvb, obs->earth_pvh[0],
                &obs->astrom);
    }
    obs->eo = eraEors(r, s); // Equation of origins.

    // Update earth position.
    eraCp(obs->astrom.eb, obs->obs_pvb[0]);
    vec3_mul(ERFA_DC, obs->astrom.v, obs->obs_pvb[1]);
    if (!obs->space)
        eraPvmpv(obs->obs_pvb, obs->earth_pvb, obs->obs_pvg);
    // Update refraction constants.
    refraction_prepare(obs->pressure, 15, 0.5, &obs->refa, &obs->refb);
    update_nutation_precession_mat(obs);

    update_matrices(obs);
    eraPvmpv(obs->earth_pvb, obs->earth_pvh, obs->sun_pvb);
    // Compute sun's apparent position in observer reference frame
    eraPvmpv(obs->sun_pvb, obs->obs_pvb, obs->sun_pvo);
    // Correct in one shot space motion, annual & diurnal abberrations
    correct_speed_of_light(obs->sun_pvo);
}

void observer_update(observer_t *obs, bool fast)
{

    uint64_t hash, hash_partial;

    observer_compute_hash(obs, &hash_partial, &hash);
    // Check if we have computed accurate positions already
    if (hash == obs->hash)
        return;

    // Check if we have computed 'fast' positions already
    if (fast) {
        // Add one to the hash for the fast update hash value.
        hash++;
        if (hash == obs->hash)
            return;
        if (    hash_partial != obs->hash_partial ||
                fabs(obs->last_accurate_update - obs->tt) >= 1.0)
            fast = false;
    }

    if (fast)
        observer_update_fast(obs);
    else
        observer_update_full(obs);

    obs->last_update = obs->tt;
    obs->hash_partial = hash_partial;
    obs->hash = hash;
    if (!fast)
        obs->last_accurate_update = obs->tt;
}

static int observer_init(obj_t *obj, json_value *args)
{
    observer_t*  obs = (observer_t*)obj;
    mat3_set_identity(obs->ro2m);
    observer_compute_hash(obs, &obs->hash_partial, &obs->hash);
    return 0;
}

static obj_t *observer_clone(const obj_t *obj)
{
    observer_t *ret;
    ret = (observer_t*)obj_create("observer", NULL);
    // Copy all except obj attributes.
    memcpy(((char*)ret) + sizeof(obj_t), ((char*)obj) + sizeof(obj_t),
           sizeof(*ret) - sizeof(obj_t));
    return &ret->obj;
}

static void on_utc_changed(obj_t *obj, const attribute_t *attr)
{
    // Sync TT.
    observer_t *obs = (observer_t*)obj;
    obs->tt = utc2tt(obs->utc);
    module_changed(obj, "tt");
}

static void on_tt_changed(obj_t *obj, const attribute_t *attr)
{
    // Sync UTC.
    observer_t *obs = (observer_t*)obj;
    double dut1;
    obs->utc = tt2utc(obs->tt, &dut1);
    obs->ut1 = obs->utc + dut1 / ERFA_DAYSEC;
    module_changed(obj, "utc");
}

bool observer_is_uptodate(const observer_t *obs, bool fast)
{
    uint64_t hash, hash_partial;
    observer_compute_hash(obs, &hash_partial, &hash);
    if (hash == obs->hash) return true;
    if (fast && (hash + 1 == obs->hash)) return true;
    return false;
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
        PROPERTY(tt, TYPE_MJD, MEMBER(observer_t, tt),
                 .on_changed = on_tt_changed),
        PROPERTY(utc, TYPE_MJD, MEMBER(observer_t, utc),
                 .on_changed = on_utc_changed),
        PROPERTY(pitch, TYPE_ANGLE, MEMBER(observer_t, pitch)),
        PROPERTY(yaw, TYPE_ANGLE, MEMBER(observer_t, yaw)),
        PROPERTY(roll, TYPE_ANGLE, MEMBER(observer_t, roll)),
        PROPERTY(view_offset_alt, TYPE_ANGLE,
                 MEMBER(observer_t, view_offset_alt)),
        PROPERTY(azalt, TYPE_V3, .fn = observer_get_azalt),
        PROPERTY(space, TYPE_BOOL, MEMBER(observer_t, space)),
        {}
    },
};
OBJ_REGISTER(observer_klass)

