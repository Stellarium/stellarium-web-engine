/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef NO_LIBCURL

#include "request.h"
#include "utstring.h"

#include <assert.h>
#include <curl/curl.h>
#include <errno.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifndef LOG_E
#   define LOG_E
#endif

#ifndef PATH_MAX
#   define PATH_MAX 1024
#endif

#define MAX_NB  16

// static data.
static struct {
    CURLM        *curlm;
    char         *cache_dir;
    int          nb; // Number of current running handles.
} g = {};

struct request
{
    char        *url;
    CURL        *handle;
    UT_string   data_buf;       // Receive the data.
    UT_string   header_buf;     // Receive the header.
    long        status_code;    // HTTP status code
    void        *data;          // Actual data.
    int         size;
    bool        done;           // Request finished
    char        *local_path;    // Data saved to this file

    struct curl_slist *headers;
    char        *etag;
    double      expiration;     // Unix time expiration date.
};

static const char *request_get_file(request_t *req, int *status_code);

static void *read_file(const char *path, int *size)
{
    FILE *file;
    char *ret;
    int read_size __attribute__((unused));
    int size_default;

    size = size ?: &size_default; // Allow to pass NULL as size;
    file = fopen(path, "rb");
    if (!file) return NULL;
    assert(file);
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    ret = malloc(*size + 1);
    read_size = fread(ret, *size, 1, file);
    assert(read_size == 1 || *size == 0);
    ret[*size] = '\0';
    fclose(file);
    return ret;
}

static double get_unix_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000. / 1000.;
}

static bool file_exists(const char *path)
{
    FILE *file;
    if ((file = fopen(path, "r"))) {
        fclose(file);
        return true;
    }
    return false;
}

/*
 * Create directories for a given file path.
 */
static int ensure_dir(const char *path)
{
    char tmp[PATH_MAX];
    char *p;
    strcpy(tmp, path);
    for (p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if ((mkdir(tmp, S_IRWXU) != 0) && (errno != EEXIST)) return -1;
        *p = '/';
    }
    return 0;
}

static char *create_local_path(const char *url, const char *suffix)
{
    char *ret;
    int i, r;
    r = asprintf(&ret, "%s/%s%s", g.cache_dir, url, suffix ?: "");
    if (r == -1) LOG_E("Error");
    for (i = strlen(g.cache_dir) + 1; ret[i]; i++) {
        if (ret[i] == '/' || ret[i] == ':')
            ret[i] = '_';
    }
    ensure_dir(ret);
    return ret;
}

void request_init(const char *cache_dir)
{
    assert(cache_dir);
    if (!g.curlm) g.curlm = curl_multi_init();
    free(g.cache_dir);
    g.cache_dir = strdup(cache_dir);
}

request_t *request_create(const char *url)
{
    char *local_path, *info_path;
    char etag[128];
    double expiration;
    FILE *file;
    int r;
    request_t *req = calloc(1, sizeof(*req));
    req->url = strdup(url);

    assert(strchr(url, ':')); // Make sure we have a protocol.

    // Check for cache info.
    local_path = create_local_path(url, NULL);
    info_path = create_local_path(url, ".info");
    if (file_exists(local_path) && file_exists(info_path)) {
        file = fopen(info_path, "r");
        r = fscanf(file, "etag: %s\n", etag);
        if (r == 1) req->etag = strdup(etag);
        r = fscanf(file, "expiration: %lf\n", &expiration);
        if (r == 1) req->expiration = expiration;
        fclose(file);

        // If the cached version is not expired yet just use it.
        if (req->expiration && req->expiration > get_unix_time()) {
            req->local_path = strdup(local_path);
            req->status_code = 200;
            req->done = true;
        }
    }

    free(local_path);
    free(info_path);
    return req;
}

int request_is_finished(const request_t *req)
{
    return req->handle == NULL;
}

void request_delete(request_t *req)
{
    if (!req) return;
    if (req->handle) LOG_E("Aborting request not implemented yet!");
    if (req->data != utstring_body(&req->data_buf)) free(req->data);
    utstring_done(&req->data_buf);
    utstring_done(&req->header_buf);
    free(req->url);
    free(req->local_path);
    free(req->etag);
    if (req->headers) curl_slist_free_all(req->headers);
    free(req);
}

static void save_cache(const char *url, const char *path, const char *etag,
                       double expiration)
{
    char *info_path;
    FILE *file;
    int r;
    r = asprintf(&info_path, "%s.info", path);
    if (r == -1) LOG_E("Error");
    file = fopen(info_path, "w");
    fprintf(file, "etag: %s\n", etag);
    fprintf(file, "expiration: %.0f\n", expiration);
    fclose(file);
    free(info_path);
}

static bool header_find(const char *header, const char *re,
                        char *buf, int buf_size)
{
    bool ret = false;
    int r, len;
    regex_t reg;
    regmatch_t matches[2];

    regcomp(&reg, re, REG_EXTENDED | REG_ICASE);
    r = regexec(&reg, header, 2, matches, 0);
    if (r == 0) {
        len = matches[1].rm_eo - matches[1].rm_so;
        if (len < buf_size) {
            strncpy(buf, header + matches[1].rm_so, len);
            buf[len] = '\0';
            ret = true;
        }
    }

    regfree(&reg);
    return ret;
}

