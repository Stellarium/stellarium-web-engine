/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include <inttypes.h>

// Global list of all the registered klasses.
static obj_klass_t *g_klasses = NULL;

static obj_t *obj_create_(obj_klass_t *klass, json_value *args)
{
    const char *attr;
    int i;
    obj_t *obj;

    assert(klass->size);
    obj = calloc(1, klass->size);
    obj->ref = 1;
    obj->klass = klass;

    if (obj->klass->init) {
        if (obj->klass->init(obj, args) != 0) {
            free(obj);
            return NULL;
        }
    }

    // Set the attributes.
    if (args && args->type == json_object) {
        for (i = 0; i < args->u.object.length; i++) {
            attr = args->u.object.values[i].name;
            if (obj_has_attr(obj, attr)) {
                obj_call_json(obj, attr, args->u.object.values[i].value);
            }
        }
    }

    return obj;
}

obj_t *obj_create(const char *type, json_value *args)
{
    obj_t *obj;
    obj_klass_t *klass;

    for (klass = g_klasses; klass; klass = klass->next) {
        if (klass->id && strcmp(klass->id, type) == 0) break;
        if (klass->model && strcmp(klass->model, type) == 0) break;
    }
    assert(klass);
    obj = obj_create_(klass, args);
    return obj;
}

// Same as obj_create but the json arguments are passes the json arguments
// as a string.
EMSCRIPTEN_KEEPALIVE
obj_t *obj_create_str(const char *type, const char *args)
{
    obj_t *ret;
    json_value *jargs;
    json_settings settings = {.value_extra = json_builder_extra};
    jargs = args ? json_parse_ex(&settings, args, strlen(args), NULL) : NULL;
    ret = obj_create(type, jargs);
    json_value_free(jargs);
    return ret;
}

EMSCRIPTEN_KEEPALIVE
void obj_release(obj_t *obj)
{
    if (!obj) return;
    assert(obj->ref);
    obj->ref--;
    if (obj->ref == 0) {
        if (obj->parent) {
            LOG_E("Trying to delete an object still owned by a parent!");
            LOG_E("id: %s, klass: %s", obj->id, obj->klass->id);
        }
        assert(!obj->parent);
        if (obj->klass->del) obj->klass->del(obj);
        free(obj);
    }
}

EMSCRIPTEN_KEEPALIVE
obj_t *obj_retain(obj_t *obj)
{
    if (!obj) return NULL;
    assert(obj->ref);
    obj->ref++;
    return obj;
}

EMSCRIPTEN_KEEPALIVE
obj_t *obj_clone(const obj_t *obj)
{
    assert(obj->klass->clone);
    return obj->klass->clone(obj);
}

static void name_on_designation(const obj_t *obj, void *user, const char *dsgn)
{
    int current_score = 1;
    int *score = USER_GET(user, 0);
    char *out = USER_GET(user, 1);
    int len = *(int*)USER_GET(user, 2);
    if (strncmp(dsgn, "NAME ", 5) == 0) {
        dsgn += 5;
        current_score = 2;
    }
    if (current_score <= *score) return;
    *score = current_score;
    snprintf(out, len, "%s", dsgn);
}

/*
 * Function: obj_get_name
 * Return the given name for an object or its first designation.
 */
const char *obj_get_name(const obj_t *obj, char *buf, int len)
{
    int score = 0;
    assert(len > 0);
    buf[0] = '\0';
    obj_get_designations(obj, USER_PASS(&score, buf, &len),
                         name_on_designation);
    return buf;
}

int obj_render(const obj_t *obj, const painter_t *painter)
{
    if (obj->klass->render)
        return obj->klass->render(obj, painter);
    else
        return 0;
}

/*
 * Function: obj_get_pvo
 * Return the position and speed of an object.
 *
 * Parameters:
 *   obj    - A sky object.
 *   obs    - An observer.
 *   pvo    - Output ICRF position with origin on the observer.
 *
 * Return:
 *   0 for success, otherwise an error code, and in that case the position
 *   is undefined.
 */
