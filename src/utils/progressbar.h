/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */


/*
 * Function: progressbar_report
 * Create or update a progressbar.
 *
 * This add a progressbar in the global list, or update its progress if it
 * already exists.
 *
 * Parameters:
 *    id        - Unique id of this progressbar.
 *    label     - Label to show.
 *    v         - Current progress value.
 *    total     - Progress total.
 *    keepalive - Defines when we remove the progressbar from the list.
 *                If set to 0, we keep the progressbar alive until we get
 *                a value v equal to total.
 *                If set to a positive value, we keep the progressbar alive
 *                for this number of frames, regardless of the progress
 *                status.
 *                if set to -1, we use a default small value.
 */
void progressbar_report(const char *id, const char *label, int v, int total,
                        int keepalive);

void progressbar_report_error(const char *id, const char *label,
                              int code, const char *msg);

/*
 * Function: progressbar_update
 * Update all the progressbars and remove the one that are inactives
 */
void progressbar_update(void);

/*
 * Function: progressbar_list
 * Iter all the progressbars.
 */
int progressbar_list(void *user, void (*callback)(void *user,
                                      const char *id, const char *label,
                                      int v, int total,
                                      int error, const char *error_msg));

// Callback called each time a progressbar changed.
void progressbar_add_listener(void (*f)(const char *id));

/*
 * Function: progressbar_print_all
 * Debug function that print all the progressbar to stdout.
 */
void progressbar_print_all(void);
