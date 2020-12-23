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

// Fix mkdir on MINGW
#ifdef WIN32
#   define mkdir(p, m) mkdir(p)
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
#ifndef WIN32
    time_t t = time(NULL);
    struct tm lt = {0};
    localtime_r(&t, &lt);
    return lt.tm_gmtoff;
#else
    // Not implemented yet.
    return 0;
#endif
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

int sys_device_sensors(bool enable_accelero, bool enable_magneto,
                       double acc[3], double mag[3], int *rot,
                       double *calibration_level)
{
    if (!sys_callbacks.device_sensors) return -1;
    return sys_callbacks.device_sensors(
            sys_callbacks.user, enable_accelero, enable_magneto, acc, mag, rot,
            calibration_level);
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
           strcmp(domain, "sky") == 0 ||
           strcmp(domain, "skyculture") == 0);
    if (!sys_callbacks.translate) return str;
    return sys_callbacks.translate(sys_callbacks.user, domain, str);
}

const char *sys_get_lang()
{
    if (!sys_callbacks.get_lang) return "en";
    return sys_callbacks.get_lang();
}

bool sys_lang_supports_spacing()
{
    static int lang_has_spacing = -1;
    if (lang_has_spacing != -1)
        return lang_has_spacing;
    lang_has_spacing = strncmp(sys_get_lang(), "ar", 2) != 0 &&
                       strncmp(sys_get_lang(), "zh", 2) != 0 &&
                       strncmp(sys_get_lang(), "ja", 2) != 0 &&
                       strncmp(sys_get_lang(), "ko", 2) != 0;
    lang_has_spacing = lang_has_spacing ? 1 : 0;
    return lang_has_spacing;
}

/*
 * Function: sys_render_text
 * Render text into a texture buffer.
 *
 * Parameters:
 *   txt    - A utf string.
 *   height - The height of the font.
 *   flags  - Only accepted flag is LABEL_BOLD.
 *   w      - Output width of the buffer.
 *   h      - Output height of the buffer.
 *
 * Returns:
 *   An allocated buffer of one byte per pixel texture.
 */
char *sys_render_text(const char *txt, float height, int flags,
                      int *w, int *h, int* xoffset, int* yoffset)
{
    assert(sys_callbacks.render_text);
    return sys_callbacks.render_text(
            sys_callbacks.user, txt, height, flags, w, h, xoffset, yoffset);
}

EMSCRIPTEN_KEEPALIVE
void sys_set_translate_function(
    const char *(*callback)(void *user, const char *domain, const char *str))
{
    sys_callbacks.translate = callback;
}
