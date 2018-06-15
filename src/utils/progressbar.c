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
};

// Global hash table of all the progress bars.
static bar_t *g_bars = NULL;
static void (*g_listener)(const char *id) = NULL;

static bar_t *bar_get(const char *id)
{
    bar_t *bar;
    HASH_FIND_STR(g_bars, id, bar);
    if (!bar) {
        bar = calloc(1, sizeof(*bar));
        bar->id = strdup(id);
        HASH_ADD_KEYPTR(hh, g_bars, bar->id, strlen(bar->id), bar);
    }
    return bar;
}

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

void progressbar_report(const char *id, const char *label, int v, int total)
{
    bool changed = false;
    bar_t *bar;
    bar = bar_get(id);
    if (!bar->label || strcmp(bar->label, label) != 0) {
        free(bar->label);
        bar->label = strdup(label);
        changed = true;
    }
    if (v != bar->v || total != bar->total) changed = true;
    bar->v = v;
    bar->total = total;
    if (changed && g_listener) g_listener(id);
}

int progressbar_list(void *user, void (*callback)(void *user,
                                      const char *id, const char *label,
                                      int v, int total))
{
    bar_t *bar;
    int n;
    for (n = 0, bar = g_bars; bar; bar = bar->hh.next, n++)
        callback(user, bar->id, bar->label, bar->v, bar->total);
    return n;
}

void progressbar_add_listener(void (*f)(const char *id))
{
    assert(!g_listener || f == g_listener); // Only one listener supported.
    g_listener = f;
}
