/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"


typedef struct label label_t;
struct label
{
    label_t *next, *prev;
    uint64_t oid;         // Optional unique id for the label.
    char    *text;        // Original passed text.
    char    *render_text; // Processed text (can point to text).
    double  pos[3];       // 3D position in the given frame.
    double  win_pos[2];   // 2D position on screen (px).
    int     frame;        // One of FRAME_XXX or -1 for 2D win position.
    bool    at_inf;       // True if pos is normalized.
    double  radius;       // Radius of the object (pixel).
    double  size;         // Height of the text in pixel.
    double  color[4];     // Color of the text.
    double  angle;        // Rotation angle on screen (rad).
    int     align;        // Union of <ALIGN_FLAGS>.
    int     effects;      // Union of <TEXT_EFFECT_FLAGS>.
    fader_t fader;        // Use for auto fade-in/out of labels.
    bool    skipped;      // Set when the label cannot be rendered.

    double  priority;     // Priority used in case of positioning conflicts.
                          // Higher value means higher priority.
    double  bounds[4];
};

typedef struct labels {
    obj_t obj;
    label_t *labels;
} labels_t;

static labels_t *g_labels = NULL;

void labels_reset(void)
{
    label_t *label, *tmp;
    DL_FOREACH_SAFE(g_labels->labels, label, tmp) {
        if (label->fader.target == false && label->fader.value == 0) {
            DL_DELETE(g_labels->labels, label);
            if (label->render_text != label->text) free(label->render_text);
            free(label->text);
            free(label);
        } else {
            label->fader.target = false;
        }
    }
}

static label_t *label_get(label_t *list, const char *txt, double size,
                          uint64_t oid)
{
    label_t *label;
    DL_FOREACH(list, label) {
        if (oid != label->oid) continue;
        if (label->size == size && strcmp(txt, label->text) == 0)
            return label;
    }
    return NULL;
}

static void label_get_bounds(const painter_t *painter, const label_t *label,
                             int align, int effects, double bounds[4])
{
    double border;
    double pos[2];
    vec2_copy(label->win_pos, pos);
    border = label->radius;
    if (label->align & LABEL_AROUND) border /= sqrt(2.0);
    if (align & ALIGN_LEFT)    pos[0] += border;
    if (align & ALIGN_RIGHT)   pos[0] -= border;
    if (align & ALIGN_BOTTOM)  pos[1] -= border;
    if (align & ALIGN_TOP)     pos[1] += border;
    paint_text_bounds(painter, label->render_text, pos, align, effects,
                      label->size, bounds);
}

static bool label_get_possible_bounds(const painter_t *painter,
        const label_t *label, int i, double bounds[4])
{
    const int anchors_around[] = {
        ALIGN_LEFT   | ALIGN_BOTTOM,
        ALIGN_LEFT   | ALIGN_TOP,
        ALIGN_RIGHT  | ALIGN_BOTTOM,
        ALIGN_RIGHT  | ALIGN_TOP};

    if (!(label->align & LABEL_AROUND)) {
        if (i != 0) return false;
        label_get_bounds(painter, label, label->align, label->effects, bounds);
        return true;
    }
    if (label->align & LABEL_AROUND) {
        if (i >= 4) return false;
        label_get_bounds(painter, label, anchors_around[i], label->effects,
                         bounds);
    }
    return true;
}

static bool bounds_overlap(const double a[4], const double b[4])
{
    static const double margin = -5;
    return a[2] > b[0] - margin &&
           a[0] < b[2] + margin &&
           a[3] > b[1] - margin &&
           a[1] < b[3] + margin;
}

static bool test_label_overlaps(const label_t *label)
{
    label_t *other;
    if (!(label->align & LABEL_AROUND)) return false;
    DL_FOREACH(g_labels->labels, other) {
        if (other == label) break;
        if (other->skipped) continue;
        if (other->fader.target == false) continue;
        if (bounds_overlap(other->bounds, label->bounds)) return true;
    }
    return false;
}

static int label_cmp(void *a, void *b)
{
    return cmp(((label_t*)b)->priority, ((label_t*)a)->priority);
}

