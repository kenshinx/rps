#include "log.h"
#include "core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>


static struct logger logger;


void
_log_stream(FILE *stream, const char *fmt, ...) {
    char buf[LOG_MAX_LEN];
    va_list args;
    int len;
    
    len = 0;

    va_start(args, fmt);
    len += vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    buf[len++] = '\n';
    
    fwrite(buf, 1, len, stream);
}

void
_log(log_level level, const char *file, int line, const char *fmt, ...) {
    struct logger *l = &logger;
    int len;
    size_t size;
    char buf[LOG_MAX_LEN];
    va_list args;
    struct timeval  tv;
    
    if (l->fd == NULL) {
        return;
    }

    if (l->level < level) {
        return;
    }

    len = 0;
    size = LOG_MAX_LEN;

    gettimeofday(&tv, NULL);
    buf[len++] = '[';
    len += strftime(buf + len, size - len, "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
    len += snprintf(buf + len, size - len, "] <%s:%d> ", file, line);
    len += snprintf(buf + len, size - len, "%s: ", log_level_to_text(level)); 

    va_start(args, fmt);
    len += vsnprintf(buf + len, size - len, fmt, args);
    va_end(args);

    buf[len++] = '\n';

    fwrite(buf, 1, len, l->fd);
}

rps_status_t
log_set_level(log_level level) {
    struct logger *l = &logger;
    
    if (level < LOG_CRITICAL || level > LOG_VERBOSE) {
        log_stderr("invalid log level <%d>", level);
        return RPS_ERROR;
    }
    l->level = level;
    
    return RPS_OK;
}

rps_status_t
log_set_output(char *fname) {
    struct logger *l = &logger;
       
    if (fname == NULL || !(strlen(fname))) {
        l->fd = stdout;
    } else {
        l->fd = fopen(fname, "a");
        if (l->fd == NULL) {
            log_stderr("opening log file '%s' failed: %s", fname, strerror(errno));
            return RPS_ERROR;
        }
    }

    return RPS_OK;
}

rps_status_t
log_init(log_level level, char *fname) {
    rps_status_t status;
    
    status = log_set_level(level);
    if (status != RPS_OK) {
        return status;
    }

    status = log_set_output(fname);
    if (status != RPS_OK) {
        return status;
    }

    return RPS_OK;
}

void
log_deinit() {
    struct logger *l = &logger;
    
    if (l->fd == NULL || l->fd == stdout || l->fd == stderr) {
        return;
    }
    
    fclose(l->fd);
}

#ifdef LOG_TEST
int
main(int argc, char **argv) {
    rps_status_t stat;
    log_level level;

    level = log_level_to_int("DEBUG");
    
    stat = log_init(level, NULL);
    if (stat != RPS_OK) {
        log_stderr("init log failed.");
        exit(1);
    }
    
    log_debug("Hello %s", "kenshin");
}
#endif

