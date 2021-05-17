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
    obj_t   *obj;         // Optional object.
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
    bool    active;       // Set as long as the label is rendered.
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
            obj_release(label->obj);
            free(label);
        } else {
            label->active = false;
            label->fader.target = false;
        }
    }
}

static label_t *label_get(label_t *list, const char *txt, double size,
                          const obj_t *obj)
{
    label_t *label;
    DL_FOREACH(list, label) {
        if (obj != label->obj) continue;
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

    // Adjust the border if we are on a diagonal.
    if (    (align & (ALIGN_LEFT | ALIGN_RIGHT)) &&
            (align & (ALIGN_TOP | ALIGN_BOTTOM))) {
        border /= sqrt(2.0);
    }

    if (align & ALIGN_LEFT)    pos[0] += border;
    if (align & ALIGN_RIGHT)   pos[0] -= border;
    if (align & ALIGN_BOTTOM)  pos[1] -= border;
    if (align & ALIGN_TOP)     pos[1] += border;
    paint_text_bounds(painter, label->render_text, pos, align, effects,
                      label->size, bounds);
}

// Compute the intersection of two bounding box.
// If the boxes do not intersect, set out to 0 and return false.
static bool bounds_intersection(const double a[4], const double b[4],
                                double out[4])
{
    out[0] = max(a[0], b[0]);
    out[1] = max(a[1], b[1]);
    out[2] = min(a[2], b[2]);
    out[3] = min(a[3], b[3]);

    if (out[0] >= out[2] || out[1] >= out[3]) {
        memset(out, 0, 4 * sizeof(double));
        return false;
    }
    return true;
}

// Return the signed distance of a point to a bounding rect.
static double bounds_dist_point(const double rect[4], const double p[2])
{
    double x = (rect[0] + rect[2]) / 2;
    double y = (rect[1] + rect[3]) / 2;
    double w = rect[2] - rect[0];
    double h = rect[3] - rect[1];
    double dx = max(fabs(p[0] - x) - w / 2, 0);
    double dy = max(fabs(p[1] - y) - h / 2, 0);
    return sqrt(dx * dx + dy * dy);
}

static double test_label_overlaps(const label_t *label)
{
    label_t *other;
    double ret = 0, overlap;
    double inter[4];

    if (!(label->effects & TEXT_FLOAT)) return 0.0;
    DL_FOREACH(g_labels->labels, other) {
        if (other == label) break;
        if (other->fader.target == false) continue;
        if (!bounds_intersection(label->bounds, other->bounds, inter))
            continue;
        overlap = max(inter[2] - inter[0], inter[3] - inter[1]);
        if (overlap > ret)
            ret = overlap;
    }
    return ret;
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
    double win_pos[2], view_pos[3];
    const double max_overlap = 8;
    painter_t painter = *painter_;
    bool use_depth;

    DL_SORT(g_labels->labels, label_cmp);
    DL_FOREACH(g_labels->labels, label) {

        if (core->selection && core->hide_selection_label &&
                label->obj == core->selection)
            continue;

        vec4_copy(label->color, painter.color);
        painter.color[3] *= label->fader.value;
        // Re-project label on screen
        if (label->frame != -1) {
            painter_project(&painter, label->frame, label->pos, label->at_inf,
                            false, label->win_pos);
        }
        use_depth = false;
        painter.flags &= ~PAINTER_ENABLE_DEPTH;
        if (label->frame != -1 && !label->at_inf) {
            convert_frame(painter.obs, label->frame, FRAME_VIEW,
                          label->at_inf, label->pos, view_pos);
            use_depth = true;
            painter.flags |= PAINTER_ENABLE_DEPTH;
        }
        label_get_bounds(&painter, label, label->align, label->effects,
                         label->bounds);
        label->fader.target = label->active &&
                                (test_label_overlaps(label) <= max_overlap);
        win_pos[0] = label->bounds[0];
        win_pos[1] = label->bounds[1];
        paint_text(&painter, label->render_text, win_pos,
                   use_depth ? view_pos : NULL,
                   ALIGN_LEFT | ALIGN_TOP, label->effects, label->size,
                   label->angle);
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
                obj_t *obj)
{
    const double p[3] = {pos[0], pos[1], 0};
    labels_add_3d(text, -1, p, true, radius, size, color, angle, align,
                  effects, priority, obj);
}

void labels_add_3d(const char *text, int frame, const double pos[3],
                   bool at_inf, double radius, double size,
                   const double color[4], double angle, int align,
                   int effects, double priority, obj_t *obj)
{
    if (!align) align = ALIGN_CENTER | ALIGN_BOTTOM;
    if (!(effects & TEXT_FLOAT)) priority = 1024.0; // Use FLT_MAX ?
    assert(priority <= 1024.0);
    assert(color);
    assert(!angle); // Not supported at the moment.
    assert(!obj || (obj->klass && obj->klass->get_info));
    label_t *label;

    if (!text || !*text) return;

    label = label_get(g_labels->labels, text, size, obj);
    if (!label) {
        label = calloc(1, sizeof(*label));
        label->obj = obj_retain(obj);
        fader_init(&label->fader, false);
        label->render_text = label->text = strdup(text);
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
    label->active = true;
}

/*
 * Function: labels_has_obj
 * Check if an object is in the labels list
 */
bool labels_has_obj(const obj_t *obj)
{
    label_t *label;
    DL_FOREACH(g_labels->labels, label) {
        if (label->obj == obj)
            return true;
    }
    return false;
}

/*
 * Function: labels_get_at
 * Find a label at a given position on screen.
 */
obj_t *labels_get_obj_at(const double pos[2], double max_dist)
{
    label_t *label;
    double dist;

    DL_FOREACH(g_labels->labels, label) {
        if (!label->obj || !label->active || label->fader.value == 0)
            continue;
        dist = bounds_dist_point(label->bounds, pos);
        if (dist <= max_dist) {
            return obj_retain(label->obj);
        }
    }
    return 0;
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
