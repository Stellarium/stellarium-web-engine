/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef IDENTIFIERS_H
#define IDENTIFIERS_H

#include <stdbool.h>
#include <stdint.h>

/***** Identifiers manager *************************************************/
// XXX: cleanup this API!  Maybe rename to 'names'
void identifiers_init(void);
int identifiers_make_canonical(const char *v, char *out, int n);

void identifiers_add(const char *cat, const char *value,
                     uint64_t oid, uint64_t hint, char type[4],
                     double score,
                     const char *search_value, const char *show_value);

bool identifiers_iter_(uint64_t oid, const char *catalog,
                       uint64_t *roid,
                       uint64_t *hint,
                       const char **rcat,
                       const char **value,
                       const char **can,
                       const char **show_value,
                       char type[4],
                       double *score,
                       void **tmp);
// Convenience macro to iter identifiers.
#define IDENTIFIERS_ITER(id, cat, roid, rhint, rcat, rvalue, rcan, rshow, \
                         rtype, rscore) \
    for (void* tmp = NULL; \
         identifiers_iter_(id, cat, roid, rhint, rcat, rvalue, rcan, rshow, \
                           rtype, rscore, &tmp); )

// Return the first identifier of a catalog for a given id.
const char *identifiers_get(uint64_t oid, const char *catalog);

// Return the first id that corresponds to a given query.
uint64_t identifiers_search(const char *query);

#endif // IDENTIFIERS_H
