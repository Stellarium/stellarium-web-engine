/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

enum {
    SKIPPED = 1 << 16,
};

struct label
{
    label_t *next, *prev;
    char    *text;
    double  pos[2];
    double  radius;     // Radius of the object.
    double  size;
    double  color[4];
    double  angle;
    int     flags;

    double  priority;
    double  box[4];
};

static label_t *g_labels = NULL;

static int labels_render(const obj_t *obj, const painter_t *painter);

typedef struct labels {
    obj_t obj;
} labels_t;

static obj_klass_t labels_klass = {
    .id = "labels",
    .size = sizeof(labels_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .render = labels_render,
    .render_order = 100,
};

void labels_reset(void)
{
    label_t *label, *tmp;
    DL_FOREACH_SAFE(g_labels, label, tmp) {
        DL_DELETE(g_labels, label);
        free(label->text);
        free(label);
    }
}

static void label_get_box(const painter_t *painter, const label_t *label,
                          int anchor, double box[4])
{
    double borders[2];
    double pos[2];
    int size[2];
    double border = 8;

    vec2_copy(label->pos, pos);
    paint_text_size(painter, label->text, label->size, size);
    borders[0] = tan(label->radius / 2) / painter->proj->scaling[0];
    borders[1] = tan(label->radius / 2) / painter->proj->scaling[1];

    if (anchor & ANCHOR_LEFT)
        pos[0] += (size[0] + border) / painter->fb_size[0] + borders[0];
    if (anchor & ANCHOR_RIGHT)
        pos[0] -= (size[0] + border) / painter->fb_size[0] + borders[0];
    if (anchor & ANCHOR_BOTTOM)
        pos[1] += (size[1] + border)  / painter->fb_size[1] + borders[1];
    if (anchor & ANCHOR_TOP)
        pos[1] -= (size[1] + border)  / painter->fb_size[1] + borders[1];

    box[0] = pos[0] - (double)size[0] / painter->fb_size[0];
    box[1] = pos[1] - (double)size[1] / painter->fb_size[1];
    box[2] = pos[0] + (double)size[0] / painter->fb_size[0];
    box[3] = pos[1] + (double)size[1] / painter->fb_size[1];
}

static bool label_get_boxes(const painter_t *painter, const label_t *label,
                            int i, double box[4])
{
    const int anchors_around[] = {ANCHOR_BOTTOM_LEFT, ANCHOR_BOTTOM_RIGHT,
                                  ANCHOR_TOP_LEFT, ANCHOR_TOP_RIGHT};
    const int anchors_over[] = {ANCHOR_CENTER, ANCHOR_TOP,
                                ANCHOR_BOTTOM, ANCHOR_LEFT, ANCHOR_RIGHT};
    if (label->flags & ANCHOR_FIXED) {
        if (i != 0) return false;
        label_get_box(painter, label, label->flags, box);
        return true;
    }
    if (label->flags & ANCHOR_AROUND) {
        if (i >= 4) return false;
        label_get_box(painter, label, anchors_around[i], box);
    }
    if (label->flags & ANCHOR_CENTER) {
        if (i >= 5) return false;
        label_get_box(painter, label, anchors_over[i], box);
    }
    return true;
}

static bool box_overlap(const double a[4], const double b[4])
{
    return a[2] >  b[0] &&
           a[0] <= b[2] &&
           a[3] >  b[1] &&
           a[1] <= b[3];
}

static bool test_label_overlaps(const label_t *label)
{
    label_t *other;
    if (label->flags & ANCHOR_FIXED) return false;
    DL_FOREACH(g_labels, other) {
        if (other == label) break;
        if (other->flags & SKIPPED) continue;
        if (box_overlap(other->box, label->box)) return true;
    }
    return false;
}

static int label_cmp(void *a, void *b)
{
    return cmp(((label_t*)b)->priority, ((label_t*)a)->priority);
}

static int labels_render(const obj_t *obj, const painter_t *painter)
{
    label_t *label;
    int i;
    double pos[2];
    DL_SORT(g_labels, label_cmp);
    DL_FOREACH(g_labels, label) {
        for (i = 0; ; i++) {
            if (!label_get_boxes(painter, label, i, label->box)) {
                label->flags |= SKIPPED;
                goto skip;
            }
            if (!test_label_overlaps(label)) break;
        }
        pos[0] = (label->box[0] + label->box[2]) / 2;
        pos[1] = (label->box[1] + label->box[3]) / 2;
        paint_text(painter, label->text, pos, label->size,
                   label->color, label->angle);
        label->flags &= ~SKIPPED;
skip:;
    }
    return 0;
}

label_t *labels_add(const char *text, const double pos[2], double radius,
                    double size, const double color[4], double angle,
                    int flags, double priority)
{
    if (flags & ANCHOR_FIXED) priority = 1024.0; // Use FLT_MAX ?
    assert(priority <= 1024.0);
    assert(color);
    label_t *label = calloc(1, sizeof(*label));
    *label = (label_t) {
        .text = strdup(text),
        .pos = {pos[0], pos[1]},
        .radius = radius,
        .size = size,
        .color = {color[0], color[1], color[2], color[3]},
        .angle = angle,
        .flags = flags,
        .priority = priority,
    };
    DL_APPEND(g_labels, label);
    return label;
}

OBJ_REGISTER(labels_klass)
