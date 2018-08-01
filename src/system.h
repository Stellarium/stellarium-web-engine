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
