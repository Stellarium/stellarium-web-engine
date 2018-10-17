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

int sys_device_sensors(int enable, double acc[3], double mag[3])
{
    if (!sys_callbacks.device_sensors) return -1;
    return sys_callbacks.device_sensors(sys_callbacks.user, enable, acc, mag);
}

int sys_get_position(double *lat, double *lon, double *alt, double *accuracy)
{
    if (!sys_callbacks.get_position) return -1;
    return sys_callbacks.get_position(
                        sys_callbacks.user, lat, lon, alt, accuracy);
}

const char *sys_translate(const char *domain, const char *str)
{
    if (!sys_callbacks.translate) return str;
    return sys_callbacks.translate(sys_callbacks.user, domain, str);
}

char *sys_render_text(const char *txt, float height, int *w, int *h)
{
    if (!sys_callbacks.render_text)
        return (void*)font_render(txt, height, w, h);
    return sys_callbacks.render_text(sys_callbacks.user, txt, height, w, h);
}
