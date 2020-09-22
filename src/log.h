/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef LOG_H
#define LOG_H

/*
 * Define all the LOG_X macros
 */

enum {
    NOC_LOG_VERBOSE = 2,
    NOC_LOG_DEBUG   = 3,
    NOC_LOG_INFO    = 4,
    NOC_LOG_WARN    = 5,
    NOC_LOG_ERROR   = 6,
};

#ifndef LOG_LEVEL
#   if DEBUG || !defined(DEBUG)
#       define LOG_LEVEL NOC_LOG_DEBUG
#   else
#       define LOG_LEVEL NOC_LOG_INFO
#   endif
#endif

#define LOG(level, msg, ...) do { \
    if (level >= LOG_LEVEL) \
        dolog(level, msg, __func__, __FILE__, __LINE__, ##__VA_ARGS__); \
} while(0)

#define LOG_V(msg, ...) LOG(NOC_LOG_VERBOSE, msg, ##__VA_ARGS__)
#define LOG_D(msg, ...) LOG(NOC_LOG_DEBUG,   msg, ##__VA_ARGS__)
#define LOG_I(msg, ...) LOG(NOC_LOG_INFO,    msg, ##__VA_ARGS__)
#define LOG_W(msg, ...) LOG(NOC_LOG_WARN,    msg, ##__VA_ARGS__)
#define LOG_E(msg, ...) LOG(NOC_LOG_ERROR,   msg, ##__VA_ARGS__)

#define LOG_W_ONCE(msg, ...) do { \
    static bool _tmp = false; \
    if (!_tmp) { LOG_W(msg, ##__VA_ARGS__); _tmp = true; } \
} while(0)

void dolog(int level, const char *msg,
           const char *func, const char *file, int line, ...);

#endif // LOG_H
