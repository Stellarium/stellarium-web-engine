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
#include "labels.h"
#include "projection.h"
#include "skyculture.h"
#include "hips.h"
#include "observer.h"
#include "obj.h"
#include "module.h"
#include "otypes.h"
#include "bayer.h"
#include "telescope.h"
#include "tonemapper.h"


typedef struct core core_t;

extern core_t *core;    // Global core object.

/*************************************************************************
 * Section: Rendering
 */

// I could try to use uint8_t to save memory here.
typedef struct {
    int level;
    int x, y;
} qtree_node_t;

// Generic function to split a surface into sub surfaces using a Breadth-first
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
//      uv      Original UV coordinates passed to the function.
//      pos     model pos of the current node.
//      mat     3x3 transformation to go from the original uv to the current
//              one.
//      painter The painter passed as argument, or, in step 2, eventually
//              a painter whose projection has been shifted to handle
//              a discontinuous case.
//      frame   One of the <FRAME> enum.
//      user    The use data passed as argument.
int traverse_surface(
        qtree_node_t *nodes,
        int nb_nodes,
        const double uv[4][2],
        const projection_t *proj,
        const painter_t *painter,
        int frame,
        void *user,
        int (*f)(int step,
                 qtree_node_t *node,
                 const double uv[4][2],
                 const double pos[4][4],
                 const double mat[3][3],
                 const painter_t *painter,
                 void *user));

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

    // Two parameters to manually adjust the size of the stars.
    double          star_linear_scale;
    double          star_scale_screen_factor;
    double          star_relative_scale;

    // Set the hints magnitude offset.
    double          hints_mag_offset;
    double          contrast; // Apply contrast to the rendered stars.

    tonemapper_t    tonemapper;
    bool            fast_adaptation; // True if eye adpatation is fast
    double          lwmax; // Max visible luminance.
    double          lwmax_min; // Min value for lwmax.
    double          lwsky_average;  // Current average sky luminance
    double          max_point_radius; // Max radius in pixel.
    double          min_point_radius;
    double          skip_point_radius;

    telescope_t     telescope;
    bool            telescope_auto; // Auto adjust telescope.
    double          exposure_scale;

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
        double      duration; // Animation duration in sec.
        // Set to true if the move is toward newly locked object.
        bool        move_to_lock;
    } target;

    struct {
        double      t;        // Goes from 0 to 1 as we move.
        double      duration; // Animation duration in sec.
        double      src_fov;  // Initial fov.
        double      dst_fov;  // Destination fov.
    } fov_animation;

    // Zoom movement. -1 to zoom out, +1 to zoom in.
    double zoom;

    // Maintains a list of clickable/hoverable areas.
    areas_t         *areas;

    // Computed min radius before we start to dim out stars brightness.
    double r_min;

    // Can be used for debugging.  It's conveniant to have an exposed test
    // attribute.
    bool test;
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

void core_init(double win_w, double win_h, double pixel_scale);
void core_add_default_sources(void);

void core_release(void);

/*
 * Function: core_update
 * Update the core and all the modules
 *
 * Parameters:
 *   dt     - Time imcrement from last frame (sec).
 */
int core_update(double dt);

/*
 * Function: core_update_fov
 * Update the core fov animation.
 *
 * Should be called before core_update
 *
 * Parameters:
 *   dt     - Time imcrement from last frame (sec).
 */
void core_update_fov(double dt);

int core_render(double win_w, double win_h, double pixel_scale);
// x and y in screen coordinates.
void core_on_mouse(int id, int state, double x, double y);
void core_on_key(int key, int action);
void core_on_char(uint32_t c);
void core_on_zoom(double zoom, double x, double y);

/*
 * Function: core_get_proj
 * Get the core current view projection
 *
 * Parameters:
 *   proj   - Pointer to a projection_t instance that get initialized.
 */
void core_get_proj(projection_t *proj);

/*
 * Function: core_get_obj_at
 * Get the object at a given screen position.
 *
 * Parameters:
 *   x        - The screen x position.
 *   y        - The screen y position.
 *   max_dist - Maximum distance in pixel between the object and the given
 *              position.
 */