static void on_done(request_t *req)
{
    char buf[128] = {};
    const char *header;
    const char *path;

    assert(!req->local_path);

    // The resource didn't change.
    if (req->status_code / 100 == 3) {
        req->local_path = create_local_path(req->url, NULL);
        assert(file_exists(req->local_path));
    }

    if (req->status_code / 100 != 2) goto end;

    // Parse header for cache control.
    header = utstring_body(&req->header_buf);
    if (header_find(header, "ETag: \"(.+)\"\r\n", buf, sizeof(buf))) {
        free(req->etag);
        req->etag = strdup(buf);
    }
    if (header_find(header, "Cache-Control: max-age=([0-9]+)\r\n",
                    buf, sizeof(buf))) {
        req->expiration = get_unix_time() + atof(buf);
    }
    // For the moment we save all the files in the cache as long as they
    // have an etag.  We also never clean the cache!
    if (req->etag) {
        path = request_get_file(req, NULL);
        save_cache(req->url, path, req->etag, req->expiration);
    }

end:
    return;
}

static void update(void)
{
    int nb, msgs_in_queue;
    CURLMsg *msg;
    CURL *handle;
    request_t *req;
    static double last = 0;

    // Avoid loading too many resources too fast to keep a good framerate.
    if ((get_unix_time() - last) < 16.0 / 1000) return;

    assert(g.curlm);
    curl_multi_perform(g.curlm, &nb);
    if (nb == g.nb) return;
    while ((msg = curl_multi_info_read(g.curlm, &msgs_in_queue))) {
        if (msg->msg == CURLMSG_DONE) {
            handle = msg->easy_handle;
            curl_easy_getinfo(handle, CURLINFO_PRIVATE, (char**)&req);
            curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE,
                                      &req->status_code);
            // Convention: returns a server timeout if the connection failed.
            if (!req->status_code && msg->data.result)
                req->status_code = 598;
            g.nb--;
            curl_multi_remove_handle(g.curlm, handle);
            curl_easy_cleanup(handle);
            req->handle = NULL;
            req->done = true;
            if (req->status_code / 100 == 2) {
                req->size = utstring_len(&req->data_buf);
                // Add a 0 byte at the end of the data, this is useful for
                // text resources.
                utstring_bincpy(&req->data_buf, "", 1);
                req->data = utstring_body(&req->data_buf);
            }
            on_done(req);
            last = get_unix_time();
            break;
        }
    }
}

static size_t write_callback(
        char *ptr, size_t size, size_t nmemb, void *userdata)
{
    UT_string *buf = userdata;
    size_t len = size * nmemb;
    utstring_bincpy(buf, ptr, len);
    return len;
}

static void req_update(request_t *req)
{
    int r;
    char *tmp;
    assert(g.curlm); // Check that request_init was called!
    if (req->done) return;
    if (!req->handle && g.nb < MAX_NB) {
        req->handle = curl_easy_init();
        utstring_init(&req->data_buf);
        utstring_init(&req->header_buf);
        curl_easy_setopt(req->handle, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(req->handle, CURLOPT_WRITEDATA, &req->data_buf);
        curl_easy_setopt(req->handle, CURLOPT_HEADERDATA, &req->header_buf);
        curl_easy_setopt(req->handle, CURLOPT_URL, req->url);
        curl_easy_setopt(req->handle, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(req->handle, CURLOPT_PRIVATE, req);
        curl_easy_setopt(req->handle, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(req->handle, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(req->handle, CURLOPT_SSL_VERIFYHOST, 0);
        // curl_easy_setopt(req->handle, CURLOPT_VERBOSE, 1);
        if (req->etag) {
            r = asprintf(&tmp, "If-None-Match: \"%s\"", req->etag);
            if (r == -1) LOG_E("Error");
            req->headers = curl_slist_append(req->headers, tmp);
            free(tmp);
        }
        if (req->headers)
            curl_easy_setopt(req->handle, CURLOPT_HTTPHEADER, req->headers);

        curl_multi_add_handle(g.curlm, req->handle);
        g.nb++;
    }

    update();
}

static const char *request_get_file(request_t *req, int *status_code)
{
    FILE *file;
    if (!req->local_path) {
        req_update(req);
        if (req->done && req->status_code / 100 == 2) {
            req->local_path = create_local_path(req->url, NULL);
            file = fopen(req->local_path, "wb");
            if (!file) perror(NULL);
            assert(file);
            fwrite(utstring_body(&req->data_buf), 1, req->size, file);
            fclose(file);
        }
    }
    if (status_code) *status_code = req->status_code;
    return req->local_path;
}

const void *request_get_data(request_t *req, int *size, int *status_code)
{
    req_update(req);
    if (status_code) *status_code = req->status_code;
    if (!req->done) {
        if (size) *size = 0;
        return NULL;
    }
    // Local file, copy it into the data buffer.
    if (!req->data && req->local_path) {
        req->data = read_file(req->local_path, &req->size);
    }
    if (size) *size = req->size;
    return req->data;
}

void request_make_fresh(request_t *req)
{
    free(req->etag);
    req->etag = NULL;
}

#else // NO_LIBCURL

#ifdef REQUEST_DUMMY

#include "request.h"
#include <stdlib.h>

struct request
{
};

void request_init(const char *cache_dir)
{
}

request_t *request_create(const char *url)
{
    return calloc(1, sizeof(request_t));
}

int request_is_finished(const request_t *req)
{
    return true;
}

void request_delete(request_t *req)
{
    free(req);
}
const void *request_get_data(request_t *req, int *size, int *status_code)
{
    *size = 0;
    *status_code = 598;
    return NULL;
}

#endif // REQUEST_DUMMY

#endif // NO_LIBCURL
