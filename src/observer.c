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

static void update_matrices(observer_t *obs)
{
    eraASTROM *astrom = &obs->astrom;
    // We work with 3x3 matrices, so that we can use the erfa functions.
    double ro2v[3][3];  // Rotate from observed to view.
    double ri2h[3][3];  // Equatorial J2000 (ICRS) to horizontal.
    double rh2i[3][3];  // Horizontal to Equatorial J2000 (ICRS).
    double ri2v[3][3];  // Equatorial J2000 (ICRS) to view.
    double ri2e[3][3];  // Equatorial J2000 (ICRS) to ecliptic.
    double re2i[3][3];  // Eclipic to Equatorial J2000 (ICRS).
    double re2h[3][3];  // Ecliptic to horizontal.
    double re2v[3][3];  // Ecliptic to view.

    mat3_set_identity(ro2v);
    // r2gl changes the coordinate from z up to y up orthonomal.
    double r2gl[3][3] = {{0, 0,-1},
                         {1, 0, 0},
                         {0, 1, 0}};
    mat3_rz(obs->roll, ro2v, ro2v);
    mat3_rx(-obs->altitude, ro2v, ro2v);
    mat3_ry(obs->azimuth, ro2v, ro2v);
    mat3_mul(ro2v, r2gl, ro2v);

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

    // Ecliptic to horizontal.
    mat3_copy(ri2h, re2h);
    mat3_rx(eraObl80(DJM0, obs->ut1), re2h, re2h);
    mat3_mul(ro2v, re2h, re2v);

    // Convert all to 4x4 matrices.
    mat3_to_mat4(ro2v, obs->ro2v);
    mat3_to_mat4(ri2h, obs->ri2h);
    mat3_to_mat4(rh2i, obs->rh2i);
    mat3_to_mat4(ri2v, obs->ri2v);
    mat3_to_mat4(ri2e, obs->ri2e);
    mat3_to_mat4(re2i, obs->re2i);
    mat3_to_mat4(re2h, obs->re2h);
    mat3_to_mat4(re2v, obs->re2v);
}

void observer_recompute_hash(observer_t *obs)
{
    uint64_t v = 0;
    #define H(a) v = crc64(v, &obs->a, sizeof(obs->a))
    H(elong);
    H(phi);
    H(hm);
    H(horizon);
    H(pressure);
    H(refraction);
    H(altitude);
    H(azimuth);
    H(roll);
    H(tt);
    #undef H
    obs->hash = v;
}

void observer_update(observer_t *obs, bool fast)
{
    double utc1, utc2, ut11, ut12;
    double dt, dut1 = 0;
    double pvb[2][3];
    double p[4] = {0};

    if (!fast || obs->force_full_update) obs->dirty = true;
    if (obs->last_update != obs->tt) obs->dirty = true;
    if (!obs->dirty) return;

    // Compute UT1 and UTC time.
    dt = deltat(obs->tt);
    dut1 = 0;
    eraTtut1(DJM0, obs->tt, dt, &ut11, &ut12);
    eraUt1utc(ut11, ut12, dut1, &utc1, &utc2);
    obs->ut1 = ut11 - DJM0 + ut12;
    obs->utc = utc1 - DJM0 + utc2;

    if (obs->force_full_update || fabs(obs->last_full_update - obs->tt) > 1)
        fast = false;

    if (fast) {
        eraAper13(DJM0, obs->ut1, &obs->astrom);
        eraPvu(obs->tt - obs->last_update, obs->earth_pvh, obs->earth_pvh);
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
        eraEpv00(DJM0, obs->tt, obs->earth_pvh, pvb);
        obs->last_full_update = obs->tt;
    }

    obs->last_update = obs->tt;
    update_matrices(obs);

    // Compute pointed at constellation.
    eraS2c(obs->azimuth, obs->altitude, p);
    mat4_mul_vec4(obs->rh2i, p, obs->pointer.icrs);
    find_constellation_at(obs->pointer.icrs, obs->pointer.cst);

    obs->dirty = false;
    obs->force_full_update = false;
    observer_recompute_hash(obs);
}

static int city_get_choices(
        int (*f)(const char* name, uint64_t oid, void *user),
        void *user)
{
    // XXX: we are iterating all the registered names, this is too slow.
    // We should have a way to index the objects by catalog in identifiers.
    const char *cat, *value, *search_str;
    uint64_t oid;
    IDENTIFIERS_ITER(0, "NAME", &oid, &cat, &value, &search_str, NULL) {
        if (oid_is_catalog(oid, "CITY"))
            if (f(value, oid, user)) return 0;
    }
    return 0;
}

