#include "log.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef RPS_STACKTRACE
#include <execinfo.h>
#endif


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
_log_hex(log_level level, const char *file, int line, char *data, int n) {
    int i;
    char buf[LOG_MAX_LEN];

    size_t len = 0;
    size_t size = LOG_MAX_LEN;

    for (i=0; i<n; i++) {
        if (len >= size) {
            break;
        }
        len += snprintf(buf + len, size - len, "%x ", data[i]);
    }

    _log(level, file, line, buf);
}

void
_log(log_level level, const char *file, int line, const char *fmt, ...) {
    struct logger *l = &logger;
    int len;
    size_t size;
    char buf[LOG_MAX_LEN];
    va_list args;
    struct timeval  tv;
    
    if (l->fd < 0) {
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
    len += sprintf(buf + len, "] ");
#ifdef RPS_DEBUG_OPEN
    len += snprintf(buf + len, size - len, "<%s:%d> ", file, line);
#else
    /* unused parameter */
    (void)file;
    (void)line;
#endif
    len += snprintf(buf + len, size - len, "%s: ", log_level_to_text(level)); 

    va_start(args, fmt);
    len += vsnprintf(buf + len, size - len, fmt, args);
    va_end(args);

    buf[len++] = '\n';
    write(l->fd, buf, len);
}

/* Reentrant function, can be safely called in signal handler */
void
_log_safe(const char *fmt, ...) {
    struct logger *l = &logger;
    int len, size, errno_save;
    char buf[LOG_MAX_LEN];
    va_list args;
    
    if (l->fd < 0) {
        return;
    }

    /* save and restore errno */
    errno_save = errno;
    len = 0;
    size = LOG_MAX_LEN;

    va_start(args, fmt);
    len += vsnprintf(buf + len, size - len, fmt, args);
    va_end(args);

    buf[len++] = '\n';
    write(l->fd, buf, len);

    errno = errno_save;
}

int
log_level_set(log_level level) {
    struct logger *l = &logger;
    
    if (level < LOG_CRITICAL || level > LOG_VERBOSE) {
        log_stderr("invalid log level <%d>", level);
        return -1;
    }
    l->level = level;
    
    return 0;
}

void
log_level_up(void) {
    struct logger *l = &logger;

    if (l->level < LOG_VERBOSE) {
        l->level++;
        log_safe("up log level to %s", log_level_to_text(l->level));
    } else {
        log_safe("up log level faild, current has been %s", 
                log_level_to_text(l->level));
    }
}

void
log_level_down(void) {
    struct logger *l = &logger;

    if (l->level > LOG_CRITICAL) {
        l->level--;
        log_safe("down log level to %s", log_level_to_text(l->level));
    } else {
        log_safe("down log level faild, current has been %s", 
                log_level_to_text(l->level));
    }
}

int
log_output_set(char *fname) {
    struct logger *l = &logger;
       
    if (fname == NULL || !(strlen(fname))) {
        l->fd = STDOUT_FILENO;
    } else {
        l->fd = open(fname, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (l->fd < 0) {
            log_stderr("opening log file '%s' failed: %s", fname, strerror(errno));
            return -1;
        }
    }

    return 0;
}

int
log_init(log_level level, char *fname) {
    int status;
    
    status = log_level_set(level);
    if (status != 0) {
        return status;
    }

    status = log_output_set(fname);
    if (status != 0) {
        return status;
    }

    return 0;
}

void
log_deinit() {
    struct logger *l = &logger;
    
    if (l->fd < 0  || l->fd == STDOUT_FILENO || l->fd == STDERR_FILENO) {
        return;
    }
    
    close(l->fd);
}

void
log_stacktrace() {
#ifdef RPS_STACKTRACE
    struct logger *l = &logger;
    void *stack[64];
    int size;
    
    if (l->fd < 0) {
        return;
    }

    size = backtrace(stack, 64);
    backtrace_symbols_fd(stack, size, l->fd);
#endif
}

#ifdef LOG_TEST
int
main(int argc, char **argv) {
    int stat;
    log_level level;

    level = log_level_to_int("DEBUG");
    
    stat = log_init(level, NULL);
    if (stat != 0) {
        log_stderr("init log failed.");
        exit(1);
    }
    
    log_debug("Hello %s", "kenshin");
}
#endif