obj_t *core_get_obj_at(double x, double y, double max_dist);

/*
 * Function: core_get_module
 * Return a core module by name
 *
 * Parameter:
 *    id    - Id or dot separated path to a module.  All the modules have
 *            the path 'core.<something>', but to make it simpler, here
 *            we also accept to search without the initial 'core.'.
 *
 * Return:
 *    The module object, or NULL if none was found.
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

void core_report_luminance_in_fov(double lum, bool fast_adaptation);

/*
 * Function: core_get_point_for_mag
 * Compute a point radius and luminosity from a visual magnitude.
 *
 * Parameters:
 *   mag       - The visual magnitude.
 *   radius    - Output radius in window pixels.
 *   luminance - Output luminance from 0 to 1, gamma corrected.  Ignored if
 *               set to NULL.
 */
void core_get_point_for_mag(double mag, double *radius, double *luminance);

/*
 * Function: core_mag_to_illuminance
 * Compute the illuminance for a given magnitude.
 *
 * This function is independent from the object surface area.
 *
 * Parameters:
 *   mag       - The visual magnitude integrated over the object's surface.
 *
 * Return:
 *   Object illuminance in lux (= lum/m² = cd.sr/m²)
 */
double core_mag_to_illuminance(double vmag);

/*
 * Function: core_mag_to_surf_brightness
 * Compute the sufrace brightness from a mag and surface.
 *
 * Parameters:
 *   mag       - The object's visual magnitude.
 *   surf      - The object's angular surface in rad^2
 *
 * Return:
 *   Object surface brightness in mag/arcsec²
 */
double core_mag_to_surf_brightness(double mag, double surf);

/*
 * Function: core_illuminance_to_lum_apparent
 * Compute the apparent luminance from an object's luminance and surface.
 *
 * Parameters:
 *   illum     - The illuminance.
 *   surf      - The angular surface in rad^2
 *
 * Return:
 *   Object luminance in cd/m².
 */
double core_illuminance_to_lum_apparent(double illum, double surf);

/*
 * Function: core_surf_brightness_to_lum_apparent
 * Compute the apparent luminance from an objet's surface brightness.
 *
 * Parameters:
 *   surf_brightness - The object surface brightness in mag/arcsec²
 *
 * Return:
 *   Object apparent luminance in cd/m².
 */
double core_surf_brightness_to_lum_apparent(double surf_brightness);

/*
 * Function: core_mag_to_lum_apparent
 * Compute the apparent luminance from an objet's magnitude and surface.
 *
 * Parameters:
 *   mag       - The visual magnitude integrated over the object's surface.
 *   surf      - The angular surface in rad^2
 *
 * Return:
 *   Object luminance in cd/m².
 */
double core_mag_to_lum_apparent(double mag, double surf);

/*
 * Function: core_get_apparent_angle_for_point
 * Get angular radius of a round object from it's pixel radius on screen.
 *
 * For example this can be used after core_get_point_for_mag to estimate the
 * angular size a circle should have to exactly fit the object.
 *
 * Parameters:
 *   proj   - The projection used.
 *   r      - Radius on screen in window pixels.
 *
 * Return:
 *   Angular radius in radian.  This is the physical radius, not scaled by
 *   the fov.
 *
 * XXX: make this a function of the projection directly?
 */
double core_get_apparent_angle_for_point(const projection_t *proj, double r);


// Return a static string representation of an object type id.
__attribute__((deprecated)) // Use otype_get_str instead.
const char *type_to_str(const char type[4]);

// Create or get a city.
obj_t *city_create(const char *name, const char *country_code,
                   const char *timezone,
                   double latitude, double longitude,
                   double elevation,
                   double get_near);

/*
 * Function: skyculture_get_name
 * Get the name of a star in the current skyculture.
 *
 * Parameters:
 *   skycultures    - A skyculture module.
 *   oid            - Object id of a star.
 *   buf            - A text buffer that get filled with the name.
 *
 * Return:
 *   NULL if no name was found.  A pointer to the passed buffer otherwise.
 */
const char *skycultures_get_name(obj_t *skycultures, uint64_t oid,
                                 char buf[128]);
