/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

// Extra flags (on top of anchors flags).
enum {
    SKIPPED = 1 << 16,
};

typedef struct label label_t;
struct label
{
    label_t *next, *prev;
    uint64_t oid;
    char    *text; // Original passed text.
    char    *render_text; // Processed text (can point to text).
    double  pos[2];
    double  radius;     // Radius of the object (pixel).
    double  size;
    double  color[4];
    double  angle;
    int     flags;
    fader_t fader;

    double  priority;
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
                          const double pos[2], uint64_t oid)
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
                             int align, double bounds[4])
{
    double border;
    double pos[2];
    vec2_copy(label->pos, pos);
    border = label->radius;
    if (label->flags & LABEL_AROUND) border /= sqrt(2.0);
    if (align & ALIGN_LEFT)    pos[0] += border;
    if (align & ALIGN_RIGHT)   pos[0] -= border;
    if (align & ALIGN_BOTTOM)  pos[1] -= border;
    if (align & ALIGN_TOP)     pos[1] += border;
    paint_text_bounds(painter, label->render_text, pos, align, label->size,
                      bounds);
}

static bool label_get_possible_bounds(const painter_t *painter,
        const label_t *label, int i, double bounds[4])
{
    const int anchors_around[] = {
        ALIGN_LEFT   | ALIGN_BOTTOM,
        ALIGN_LEFT   | ALIGN_TOP,
        ALIGN_RIGHT  | ALIGN_BOTTOM,
        ALIGN_RIGHT  | ALIGN_TOP};

    if (!(label->flags & LABEL_AROUND)) {
        if (i != 0) return false;
        label_get_bounds(painter, label, label->flags, bounds);
        return true;
    }
    if (label->flags & LABEL_AROUND) {
        if (i >= 4) return false;
        label_get_bounds(painter, label, anchors_around[i], bounds);
    }
    return true;
}

static bool bounds_overlap(const double a[4], const double b[4])
{
    return a[2] >  b[0] &&
           a[0] <= b[2] &&
           a[3] >  b[1] &&
           a[1] <= b[3];
}

static bool test_label_overlaps(const label_t *label)
{
    label_t *other;
    if (!(label->flags & LABEL_AROUND)) return false;
    DL_FOREACH(g_labels->labels, other) {
        if (other == label) break;
        if (other->flags & SKIPPED) continue;
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
        // We fade in the label slowly, but fade out very fast, otherwise
        // we don't get updated positions for fading out labels.
        fader_update(&label->fader, label->fader.target ? 0.016 : 0.16);
        painter.font = label->flags & LABEL_BOLD ? "bold" : NULL;
        for (i = 0; ; i++) {
            if (!label_get_possible_bounds(&painter, label, i, label->bounds)) {
                label->flags |= SKIPPED;
                goto skip;
            }
            if (!test_label_overlaps(label)) break;
        }
        pos[0] = label->bounds[0];
        pos[1] = label->bounds[1];
        vec4_copy(label->color, color);
        color[3] *= label->fader.value;
        paint_text(&painter, label->render_text, pos,
                   ALIGN_LEFT | ALIGN_TOP, label->size, color,
                   label->angle);
        label->flags &= ~SKIPPED;
skip:;
    }
    return 0;
}

/*
 * Function: labels_add
 * Render a label on screen.
 *
 * Parameters:
 *   text       - The text to render.
 *   win_pow    - Position of the text in windows coordinates.
 *   radius     - Radius of the point the label is linked to.  Zero for
 *                independent label.
 *   size       - Height of the text in pixel.
 *   color      - Color of the text.
 *   angle      - Rotation angle (rad).
 *   flags      - Union of <LABEL_FLAGS>.  Used to specify anchor position
 *                and text effects.
 *   oid        - Optional unique id for the label.
 */
void labels_add(const char *text, const double pos[2],
                double radius, double size, const double color[4],
                double angle, int flags, double priority, uint64_t oid)
{
    if (!(flags & LABEL_AROUND)) priority = 1024.0; // Use FLT_MAX ?
    assert(priority <= 1024.0);
    assert(color);
    label_t *label;

    if (!text || !*text) return;
    label = label_get(g_labels->labels, text, size, pos, oid);
    if (!label) {
        label = calloc(1, sizeof(*label));
        label->oid = oid;
        fader_init(&label->fader, false);
        label->render_text = label->text = strdup(text);
        if (flags & LABEL_UPPERCASE) {
            label->render_text = malloc(strlen(text) + 64);
            u8_upper(label->render_text, text, strlen(text) + 64);
        }
        DL_APPEND(g_labels->labels, label);
    }

    vec2_set(label->pos, pos[0], pos[1]);
    label->radius = radius;
    label->size = size;
    vec4_set(label->color, color[0], color[1], color[2], color[3]);
    label->angle = angle;
    label->flags = flags;
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
    .render_order = 100,
    .attributes = (attribute_t[]) {
        {},
    },
};

OBJ_REGISTER(labels_klass)