int obj_get_pvo(obj_t *obj, observer_t *obs, double pvo[2][4])
{
    char name[64];
    int r;
    assert(obj && obj->klass && obj->klass->get_info);
    assert(observer_is_uptodate(obs, true));
    r = obj->klass->get_info(obj, obs, INFO_PVO, pvo);
    // Extra check for NAN values.
    if (DEBUG && r == 0 && isnan(pvo[0][0] + pvo[0][1] + pvo[0][2])) {
        obj_get_name(obj, name, sizeof(name));
        LOG_E("NAN value in obj position (%s)", name);
        assert(false);
    }
    return r;
}

/*
 * Function: obj_get_pos
 * Conveniance function to compute the position of an object in a given frame.
 *
 * This just calls obj_get_pvo followed by convert_frame.
 *
 * Parameters:
 *   obj    - A sky object.
 *   obs    - An observer.
 *   frame  - One of the <FRAME> enum values.
 *   pos    - Output position in the given frame, using homogenous coordinates.
 */
int obj_get_pos(obj_t *obj, observer_t *obs, int frame, double pos[4])
{
    int r;
    double pvo[2][4];
    assert(obj);
    r = obj_get_pvo(obj, obs, pvo);
    if (r) {
        memset(pos, 0, 4 * sizeof(double));
        return r;
    }
    convert_framev4(obs, FRAME_ICRF, frame, pvo[0], pos);
    return 0;
}

int obj_get_info(obj_t *obj, observer_t *obs, int info,
                 void *out)
{
    double pvo[2][4], pos[3], ra, dec;
    int ret;

    assert(obj);
    observer_update(obs, true);

    if (obj->klass->get_info) {
        ret = obj->klass->get_info(obj, obs, info, out);
        if (!ret) return ret;
        if (ret != 1) return ret; // An actual error
    }

    // Some fallback values.
    switch (info) {
    case INFO_RADEC: // First component of the PVO info.
        obj_get_info(obj, obs, INFO_PVO, pvo);
        memcpy(out, pvo[0], sizeof(pvo[0]));
        return 0;
    case INFO_LHA:
        obj_get_info(obj, obs, INFO_PVO, pvo);
        convert_frame(obs, FRAME_ICRF, FRAME_CIRS, 0, pvo[0], pos);
        eraC2s(pos, &ra, &dec);
        *(double*)out = eraAnpm(obs->astrom.eral - ra);
        return 0;
    case INFO_DISTANCE:
        obj_get_pvo(obj, obs, pvo);
        *(double*)out = pvo[0][3] ? vec3_norm(pvo[0]) : NAN;
        return 0;
    case INFO_SEARCH_VMAG:
        return obj_get_info(obj, obs, INFO_VMAG, out);
    default:
        break;
    }

    return 1;
}

EMSCRIPTEN_KEEPALIVE
char *obj_get_info_json(const obj_t *obj, observer_t *obs,
                        const char *info_str)
{
    int info = obj_info_from_str(info_str);
    int type = info % 16;
    int r = 0;
    union {
        bool b;
        int d;
        char otype[4];
        double f;
        double v2[2];
        double v3[3];
        double v4[4];
        double v4x2[2][4];
        const char *s_ptr;
    } v;
    char *ret = NULL;
    char *json = NULL;

    r = obj_get_info(obj, obs, info, &v.f);
    if (r) return NULL;

    switch (type) {
    case TYPE_FLOAT:
        if (isnan(v.f))
            r = asprintf(&ret, "null");
        else
            r = asprintf(&ret, "%.12f", v.f);
        break;
    case TYPE_INT:
        r = asprintf(&ret, "%d", v.d);
        break;
    case TYPE_BOOL:
        r = asprintf(&ret, "%s", v.b ? "true" : "fasle");
        break;
    case TYPE_OTYPE:
        r = asprintf(&ret, "\"%.4s\"", v.otype);
        break;
    case TYPE_STRING:
        if (v.s_ptr == NULL)
            r = asprintf(&ret, "null");
        else
            r = asprintf(&ret, "\"%s\"", v.s_ptr);
        break;
    case TYPE_V2:
        r = asprintf(&ret, "[%.12f, %.12f]", VEC2_SPLIT(v.v2));
        break;
    case TYPE_V3:
        r = asprintf(&ret, "[%.12f, %.12f, %.12f]", VEC3_SPLIT(v.v3));
        break;
    case TYPE_V4:
        r = asprintf(&ret, "[%.12f, %.12f, %.12f, %.12f]", VEC4_SPLIT(v.v4));
        break;
    case TYPE_V4X2:
        r = asprintf(&ret, "[[%.12f, %.12f, %.12f, %.12f],"
                            "[%.12f, %.12f, %.12f, %.12f]]",
                     VEC4_SPLIT(v.v4x2[0]), VEC4_SPLIT(v.v4x2[1]));
        break;
    default:
        assert(false);
        return NULL;
    }
    if (r < 0) LOG_E("Cannot generate json");

    /* Turn the result into an json object with a 'v' attribute, so that this
     * is a proper json document.
     * Also add the 'swe_' attribute as a convention.  */
    r = asprintf(&json, "{\"swe_\":1, \"v\":%s}", ret);
    (void)r;
    free(ret);
    return json;
}

