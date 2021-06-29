/* Stellarium Web Engine - Copyright (c) 2019 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * Helper macro to define a type based on a base type.  All the base types
 * have id inferior to 16, and extended types have a type of the form:
 * 16 * n + <base type id>
 * That way we can easily get the base type by doing a modulo 16 on a type.
 */
#define TYPE(i, base) (i * 16 + TYPE_##base)

/*
 * X macro to define all the basic types.
 * NAME, lowercase, num
 */
#define ALL_TYPES(X) \
    /* Base types */    \
    X(FLOAT,      float,    1) \
    X(INT,        int,      2) \
    X(BOOL,       bool,     3) \
    X(STRING,     string,   4) \
    X(PTR,        ptr,      5) \
    X(V2,         v2,       6) \
    X(V3,         v3,       7) \
    X(V4,         v4,       8) \
    X(V4X2,       v4x2,     9) \
    X(OTYPE,      otype,    10) \
    /* Extended types */    \
    X(MAG,        mag,      TYPE(1, FLOAT)) \
    X(ANGLE,      angle,    TYPE(2, FLOAT)) \
    X(OBJ,        obj,      TYPE(3, PTR)) \
    X(JSON,       json,     TYPE(4, STRING)) \
    X(ENUM,       enum,     TYPE(5, INT)) \
    X(STRING_PTR, string,   TYPE(6, STRING)) \
    X(COLOR,      color,    TYPE(7, V4)) \
    X(DIST,       dist,     TYPE(8, FLOAT)) \
    X(MJD,        mjd,      TYPE(9, FLOAT)) \
    X(FUNC,       func,     TYPE(10,PTR)) \

/*
 * Enum of all the types.
 * TYPE_FLOAT, TYPE_INT, etc.
 */
enum {
#define X(name, lower, value, ...) TYPE_##name = value,
    ALL_TYPES(X)
#undef X
};

const char *obj_info_str(int info);
const char *obj_info_type_str(int type);
int obj_info_from_str(const char *str);

#undef TYPE


/*
 * List of all the sky object info, with associated type.
 */
#define ALL_INFO(X) \
    X(VMAG,         vmag,       MAG,    1) \
    X(SEARCH_VMAG,  search_vmag,MAG,    2) \
    X(DISTANCE,     distance,   DIST,   4) \
    X(RADEC,        radec,      V4,     5) \
    X(PHASE,        phase,      FLOAT,  9) \
    X(RADIUS,       radius,     ANGLE,  10) \
    X(PVO,          pvo,        V4X2,   11) \
    X(LHA,          lha,        ANGLE,  12) \
    X(NEXT_PEAK,    next_peak,  MJD,    13) \
    X(POLE,         pole,       V3,     15) \

/*
 * Enum of all the info.
 * INFO_VMAG, INFO_RADIUS, etc.
 */
enum {
#define X(name, lower, type, i) INFO_##name = 1024 * i + TYPE_##type,
    ALL_INFO(X)
#undef X
};
