/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

// File: core.h

#include "areas.h"
#include "calendar.h"
#include "events.h"
#include "frames.h"
#include "identifiers.h"
#include "projection.h"
#include "skyculture.h"
#include "hips.h"
#include "observer.h"
#include "obj.h"
#include "bayer.h"


typedef struct core core_t;
typedef struct label label_t;

extern core_t *core;    // Global core object.

/*************************************************************************
 * Section: Rendering
 */

// I could try to use uint8_t to save memory here.
typedef struct {
    int level;
    union {
        struct {
            int x, y;
        };
        int xy[2];
    };
    int c;
    int s[2];
} qtree_node_t;

// Generic function to split a surface into sub surfaces using a Depth-first
// search.
//
//  nodes       An array of node that is used internally by the algo.
//  nb_nodes    The number of nodes given.
//  uv          A 2d quad that defines the initial surface.  If NULL, then
//              a default u in [0 - 1], v in [0 - 1] quad is used.
//  proj        Optional projection that is used to map uv coordinates (2d)
//              to model coordinates (3d).
//  painter     Optional painter used to get the viewport projection.
//  user        User data passed to the visitor function.
//  f           The visitor function.  The function is called several times
//              at each node.  It can return the following values:
//                  0: stop visiting this node and deeper nodes.
//                  1: stop visiting this node and go to deeper nodes.
//                  2: keep visiting this node and go to deeper nodes.
//
//      step    For each node, the visitor is called up to 3 times:
//              step = 0: first call, before any check has been done.
//              step = 1: called after we checked that the node is visible,
//              step = 2: called after we checked for discontinuity.  At
//                        this step, the painter projection should always be
//                        able to render the quad.
//              For simple case, we can return 0 or 1 at step 0, skipping
//              all the other steps.
//      node    The current node visited.
//      uv      UV coordinates of the current node.
//      pos     model pos of the current node.
//      painter The painter passed as argument, or, in step 2, eventually
//              a painter whose projection has been shifted to handle
//              a discontinuous case.
//      frame   One of the <FRAME> enum.
//      mode    0: BFS, 1: DFS.
//      user    The use data passed as argument.
//      s       The out u and v split factor for the node children.
//              default to [2, 2].
int traverse_surface(
        qtree_node_t *nodes,
        int nb_nodes,
        const double uv[4][2],
        const projection_t *proj,
        const painter_t *painter,
        int frame,
        int mode,
        void *user,
        int (*f)(int step,
                 qtree_node_t *node,
                 const double uv[4][2],
                 const double pos[4][4],
                 const painter_t *painter,
                 void *user,
                 int s[2]));

/***** Labels manager *****************************************************/

enum {
    ANCHOR_LEFT     = 1 << 0,
    ANCHOR_RIGHT    = 1 << 1,
    ANCHOR_BOTTOM   = 1 << 2,
    ANCHOR_TOP      = 1 << 3,
    ANCHOR_VCENTER  = 1 << 4,
    ANCHOR_HCENTER  = 1 << 5,

    ANCHOR_FIXED    = 1 << 6,
    ANCHOR_AROUND   = 1 << 7,

    ANCHOR_CENTER       = ANCHOR_VCENTER | ANCHOR_HCENTER,
    ANCHOR_BOTTOM_LEFT  = ANCHOR_BOTTOM | ANCHOR_LEFT,
    ANCHOR_BOTTOM_RIGHT = ANCHOR_BOTTOM | ANCHOR_RIGHT,
    ANCHOR_TOP_LEFT     = ANCHOR_TOP | ANCHOR_LEFT,
    ANCHOR_TOP_RIGHT    = ANCHOR_TOP | ANCHOR_RIGHT,
};

void labels_reset(void);
label_t *labels_add(const char *text, const double pos[2], double radius,
                    double size, const double color[4], double angle,
                    int flags, double priority);

/******* Section: Core ****************************************************/

/* Type: core_t
 * Contains all the modules and global state of the program.
 */
struct core
{
    obj_t           obj;
    observer_t      *observer;
    double          fov;
    // Global timezone setting for rendering times, independent of the city.
    const char      timezone[64];
    // Global utc offset used when rendering the time (min)
    int             utc_offset;

    // Used to compute eyes adaptation.  It's not very good for the moment,
    // just enough to hide the stars during daytime.
    double          max_vmag_in_fov;
    double          vmag_shift;
    // For debugging: allow to manually set a vmag shift.
    double          manual_vmag_shift;