static int labels_init(obj_t *obj, json_value *args)
{
    g_labels = (void*)obj;
    return 0;
}

static int labels_render(const obj_t *obj, const painter_t *painter_)
{
    label_t *label;
    int i;
    double pos[2], color[4];
    painter_t painter = *painter_;
    DL_SORT(g_labels->labels, label_cmp);
    DL_FOREACH(g_labels->labels, label) {
        // Re-project label on screen
        if (label->frame != -1) {
            painter_project(painter_, label->frame, label->pos, label->at_inf,
                            false, label->win_pos);
        }

        for (i = 0; ; i++) {
            if (!label_get_possible_bounds(&painter, label, i, label->bounds)) {
                label->skipped = true;
                goto skip;
            }

            // Don't try to fit label currently fading out
            if (label->fader.target == false) break;

            if (!test_label_overlaps(label)) break;

            // The label is colliding with another one, try to fit up and
            // down around position before giving up
            label->bounds[1] -= 2;
            label->bounds[3] -= 2;
            if (!test_label_overlaps(label)) break;
            label->bounds[1] += 4;
            label->bounds[3] += 4;
            if (!test_label_overlaps(label)) break;
            label->bounds[1] -= 6;
            label->bounds[3] -= 6;
            if (!test_label_overlaps(label)) break;
            label->bounds[1] -= 2;
            label->bounds[3] -= 2;
            if (!test_label_overlaps(label)) break;
        }
        pos[0] = label->bounds[0];
        pos[1] = label->bounds[1];
        vec4_copy(label->color, color);
        color[3] *= label->fader.value;
        paint_text(&painter, label->render_text, pos,
                   ALIGN_LEFT | ALIGN_TOP, label->effects, label->size, color,
                   label->angle);
        label->skipped = false;
skip:;
    }
    return 0;
}

static int labels_update(obj_t *obj, double dt)
{
    label_t *label = (label_t *)obj;
    DL_FOREACH(g_labels->labels, label) {
        fader_update(&label->fader, dt);
    }
    return 0;
}


void labels_add(const char *text, const double pos[2],
                double radius, double size, const double color[4],
                double angle, int align, int effects, double priority,
                uint64_t oid)
{
    const double p[3] = {pos[0], pos[1], 0};
    labels_add_3d(text, -1, p, true, radius, size, color, angle, align,
                  effects, priority, oid);
}

void labels_add_3d(const char *text, int frame, const double pos[3],
                   bool at_inf, double radius, double size,
                   const double color[4], double angle, int align,
                   int effects, double priority, uint64_t oid)
{
    if (!(align & LABEL_AROUND)) priority = 1024.0; // Use FLT_MAX ?
    assert(priority <= 1024.0);
    assert(color);
    assert(!angle); // Not supported at the moment.
    label_t *label;

    if (!text || !*text) return;

    label = label_get(g_labels->labels, text, size, oid);
    if (!label) {
        label = calloc(1, sizeof(*label));
        label->oid = oid;
        fader_init(&label->fader, false);
        label->render_text = label->text = strdup(text);
        // Note: should be done by the painter directly.
        if (effects & TEXT_UPPERCASE) {
            label->render_text = malloc(strlen(text) + 64);
            u8_upper(label->render_text, text, strlen(text) + 64);
        }
        DL_APPEND(g_labels->labels, label);
    }

    if (frame == -1)
        vec2_copy(pos, label->win_pos);
    else {
        vec3_copy(pos, label->pos);
    }
    label->frame = frame;
    label->at_inf = at_inf;
    label->radius = radius;
    label->size = size;
    vec4_set(label->color, color[0], color[1], color[2], color[3]);
    label->angle = angle;
    label->align = align;
    label->effects = effects;
    label->priority = priority;
    label->fader.target = true;
}

/*
 * Meta class declarations.
 */

static obj_klass_t labels_klass = {
    .id = "labels",
    .size = sizeof(labels_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init = labels_init,
    .render = labels_render,
    .update = labels_update,
    .render_order = 100,
    .attributes = (attribute_t[]) {
        {},
    },
};

OBJ_REGISTER(labels_klass)