EMSCRIPTEN_KEEPALIVE
const char *obj_get_id(const obj_t *obj)
{
    return obj->id;
}

static int on_name(const obj_t *obj, void *user,
                   const char *cat, const char *value)
{
    void (*f)(const obj_t *obj, void *user, const char *value);
    void *u;
    int *nb;
    char buf[1024];
    f = USER_GET(user, 0);
    u = USER_GET(user, 1);
    nb = USER_GET(user, 2);
    if (cat && *cat) {
        snprintf(buf, sizeof(buf), "%s %s", cat, value);
        f(obj, u, buf);
    } else {
        f(obj, u, value);
    }
    nb++;
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int obj_get_designations(const obj_t *obj, void *user,
                  void (*f)(const obj_t *obj, void *user, const char *dsgn))
{
    int nb = 0;
    if (obj->klass->get_designations)
        obj->klass->get_designations(obj, USER_PASS(f, user, &nb), on_name);
    return nb;
}


static void on_name2(const obj_t *obj, void *user, const char *dsgn) {
    json_value* arr = (json_value*)user;
    json_array_push(arr, json_string_new(dsgn));
}

json_value *obj_get_json_data(const obj_t *obj)
{
    json_value* ret;
    json_value* types;
    json_value* names;
    const char* ptype, *model;
    char tmp[5] = {};

    if (obj->klass->get_json_data)
        ret = obj->klass->get_json_data(obj);
    else
        ret = json_object_new(0);

    // Generic code to add object's model
    model = obj->klass->model ?: obj->klass->id;
    json_object_push(ret, "model", json_string_new(model));

    // Generic code to add object's types list
    types = json_array_new(1);
    strncpy(tmp, obj->type, 4);
    json_array_push(types, json_string_new(tmp));
    ptype = otype_get_parent(obj->type);
    while (ptype) {
        strncpy(tmp, ptype, 4);
        json_array_push(types, json_string_new(tmp));
        ptype = otype_get_parent(ptype);
    }
    json_object_push(ret, "types", types);

    // Generic code to add object's names list
    names = json_array_new(1);
    obj_get_designations(obj, (void*)names, on_name2);
    json_object_push(ret, "names", names);
    return ret;
}

EMSCRIPTEN_KEEPALIVE
char *obj_get_json_data_str(const obj_t *obj)
{
    json_value *data;
    char *ret;
    int size;

    data = obj_get_json_data(obj);
    if (!data) return NULL;
    size = json_measure(data);
    ret = calloc(1, size);
    json_serialize(ret, data);
    json_builder_free(data);
    return ret;
}

// XXX: cleanup this code.
static json_value *obj_fn_default(obj_t *obj, const attribute_t *attr,
                                  const json_value *args)
{
    assert(attr->type);
    void *p = ((void*)obj) + attr->member.offset;
    // Buffer large enough to contain any kind of property data, including
    // static strings.
    char buf[4096] __attribute__((aligned(8)));
    obj_t *o;

    // If no input arguents, return the value.
    if (!args || (args->type == json_array && !args->u.array.length)) {
        if (attr->type % 16 == TYPE_BOOL)
            return args_value_new(attr->type, *(bool*)p);
        else if (attr->type % 16 == TYPE_INT)
            return args_value_new(attr->type, *(int*)p);
        else if (attr->type % 16 == TYPE_FLOAT) {
            // Works for both float and double.
            if (attr->member.size == sizeof(double)) {
                return args_value_new(attr->type, *(double*)p);
            } else if (attr->member.size == sizeof(float)) {
                return args_value_new(attr->type, *(float*)p);
            } else {
                assert(false);
                return NULL;
            }
        } else if (attr->type == TYPE_JSON) {
            return json_copy(*(json_value**)p);
        } else if (attr->type % 16 == TYPE_PTR)
            return args_value_new(attr->type, *(void**)p);
        else if (attr->type == TYPE_STRING_PTR)
            return args_value_new(attr->type, *(char**)p);
        else
            return args_value_new(attr->type, p);
    } else { // Set the value.
        assert(attr->member.size <= sizeof(buf));
        args_get(args, attr->type, buf);
        if (memcmp(p, buf, attr->member.size) != 0) {
            // If we override an object, don't forget to release the
            // previous value and increment the ref to the new one.
            if (attr->type == TYPE_OBJ) {
                o = *(obj_t**)p;
                obj_release(o);
                memcpy(&o, buf, sizeof(o));
                if (o) o->ref++;
            }
            memcpy(p, buf, attr->member.size);
            if (attr->on_changed) attr->on_changed(obj, attr);
            module_changed(obj, attr->name);
        }
        return NULL;
    }
}

EMSCRIPTEN_KEEPALIVE
const attribute_t *obj_get_attr_(const obj_t *obj, const char *attr_name)
{
    attribute_t *attr;
    int i;
    assert(obj);
    if (!obj->klass->attributes) return NULL;
    for (i = 0; ; i++) {
        attr = &obj->klass->attributes[i];
        if (!attr->name) return NULL;
        if (strcmp(attr->name, attr_name) == 0) break;
    }
    return attr;
}

EMSCRIPTEN_KEEPALIVE
void obj_foreach_attr(const obj_t *obj,
                      void *user,
                      void (*f)(const char *attr, int is_property, void *user))
{
    int i;
    attribute_t *attr;
    obj_klass_t *klass;

    klass = obj->klass;
    if (!klass) return;
    if (!klass->attributes) return;
    for (i = 0; ; i++) {
        attr = &klass->attributes[i];
        if (!attr->name) break;
        f(attr->name, attr->is_prop, user);
    }
}

// Only to be used in the js code so that we can access object children
// directly using properties.
EMSCRIPTEN_KEEPALIVE
void obj_foreach_child(const obj_t *obj, void (*f)(const char *id))
{
    obj_t *child;
    for (child = obj->children; child; child = child->next) {
        if (!(child->klass->flags & OBJ_IN_JSON_TREE)) continue;
        if (!child->id) continue;
        f(child->id);
    }
}

bool obj_has_attr(const obj_t *obj, const char *attr)
{
    return obj_get_attr_(obj, attr) != NULL;
}

json_value *obj_call_json(obj_t *obj, const char *name,
                          const json_value *args)
{

    const attribute_t *attr;
    attr = obj_get_attr_(obj, name);
    if (!attr) {
        LOG_E("Cannot find attribute %s of object %s", name, obj->id);
        return NULL;
    }
    return (attr->fn ?: obj_fn_default)(obj, attr, args);
}

EMSCRIPTEN_KEEPALIVE
char *obj_call_json_str(obj_t *obj, const char *attr, const char *args)
{
    json_value *jargs, *jret;
    char *ret;
    int size;

    jargs = args ? json_parse(args, strlen(args)) : NULL;
    jret = obj_call_json(obj, attr, jargs);
    if (!jret) return NULL;
    size = json_measure(jret);
    ret = calloc(1, size);
    json_serialize(ret, jret);
    json_value_free(jargs);
    json_builder_free(jret);
    return ret;
}

int obj_get_attr(const obj_t *obj, const char *name, ...)
{
    json_value *ret;
    const attribute_t *attr;
    va_list ap;

    attr = obj_get_attr_(obj, name);
    va_start(ap, name);
    ret = obj_call_json(obj, name, NULL);
    assert(ret);
    args_vget(ret, attr->type, &ap);
    json_builder_free(ret);
    va_end(ap);
    return 0;
}

int obj_get_attr2(const obj_t *obj, const char *name, int type, ...)
{
    json_value *ret;
    const attribute_t *attr;
    va_list ap;

    assert(obj);
    attr = obj_get_attr_(obj, name);
    if (attr->type != type) return -1;
    va_start(ap, type);
    ret = obj_call_json(obj, name, NULL);
    assert(ret);
    args_vget(ret, attr->type, &ap);
    json_builder_free(ret);
    va_end(ap);
    return 0;
}

int obj_set_attr(const obj_t *obj, const char *name, ...)
{
    json_value *arg, *ret;
    va_list ap;
    const attribute_t *attr;

    attr = obj_get_attr_(obj, name);
    if (!attr) {
        LOG_E("Unknow attribute %s", name);
        assert(false);
    }
    va_start(ap, name);
    arg = args_vvalue_new(attr->type, &ap);
    ret = obj_call_json(obj, name, arg);
    json_builder_free(arg);
    json_builder_free(ret);
    va_end(ap);
    return 0;
}

void obj_register_(obj_klass_t *klass)
{
    assert(klass->size);
    LL_PREPEND(g_klasses, klass);
}

static int klass_sort_cmp(void *a, void *b)
{
    obj_klass_t *at, *bt;
    at = a;
    bt = b;
    int ak = at->create_order ?: at->render_order;
    int bk = bt->create_order ?: bt->render_order;
    return cmp(ak, bk);
}

obj_klass_t *obj_get_all_klasses(void)
{
    LL_SORT(g_klasses, klass_sort_cmp);
    return g_klasses;
}

obj_klass_t *obj_get_klass_by_name(const char *name)
{
    obj_klass_t *klass;
    for (klass = g_klasses; klass; klass = klass->next) {
        if (klass->id && strcmp(klass->id, name) == 0) return klass;
    }
    return NULL;
}

void obj_get_2d_ellipse(obj_t *obj,  const observer_t *obs,
                        const projection_t *proj,
                        double win_pos[2], double win_size[2],
                        double* win_angle)
{
    double pvo[2][4], p[4];
    double vmag, s = 0, luminance, radius = 0;

    if (obj->klass->get_2d_ellipse) {
        obj->klass->get_2d_ellipse(obj, obs, proj,
                                   win_pos, win_size, win_angle);
        return;
    }

    // Fall back to generic version

    // Ellipse center
    obj_get_pvo(obj, obs, pvo);
    vec3_normalize(pvo[0], pvo[0]);
    convert_frame(obs, FRAME_ICRF, FRAME_VIEW, true, pvo[0], p);
    project_to_win(proj, p, p);
    vec2_copy(p, win_pos);

    // Empirical formula to compute the pointer size.
    if (obj_get_info(obj, obs, INFO_VMAG, &vmag) == 0) {
        core_get_point_for_mag(vmag, &s, &luminance);
        s *= 2;
    }

    if (obj_get_info(obj, obs, INFO_RADIUS, &radius) == 0) {
        radius = core_get_point_for_apparent_angle(proj, radius);
        s = max(s, radius);
    }

    win_size[0] = s;
    win_size[1] = s;
    *win_angle = 0;
}

const char *obj_info_str(int info)
{
#define X(name, ...) if (INFO_##name == info) return #name;
    ALL_INFO(X)
#undef X
    return NULL;
}

const char *obj_info_type_str(int type)
{
    const char *names[] = {
#define X(name, lower, ...) [TYPE_##name] = #lower,
        ALL_TYPES(X)
#undef X
    };
    return names[type];
}

int obj_info_from_str(const char *str)
{
#define X(name, ...) if (strcasecmp(str, #name) == 0) return INFO_##name;
    ALL_INFO(X)
#undef X
    LOG_E("No such info name: %s", str);
    return -1;
}

static json_value *json_extract_attr(json_value *val, const char *attr)
{
    int i;
    json_value *ret = NULL;
    for (i = 0; i < val->u.object.length; i++) {
        if (strcmp(attr, val->u.object.values[i].name) != 0) continue;
        ret = val->u.object.values[i].value;
        val->u.object.values[i].value = NULL;
        break;
    }
    return ret;
}

void obj_set_attrs_json(const obj_t *obj, const char *json, char **restore)
{
    json_value *doc, *value, *restore_doc = NULL, *tmp;
    const char *key, *id;
    char buf[128];
    obj_t *o, *child;
    int i, len;
    json_serialize_opts opts = {
        .mode = json_serialize_mode_multiline,
        .indent_size = 4,
    };

    doc = json_parse(json, strlen(json));
    assert(doc && doc->type == json_object);
    if (restore) restore_doc = json_object_new(0);

    for (i = 0; i < doc->u.object.length; i++) {
        o = obj;
        key = doc->u.object.values[i].name;
        while (strchr(key, '.')) {
            id = key;
            len = strchr(id, '.') - id;
            key = id + len + 1;
            snprintf(buf, sizeof(buf), "%.*s", len, id);
            child = module_get_child(o, buf);
            if (!child)
                obj_get_attr2(o, buf, TYPE_OBJ, &child);
            if (!child) LOG_W("Cannot find key %s", buf);
            assert(child);
            o = child;
        }
        if (restore_doc) {
            tmp = obj_call_json(o, key, NULL);
            value = json_extract_attr(tmp, "v");
            json_builder_free(tmp);
            json_object_push(restore_doc, doc->u.object.values[i].name, value);
        }
        value = doc->u.object.values[i].value;
        obj_call_json(o, key, value);
    }
    json_value_free(doc);

    if (restore) {
        len = json_measure_ex(restore_doc, opts);
        *restore = calloc(1, len);
        json_serialize_ex(*restore, restore_doc, opts);
        json_builder_free(restore_doc);
    }
}

/******** TESTS ***********************************************************/

#if COMPILE_TESTS

typedef struct test {
    obj_t   obj;
    double  alt;
    int     proj;
    double  my_attr;
    int     nb_changes;
} test_t;

static void test_my_attr_changed(obj_t *obj, const attribute_t *attr)
{
    ((test_t*)obj)->nb_changes++;
}

static json_value *test_lookat_fn(obj_t *obj, const attribute_t *attr,
                                  const json_value *args)
{
    return NULL;
}

static obj_klass_t test_klass = {
    .id = "test",
    .attributes = (attribute_t[]) {
        PROPERTY(altitude, TYPE_FLOAT, MEMBER(test_t, alt)),
        PROPERTY(my_attr, TYPE_FLOAT, MEMBER(test_t, my_attr),
                 .on_changed = test_my_attr_changed),
        PROPERTY(projection, TYPE_ENUM, MEMBER(test_t, proj),
                 .desc = "Projection"),
        FUNCTION(lookat, .fn = test_lookat_fn),
        {}
    },
};

static void test_simple(void)
{
    test_t test = {};
    double alt;

    test.obj.klass = &test_klass;
    test.alt = 10.0;
    obj_get_attr(&test.obj, "altitude", &alt);
    assert(alt == 10.0);
    obj_set_attr(&test.obj, "altitude", 5.0);
    assert(test.alt == 5.0);

    obj_set_attr(&test.obj, "my_attr", 20.0);
    assert(test.my_attr == 20.0);
    assert(test.nb_changes == 1);
    obj_set_attr(&test.obj, "my_attr", 20.0);
    assert(test.nb_changes == 1);
    obj_set_attr(&test.obj, "my_attr", 30.0);
    assert(test.nb_changes == 2);
}

TEST_REGISTER(NULL, test_simple, TEST_AUTO);

#endif
