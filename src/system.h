/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: system.h
 * Contains some functions whose implementation depend on the target OK
 */

/*
 * Function: sys_log
 * Print a line of text to the console.
 *
 * It's better to call one of the LOG macro instead.
 */
void sys_log(const char *msg);

/*
 * Function: sys_get_unix_time
 * Return the unix time (in seconds)
 */
double sys_get_unix_time(void);

/*
 * Function: sys_get_utc_offset
 * Return the local time UTC offset in seconds
 */
int sys_get_utc_offset(void);

/*
 * Function: sys_get_user_dir
 * Return the user data directory.
 */
const char *sys_get_user_dir(void);

/*
 * Function: sys_device_sensors
 * Get the readings from the device accelerometers and magnetometer.
 *
 * Parameters:
 *   enable - set to 1 to enable the sensors, 0 to stop them.
 *   acc    - get the accelerometer readings.
 *   mag    - get the magnetomer readings.
 *
 * Return:
 *   0 on success.
 */
int sys_device_sensors(int enable, double acc[3], double mag[3]);

int sys_get_position(double *lat, double *lon, double *alt, double *accuracy);

/*
 * Function: sys_translate
 * Translate a string in current locale.
 *
 * Parameters:
 *   domain - optional domain hint for the translator.
 *   str    - the string to translate.
 *
 * Return:
 *   The translated string.
 */
const char *sys_translate(const char *domain, const char *str);

/*
 * Global structure that holds pointers to functions that allow to change
 * the behavior of system calls.
 */
typedef struct {
    void *user;
    void (*log)(void *user, const char *msg);
    const char *(*get_user_dir)(void *user);
    int (*device_sensors)(void *user, int enable,
                          double acc[3], double mag[3]);
    int (*get_position)(void *user, double *lat, double *lon,
                        double *alt, double *accuracy);
    const char *(*translate)(void *user, const char *domain, const char *str);
} sys_callbacks_t;

extern sys_callbacks_t sys_callbacks;