    // Set the hints max magnitude.
    double          hints_mag_max;
    double          contrast; // Apply contrast to the rendered stars.

    renderer_t      *rend;
    int             proj;
    double          win_size[2];
    double          win_pixels_scale;
    obj_t           *selection;
    obj_t           *hovered;
    bool            fast_mode; // Render as fast as possible.

    // Profiling data.
    struct {
        double      start_time; // Start of measurement window (sec)
        int         nb_frames;  // Number of frames elapsed.
        double      fps;        // Averaged FPS counter.
    } prof;

    gesture_t       gest_pan;
    gesture_t       gest_click;
    gesture_t       gest_hover;
    gesture_t       gest_pinch;

    // Number of clicks so far.  This is just so that we can wait for clicks
    // from the ui.
    int clicks;
    bool ignore_clicks; // Don't select on click.

    struct {
        struct {
            int    id; // Backend id (for example used in js).
            double pos[2];
            bool   down[2];
        } touches[2];
        bool        keys[512]; // Table of all key state.
        uint32_t    chars[16]; // Unicode characters.
    } inputs;
    bool            gui_want_capture_mouse;

    struct {
        obj_t       *lock;    // Optional obj we lock to.
        double      src_q[4]; // Initial pos quaternion.
        double      dst_q[4]; // Destination pos quaternion.
        double      t;        // Goes from 0 to 1 as we move.
        double      speed;
        double      src_fov;  // Initial fov.
        double      dst_fov;  // Destination fov (0 to ignore).
    } target;

    // Auto computed from fov and screen aspect ratio.
    double fovx;
    double fovy;

    // Maintains a list of clickable/hoverable areas.
    areas_t         *areas;
};

enum {
    KEY_ACTION_UP      = 0,
    KEY_ACTION_DOWN    = 1,
    KEY_ACTION_REPEAT  = 2,
};

// Key id, same as GLFW for convenience.
enum {
    KEY_ESCAPE      = 256,
    KEY_ENTER       = 257,
    KEY_TAB         = 258,
    KEY_BACKSPACE   = 259,
    KEY_DELETE      = 261,
    KEY_RIGHT       = 262,
    KEY_LEFT        = 263,
    KEY_DOWN        = 264,
    KEY_UP          = 265,
    KEY_PAGE_UP     = 266,
    KEY_PAGE_DOWN   = 267,
    KEY_HOME        = 268,
    KEY_END         = 269,
    KEY_SHIFT       = 340,
    KEY_CONTROL     = 341,
};

void core_init(void);
void core_release(void);
int core_render(int w, int h, double pixel_scale);
// x and y in screen coordinates.
void core_on_mouse(int id, int state, double x, double y);
void core_on_key(int key, int action);
void core_on_char(uint32_t c);
void core_on_zoom(double zoom, double x, double y);

/*
 * Function: core_get_module
 * Return a core module by name
 */
obj_t *core_get_module(const char *id);

/*
 * Function: core_report_vmag_in_fov
 * Inform the core that an object with a given vmag is visible.
 *
 * This is used for the eyes adaptations algo.
 *
 * Parameters:
 *   vmag - The magnitude of the object.
 *   r    - Visible radius of the object (rad).
 *   sep  - Separation of the center of the object to the center of the
 *          screen.
 */
void core_report_vmag_in_fov(double vmag, double r, double sep);

// Compute the observed magnitude of an object after adjustment for the
// optic and eye adaptation.
double core_get_observed_mag(double vmag);

// Compute the radius angle (in radiant) and luminance (0 to 1) for a given
// observed magnitude.
void core_get_point_for_mag(double mag, double *radius, double *luminance);

// Return a static string representation of an object type id.
const char *type_to_str(const char type[4]);

/***** Dump catalogs to json **********************************************/
int dump_catalog(const char *path);


/***** Symbol rendering **************************************************/
void symbols_init(void);
texture_t *symbols_get(const char *id, double rect[2][2]);
int symbols_paint(const painter_t *painter,
                  const char *id,
                  const double pos[2], double size, const double color[4],
                  double angle);

// Create or get a city.
obj_t *city_create(const char *name, const char *country_code,
                   const char *timezone,
                   double latitude, double longitude,
                   double elevation,
                   double get_near);


/**** Simbad object types database ******/
int otypes_lookup(const char *condensed,
                  const char **name,
                  const char **explanation,
                  int ns[4]);
// Return the parent condensed id of an otype.
const char *otypes_get_parent(const char *condensed);