static int observer_init(obj_t *obj, json_value *args)
{
    observer_recompute_hash((observer_t*)obj);
    return 0;
}

static obj_t *observer_clone(const obj_t *obj)
{
    observer_t *ret;
    ret = (observer_t*)obj_create("observer", "observer", obj->parent, NULL);
    // Copy all except obj attributes.
    memcpy(((char*)ret) + sizeof(obj_t), ((char*)obj) + sizeof(obj_t),
           sizeof(*ret) - sizeof(obj_t));
    return &ret->obj;
}

static void observer_on_changed(obj_t *obj, const attribute_t *attr)
{
    observer_t *obs = (observer_t*)obj;
    obs->dirty = true;
    obs->force_full_update = true;
    observer_recompute_hash(obs);
}

static void observer_on_timeattr_changed(obj_t *obj, const attribute_t *attr)
{
    // Make sure that the TT is synced.
    observer_t *obs = (observer_t*)obj;
    double ut11, ut12, tt1, tt2, dt, dut1 = 0;
    if (strcmp(attr->name, "utc") == 0) {
        dt = deltat(obs->utc);
        eraUtcut1(DJM0, obs->utc, dut1, &ut11, &ut12);
        eraUt1tt(ut11, ut12, dt, &tt1, &tt2);
        obs->ut1 = ut11 - DJM0 + ut12;
        obs->tt = tt1 - DJM0 + tt2;
    }
    if (strcmp(attr->name, "ut1") == 0) {
        dt = deltat(obs->ut1);
        eraUt1tt(DJM0, obs->ut1, dt, &tt1, &tt2);
        obs->tt = tt1 - DJM0 + tt2;
    }
    obs->dirty = true;
    observer_recompute_hash(obs);
}

// Expose azalt vector to js.
static json_value *observer_get_azalt(obj_t *obj, const attribute_t *attr,
                                 const json_value *args)
{
    observer_t *obs = (observer_t*)obj;
    double v[3];
    eraS2c(obs->azimuth, obs->altitude, v);
    return args_value_new("v3", "azalt", v);
}

static void observer_on_city_changed(obj_t *obj, const attribute_t *attr)
{
    observer_t *obs = (observer_t*)obj;
    double lat, lon, el;
    if (!obs->city) return;
    obj_get_attr(obs->city, "latitude", "f", &lat);
    obj_get_attr(obs->city, "longitude", "f", &lon);
    obj_get_attr(obs->city, "elevation", "f", &el);
    obj_set_attr(obj, "latitude", "f", lat);
    obj_set_attr(obj, "longitude", "f", lon);
    obj_set_attr(obj, "elevation", "f", el);
}


static obj_klass_t observer_klass = {
    .id = "observer",
    .size = sizeof(observer_t),
    .flags = OBJ_IN_JSON_TREE,
    .init = observer_init,
    .clone = observer_clone,
    .attributes = (attribute_t[]) {
        PROPERTY("longitude", "f", MEMBER(observer_t, elong),
                 .hint = "d_angle",
                 .on_changed = observer_on_changed),
        PROPERTY("latitude", "f", MEMBER(observer_t, phi),
                 .hint = "d_angle",
                 .on_changed = observer_on_changed),
        PROPERTY("elevation", "f", MEMBER(observer_t, hm),
                 .on_changed = observer_on_changed),
        PROPERTY("refraction", "b", MEMBER(observer_t, refraction),
                 .on_changed = observer_on_changed),
        PROPERTY("ut1", "f", MEMBER(observer_t, ut1),
                 .hint = "mjd", .on_changed = observer_on_timeattr_changed),
        PROPERTY("utc", "f", MEMBER(observer_t, utc),
                 .hint = "mjd", .on_changed = observer_on_timeattr_changed),
        PROPERTY("city", "p", MEMBER(observer_t, city),
                 .hint = "obj", .choices = city_get_choices,
                 .on_changed = observer_on_city_changed),
        PROPERTY("altitude", "f", MEMBER(observer_t, altitude),
                .hint = "d_angle"),
        PROPERTY("azimuth", "f", MEMBER(observer_t, azimuth),
                .hint = "h_angle"),
        PROPERTY("roll", "f", MEMBER(observer_t, roll)),
        PROPERTY("azalt", "v3", .hint = "azalt", .fn = observer_get_azalt),
        {}
    },
};
OBJ_REGISTER(observer_klass)

