/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "progressbar.h"
#include "uthash.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct bar bar_t;
struct bar {
    UT_hash_handle  hh;
    char            *id;
    char            *label;
    int             v;
    int             total;
    int             error;
    char            *error_msg;

    int             last_update;
    int             keepalive;
};

// Global hash table of all the progress bars.
static bar_t *g_bars = NULL;
static void (*g_listener)(const char *id) = NULL;
static int g_tick = 0; // Global timer.


void progressbar_print_all(void)
{
    bar_t *bar;
    int i, n;
    for (n = 0, bar = g_bars; bar; bar = bar->hh.next, n++) {
        printf("\33[2K"); // Erase current line.
        printf("%s %d/%d\n", bar->label, bar->v, bar->total);
    }
    // Move back up.
    for (i = 0; i < n; i++) printf("\033[1A");
}

void progressbar_report(const char *id, const char *label, int v, int total,
                        int keepalive)
{
    bool changed = false;
    bar_t *bar;
    // Default value of about 0.5 sec at 60 fps.
    if (keepalive == -1) keepalive = 30;

    HASH_FIND_STR(g_bars, id, bar);

    // If we report a finished bar that doesn't exist, don't do anything.
    if (!bar && total == v) return;

    if (!bar) {
        bar = calloc(1, sizeof(*bar));
        bar->id = strdup(id);
        HASH_ADD_KEYPTR(hh, g_bars, bar->id, strlen(bar->id), bar);
    }

    if (label && (!bar->label || strcmp(bar->label, label) != 0)) {
        free(bar->label);
        bar->label = strdup(label);
        changed = true;
    }
    if (v != bar->v || total != bar->total) {
        changed = true;
    }
    if (v != total) bar->last_update = g_tick;
    bar->v = v;
    bar->total = total;
    bar->keepalive = keepalive;
    if (changed && g_listener) g_listener(id);
}

void progressbar_report_error(const char *id, const char *label,
                              int code, const char *msg)
{
    bar_t *bar;

    HASH_FIND_STR(g_bars, id, bar);

    if (!bar) {
        bar = calloc(1, sizeof(*bar));
        bar->id = strdup(id);
        HASH_ADD_KEYPTR(hh, g_bars, bar->id, strlen(bar->id), bar);
    }

    if (label && (!bar->label || strcmp(bar->label, label) != 0)) {
        free(bar->label);
        bar->label = strdup(label);
    }

    bar->error = code;
    free(bar->error_msg);
    bar->error_msg = strdup(msg);
    if (g_listener) g_listener(id);
}

void progressbar_update(void)
{
    bar_t *bar, *tmp;
    HASH_ITER(hh, g_bars, bar, tmp) {
        assert(bar->keepalive >= 0);
        if (bar->keepalive == 0 && bar->v < bar->total)
            continue;
        if (bar->keepalive > 0 && g_tick <= bar->last_update + bar->keepalive)
            continue;
        if (bar->error)
            continue;
        HASH_DEL(g_bars, bar);
        g_listener(bar->id);
        free(bar->id);
        free(bar->label);
        free(bar->error_msg);
        free(bar);
    }
    g_tick++;
}

int progressbar_list(void *user, void (*callback)(void *user,
                                      const char *id, const char *label,
                                      int v, int total,
                                      int error, const char *error_msg))
{
    bar_t *bar;
    int n;
    for (n = 0, bar = g_bars; bar; bar = bar->hh.next, n++)
        callback(user, bar->id, bar->label, bar->v, bar->total, bar->error,
                 bar->error_msg);
    return n;
}

void progressbar_add_listener(void (*f)(const char *id))
{
    assert(!g_listener || f == g_listener); // Only one listener supported.
    g_listener = f;
}
