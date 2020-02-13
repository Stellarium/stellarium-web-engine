/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

// General struct that can represents any type of item in a GUI menu.
typedef struct gui_item gui_item_t;
struct gui_item
{
    const char  *id;       // If not set, we use the label.
    const char  *label;
    const char  *sub_label;
    bool        small;

    // If set the item will allow to edit the attribute of the object.
    obj_t       *obj;
    const char  *attr;

    // Direct value pointers.
    // If any of those is set, the item will edit the value.
    struct      {
        double  *f;
        int     *d;
    } value;
    // XXX this should be retrieve from the object attr meta data.
    double      default_value;

    // If set the item will be a navigation button to the menu.
    void        (*menu)(void *user);
    void        *user;  // Can be defined by the user.

    // Internal.
    gui_item_t  *next, *prev, *children;    // Items are stored in a tree.
};

void gui_same_line(void);

void gui_init(void *user);
void gui_release(void);
bool gui_item(const gui_item_t *item);
void gui_text(const char *label, ...);

// All the basic gui widgets, need to be implemented for both imgui
// and html backend.
void gui_text_unformatted(const char *text);
void gui_label(const char *label, const char *value);
bool gui_toggle(const char *label, bool *v);
bool gui_button(const char *label, double size);
bool gui_link(const char *label, const char *sublabel);
bool gui_int(const char *label, int *v);
// default_value set to NAN for no toggle button.
bool gui_double(const char *label, double *v, double min_v, double max_v,
                int precision, double default_value);
bool gui_float(const char *label, float *v, float min_v, float max_v,
               int precision, float default_value);
bool gui_double_log(const char *label, double *v, double min_v, double max_v,
                    int precision, double default_value);
bool gui_float_log(const char *label, float *v, float min_v, float max_v,
                   int precision, float default_value);

void gui_tabs(char *current);
void gui_tabs_end(void);
bool gui_tab(const char *label);
void gui_tab_end(void);
// Set the next tab as open at startup.
void gui_set_next_tab_open(void);

bool gui_input(const char *label, char *buffer, int len,
               const char **suggestions);
bool gui_input_multilines(const char *label, char *buffer, int len);

bool gui_date(double *utc);
// End of gui widgets.
// Not sure about the other functions yet.

double gui_panel_begin(const char *name, const double pos[2],
                                         const double size[2]);
void gui_panel_end(void);

// Stacked widgets.
// return true if the stack is empty (by default).  Otherwise call the
// top stack callback and return false.
bool gui_stack(const char *id, void *user);
void gui_stack_end(void);
void gui_stack_push(void (*f)(void *user), void *user);
void gui_stack_pop(void);

// Should we make this internal?
void gui_image(const char *url, int w, int h);

// Card widget:
// +----------+---+
// | content  |pic|
// +----------+---+
void gui_card(const char *label, const char *img);
bool gui_card_end();

void gui_separator(void);
void gui_header(const char *label);

void gui_fps_histo(const int *values, int size);

void gui_render_prepare(void);
void gui_render_finish(void);
