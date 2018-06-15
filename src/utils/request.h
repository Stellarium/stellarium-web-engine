/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */


typedef struct request request_t;

void request_init(const char *cache_dir);
request_t *request_create(const char *url);
void request_delete(request_t *req);
const void *request_get_data(request_t *req, int *size, int *status_code);
// Don't use cache even if we have a local copy.
void request_make_fresh(request_t *req);

// Wait for a request to complete or fail.
// Only for testing, since it is blocking.
void request_wait(request_t *req);
// Called every time a request state change.
void request_add_global_listener(void (*f)(const char *url, int state));

