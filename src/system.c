/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

// The global system instance.
sys_callbacks_t sys_callbacks = {};

void sys_log(const char *msg)
{
    if (sys_callbacks.log) {
        sys_callbacks.log(sys_callbacks.user, msg);
    } else {
        printf("%s\n", msg);
        fflush(stdout);
    }
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

const char *sys_get_user_dir(void)
{
    if (sys_callbacks.get_user_dir) {
        return sys_callbacks.get_user_dir(sys_callbacks.user);
    } else {
        return ".";
    }
}

int sys_make_dir(const char *path)
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

int sys_device_sensors(int enable, double acc[3], double mag[3], int *rot)
{
    if (!sys_callbacks.device_sensors) return -1;
    return sys_callbacks.device_sensors(
            sys_callbacks.user, enable, acc, mag, rot);
}

int sys_get_position(double *lat, double *lon, double *alt, double *accuracy)
{
    if (!sys_callbacks.get_position) return -1;
    return sys_callbacks.get_position(
                        sys_callbacks.user, lat, lon, alt, accuracy);
}

const char *sys_translate(const char *domain, const char *str)
{
    assert(domain);
    assert(strcmp(domain, "gui") == 0 ||
           strcmp(domain, "skyculture") == 0);
    if (!sys_callbacks.translate) return str;
    return sys_callbacks.translate(sys_callbacks.user, domain, str);
}

int sys_list_dir(const char *dirpath, void *user,
                 int (*f)(void *user, const char *path, int is_dir))
{
    DIR *dir;
    struct dirent *dirent;
    char buf[1024];
    if (sys_callbacks.list_dir)
        return sys_callbacks.list_dir(sys_callbacks.user, dirpath, user, f);
    dir = opendir(dirpath);
    if (!dir) return -1;
    while ((dirent = readdir(dir))) {
        if (dirent->d_name[0] == '.') continue;
        snprintf(buf, sizeof(buf) - 1, "%s/%s", dirpath, dirent->d_name);
        f(user, buf, dirent->d_type == DT_DIR);
    }
    closedir(dir);
    return 0;
}

/*
 * Function: sys_list_fonts
 * List all the font files available.
 *
 * Parameters:
 *   user   - User data passed to the callback.
 *   f      - Callback function called once per font file.  If it returns
 *            a value different than zero we stop the iteration.  The
 *            callback takes the following arguments:
 *              user        - The original user data.
 *              path        - Path to the font file.
 *              name        - Name for the font.
 *              fallback    - If not NULL, name of a previous font this font
 *                            should be used as a fallback for.
 *              scale       - Auto scale to apply (this is to fix a problem
 *                            with nanovg that seems to render some fonts
 *                            with the wrong size!).
 * Return:
 *   The number of fonts listed.
 */
int sys_list_fonts(void *user,
                   int (*f)(void *user, const char *path,
                            const char *name, const char *fallback,
                            float scale))
{
    if (sys_callbacks.list_fonts)
        return sys_callbacks.list_fonts(sys_callbacks.user, user, f);
    return 0;
}
