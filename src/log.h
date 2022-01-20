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

#define log_hex(level, ...) do {                                \
    _log_hex(level, __FILE__, __LINE__, __VA_ARGS__);           \
} while(0)                                                      \

#define log_verb(...) do {                                      \
    _log(LOG_VERBOSE, __FILE__, __LINE__, __VA_ARGS__);         \
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

#define log_safe(...) do {                                      \
    _log_safe(__VA_ARGS__);                                     \
} while(0)                                                      \

#define log_level_to_text(level)    (LOG_LEVEL_TEXT[level])

static inline int
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
    int         fd;
    log_level   level;
};


void _log_stream(FILE *stream, const char *fmt, ...);
void _log(log_level level, const char *file, int line, const char *fmt, ...);
void _log_hex(log_level level, const char *file, int line, char *data, int n);
void _log_safe(const char *fmt, ...);
int log_level_set(log_level level);
void log_level_up(void);
void log_level_down(void);
int log_output_set(char *fname);
int log_init(log_level level, char *fname);
void log_deinit();
void log_stacktrace();

#endif
