#ifndef _RPG_LOG_H
#define _RPG_LOG_H

#include <stdio.h>
#include <string.h>

#define LOG_MAX_LEN 256

typedef enum {
    LOG_CRITICAL = 0,
    LOG_ERROR,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG,
    LOG_VERBOSE,
    LOG_LEVEL_UNDEFINED = -1,
} log_level;

static const char *LOG_LEVEL_TEXT[] = {
    "CRIT",
    "ERROR",
    "WARN",
    "NOTICE",
    "INFO",
    "DEBUG",
    "VERB"
};

#define log_stdout(...) do {                                    \
    _log_stream(stdout, __VA_ARGS__);                           \
} while(0)                                                      \

#define log_stderr(...) do {                                    \
    _log_stream(stderr, __VA_ARGS__);                           \
} while(0)                                                      \

#define log_hex(level, ...) do {               \
    _log_hex(level, __FILE__, __LINE__, __VA_ARGS__);       \
} while(0)                                                      \

#define log_verb(...) do {                                      \
    _log(LOG_VERBOSE, __FILE__, __LINE__, __VA_ARGS__);            \
} while(0)                                                      \

#define log_debug(...) do {                                     \
    _log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__);           \
} while(0)                                                      \

#define log_info(...) do {                                      \
    _log(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__);            \
} while(0)                                                      \
    
#define log_notice(...) do{                                     \
    _log(LOG_NOTICE, __FILE__, __LINE__, __VA_ARGS__);          \
} while(0)                                                      \

#define log_warn(...) do {                                      \
    _log(LOG_WARNING, __FILE__, __LINE__, __VA_ARGS__);         \
} while(0)                                                      \

#define log_error(...) do {                                     \
    _log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__);           \
} while(0)                                                      \


#define log_crit(...) do {                                      \
    _log(LOG_CRITICAL, __FILE__, __LINE__, __VA_ARGS__);        \
} while(0)                                                      \

#define log_level_to_text(level)    (LOG_LEVEL_TEXT[level])

static inline log_level
log_level_to_int(const char *level) {
    int i;
    int n = sizeof(LOG_LEVEL_TEXT) / sizeof(LOG_LEVEL_TEXT[0]);

    for(i = 0; i < n; i++) {
        if (strcmp(LOG_LEVEL_TEXT[i], level) == 0) {
            return i;
        }
    }

    return LOG_LEVEL_UNDEFINED;
}


struct logger {
    FILE        *fd;
    log_level   level;
};

#ifdef RPS_DEBUG_OPEN
void _log(log_level level, const char *file, int line, const char *fmt, ...);
#else
void _log(log_level level, const char *fmt, ...);
#endif

void _log_stream(FILE *stream, const char *fmt, ...);
void _log_hex(log_level level, const char *file, int line, char *data, int n);
int log_set_level(log_level level);
int log_set_output(char *fname);
int log_init(log_level level, char *fname);
void log_deinit();

#endif
