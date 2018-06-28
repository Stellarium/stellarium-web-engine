/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

void sys_log(const char *msg)
{
    printf("%s\n", msg);
    fflush(stdout);
}

double sys_get_unix_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000. / 1000.;
}

int sys_get_utc_offset(void)
{
    time_t t = time(NULL);
    struct tm lt = {0};
    localtime_r(&t, &lt);
    return lt.tm_gmtoff;
}

int sys_storage_store(const char *db, const char *name, void *buf, int size)
{
    FILE *file;
    char path[1024];
    mkdir(".store", 0777);
    sprintf(path, ".store/%s", db);
    mkdir(path, 0777);
    sprintf(path, ".store/%s/%s", db, name);
    file = fopen(path, "w");
    assert(file);
    fwrite(buf, 1, size, file);
    fclose(file);
    return 0;
}

void *sys_storage_load(const char *db, const char *name, int *size,
                       int *error)
{
    FILE *file;
    char path[1024];
    char *ret;
    int read_size __attribute__((unused));
    int size_default;

    size = size ?: &size_default; // Allow to pass NULL as size;
    sprintf(path, ".store/%s/%s", db, name);
    file = fopen(path, "rb");
    if (!file) {
        if (error) *error = 404;
        return NULL;
    }
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
