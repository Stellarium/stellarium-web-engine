/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */


// Create or update a progressbar.
void progressbar_report(const char *id, const char *label, int v, int total);

// Iter all the progressbars.
int progressbar_list(void *user, void (*callback)(void *user,
                                      const char *id, const char *label,
                                      int v, int total));

// Callback called each time a progressbar changed.
void progressbar_add_listener(void (*f)(const char *id));

void progressbar_print_all(void);
