/* Stellarium Web Engine - Copyright (c) 2022 - Stellarium Labs SRL
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

void gui_same_line(void);

void gui_init(void *user);
void gui_release(void);
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

// End of gui widgets.
// Not sure about the other functions yet.

double gui_panel_begin(const char *name, const double pos[2],
                                         const double size[2]);
void gui_panel_end(void);

void gui_separator(void);
void gui_header(const char *label);

void gui_fps_histo(const int *values, int size);

void gui_render_prepare(void);
void gui_render_finish(void);
