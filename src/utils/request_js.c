/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#ifdef __EMSCRIPTEN__

#define MAX_NB  16 // Max number of concurrent requests.

struct request
{
    char        *url;
    int         handle;
    int         status_code;
    bool        done;
    void        *data;
    int         size;
};


static struct {
    int nb;     // Number of current running requests.
} g = {};

void request_init(const char *cache_dir)
{
    // Ignore the cache dir with emscripten.
}

request_t *request_create(const char *url)
{
    request_t *req = calloc(1, sizeof(*req));
    req->url = strdup(url);
    return req;
}

void request_delete(request_t *req)
{
    if (!req) return;
    if (req->handle) {
        emscripten_async_wget2_abort(req->handle - 1);
        g.nb--;
    }
    free(req->url);
    free(req->data);
    free(req);
}

static bool is_str(const request_t *req)
{
    return str_endswith(req->url, ".txt") ||
           str_endswith(req->url, ".json") ||
           str_endswith(req->url, ".html") ||
           str_endswith(req->url, ".utf8") ||
           str_endswith(req->url, ".fab");
}

static void onload(unsigned int _, void *arg, void *data, unsigned int size)
{
    char *tmp;
    request_t *req = arg;
    req->handle = 0;
    req->status_code = 200; // XXX: get proper code.

    // For NULL terminated string for txt requests.
    if (is_str(req) && ((char*)data)[size] != 0) {
        tmp = data;
        data = calloc(1, size + 1);
        memcpy(data, tmp, size);
        free(tmp);
    }

    req->data = data;
    req->size = size;
    req->done = true;
    g.nb--;
}

static void onerror(unsigned int _, void *arg, int err, const char *msg)
{
    request_t *req = arg;
    LOG_D("onerror %s %d %s", req->url, err, msg);
    req->handle = 0;
    // Use a default error code if we didn't get one...
    req->status_code = err ?: 499;
    req->done = true;
    g.nb--;
}

static void onprogress(unsigned int _, void *arg, int nb_bytes, int size)
{
}

const void *request_get_data(request_t *req, int *size, int *status_code)
{
    int handle;
    if (!req->done && !req->handle && g.nb < MAX_NB) {
        handle = emscripten_async_wget2_data(
                req->url, "GET", NULL, req, false,
                onload, onerror, onprogress);
        req->handle = handle + 1; // So that we cannot get 0.
        g.nb++;
    }
    if (size) *size = req->size;
    if (status_code) *status_code= req->status_code;
    return req->data;
}

void request_make_fresh(request_t *req)
{
}

#endif
