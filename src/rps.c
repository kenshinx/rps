#include "rps.h"
#include "core.h"
#include "log.h" 
#include "config.h"
#include "util.h"
#include "server.h"
#include "upstream.h"
#include "_signal.h"

#include <uv.h>

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>


static struct option long_options[] = {
    { "help",        no_argument,        NULL,   'h' },
    { "version",     no_argument,        NULL,   'V' },
    { "daemonize",   no_argument,        NULL,   'd' },
    { "verbose",     no_argument,        NULL,   'v' },
    { "conf-file",   required_argument,  NULL,   'c' },
    {  NULL,         0,                  NULL,    0  }
};

static char short_options[] = "hVdvc:p:";

static void
rps_show_usage() {
    log_stderr(
        "Usage: ./rps -c conf/rps.yaml" CRLF
        ""
    );
    log_stderr(
        "Options:" CRLF
        "   -h, --help           :this help" CRLF
        "   -V, --version        :show version and exit" CRLF
        "   -v, --verbose        :set log level be debug"  
    );
    log_stderr(
        "   -d, --daemon         :run as daemonize" CRLF
        "   -c, --config=S       :set configuration file (default: %s)" CRLF
        "",
        RPS_DEFAULT_CONFIG_FILE 
    );
    exit(1);
}

static void
rps_show_version() {
    log_stdout("rps %s", RPS_VERSION);
    exit(0);
} 

static void
rps_init(struct application *app) {
    app->log_level = RPS_DEFAULT_LOG_LEVEL;
    app->log_filename = RPS_DEFAULT_LOG_FILE;
    
    app->pid = (pid_t)-1;
    app->pid_filename = RPS_DEFAULT_PID_FILE;
    
    app->config_filename = RPS_DEFAULT_CONFIG_FILE;

    app->daemon = 0;
    app->verbose = 0;

    rps_init_random();
    
    log_init(app->log_level, app->log_filename);
	
}

static rps_status_t
rps_get_options(int argc, char **argv, struct application *app) {

    char c;

    for (;;) {
        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) {
            /* no more options */
            break;
        }

        switch (c) {
            case 'h':
                rps_show_usage();
            case 'V':
                rps_show_version();
            case 'd':
                app->daemon = 1;
                break;
            case 'v':
                app->log_level = MAX(LOG_DEBUG, RPS_DEFAULT_LOG_LEVEL);
                app->verbose = 1;
                break;
            case 'c':
                app->config_filename = optarg;
                break;
            case '?':
                /* getopt_long already printed an error message. */
                return RPS_ERROR;
            default:
                return RPS_ERROR;
        }
        
    }
    return RPS_OK;
}

static void
rps_daemonize() {
    pid_t pid, sid;
    int fd;
    struct sigaction sa;

    pid = fork();
    switch (pid) {
    case -1:
        log_stderr("fork() failed: %s", strerror(errno));
        exit(1);
        
    case 0:
        break;

    default:
        /* parent terminates */
        _exit(0);
    }
    
    /* start new session */
    sid = setsid();
    if (sid < 0) {
        log_stderr("setuid() failed, %s", strerror(errno));
        exit(1);
    }
    
     sa.sa_handler = SIG_IGN;
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = 0;
     if (sigaction(SIGHUP, &sa, NULL) < 0) {
        log_stderr("sigaction() failed, can't ignore SIGHUOP\n");
        exit(1);
     }


    pid = fork();
    switch (pid) {
    case -1:
        log_stderr("the second fork() failed: %s", strerror(errno));
        exit(1);
        
    case 0:
        break;

    default:
        /* parent terminates */
        exit(0);
    }
    
    if (chdir("/") < 0) {
        log_stderr("chdir(/) failed: %s", strerror(errno));
    }

    /* clear file mode creation mask */
    umask(0);


    /* close stdin, stdout, stderr */
    fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        log_stderr("open(\"/dev/null\") failed: %s", strerror(errno));  
        exit(1);
    }

    if (dup2(fd, STDIN_FILENO) < 0) {
        log_stderr("dup2(%d, STDIN) failed: %s", fd, strerror(errno));
        close(fd);
        exit(1);
    }

    if (dup2(fd, STDOUT_FILENO) < 0) {
        log_stderr("dup2(%d, STDOUT) failed: %s", fd, strerror(errno));
        close(fd);
        exit(1);
    }

    if (dup2(fd, STDERR_FILENO) < 0) {
        log_stderr("dup2(%d, STDERR) failed: %s", fd, strerror(errno));
        close(fd);
        exit(1);
    }

    /* close  /dev/null , in fact is close stdin, stdout, stderr */
    if (fd > STDERR_FILENO) {
        close(fd);
    }
}

static rps_status_t
rps_set_log(struct application *app) {
    log_level level;

    level = log_level_to_int((char *)app->cfg.log.level.data);
    if (level == LOG_LEVEL_UNDEFINED) {
        log_stderr("invalid log level: %s", app->cfg.log.level.data);
        return RPS_ERROR;
    }
    app->log_level = MAX(app->log_level, level);   

    app->log_filename = strdup((char *)app->cfg.log.file.data);
    if (app->log_filename == NULL) {
        return RPS_ERROR;
    }

    log_deinit();

    if (!app->daemon) {
        return log_init(app->log_level, NULL);
    } else {
        return log_init(app->log_level, app->log_filename);
    }
}

static rps_status_t
rps_server_load(struct application *app) {
    uint32_t i, n;
    rps_status_t status;
    struct config_server *cfg;
    struct server *s;

	array_null(&app->servers);

    n = array_n(app->cfg.servers.ss);

    status = array_init(&app->servers, n , sizeof(struct server));   
    if (status != RPS_OK) {
        return status;
    }
    
    for (i = 0; i < n; i++) {
        cfg = (struct config_server *)array_get(app->cfg.servers.ss, i);
        s = (struct server *)array_push(&app->servers);
        if (s == NULL) {
            goto error;
        }
        
        status = server_init(s, cfg, &app->upstreams, 
                app->cfg.servers.rtimeout, app->cfg.servers.ftimeout);
        if (status != RPS_OK) {
            goto error;
        }

    }
    
    return RPS_OK;

error:
    while (array_n(&app->servers)) {
        server_deinit((struct server *)array_pop(&app->servers));
    }
    array_deinit(&app->servers);

    return RPS_ERROR;
}


static void
rps_upstream_load(struct application *app) {
    uv_loop_t *loop;
    uv_timer_t *timer;

    loop = rps_alloc(sizeof(uv_loop_t));

    ASSERT(loop != NULL);
    
    uv_loop_init(loop);

    timer = (uv_timer_t *)rps_alloc(sizeof(*timer));
    
    timer->data = &app->upstreams;

    uv_timer_init(loop, timer);
    uv_timer_start(timer, (uv_timer_cb)upstreams_refresh, 0, app->cfg.upstreams.refresh);

    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);
    uv_loop_delete(loop);
    rps_free(loop);
    rps_free(timer);
}

static void
rps_teardown(struct application *app) {
    while (array_n(&app->servers)) {
        server_deinit((struct server *)array_pop(&app->servers));
    }
    array_deinit(&app->servers);

	upstreams_deinit(&app->upstreams);
    config_deinit(&app->cfg);
	log_deinit();
}

static rps_status_t
rps_create_pidfile(struct application *app) {
    char pid[RPS_PID_MAX_LENGTH];
    int fd, len;

    fd = open(app->pid_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        log_error("open pidfile %s failed: %s", app->pid_filename, strerror(errno));
        return RPS_ERROR;
    }

    len = snprintf(pid, RPS_PID_MAX_LENGTH, "%d", app->pid);

    if (write(fd, pid, len) < 0) {
        log_error("write to pidfile %s failed, %s", app->pid_filename, strerror(errno));
        close(fd);
        return RPS_ERROR;
    }
    
    close(fd);

    log_debug("write pid(%d) to pidfile '%s'", app->pid, app->pid_filename);

    return RPS_OK;
}

static void
rps_remove_pidfile(struct application *app) {
    if (unlink(app->pid_filename) < 0) {
        log_error("unlink pidfile '%s' failed: %s", app->pid_filename, strerror(errno));
    }

}

static rps_status_t
rps_pre_run(struct application *app) {
    
    if (app->daemon) {
        rps_daemonize();
    }

    app->pid = getpid();

    signal_init();

    if (app->pid_filename != NULL) {
        rps_create_pidfile(app);
    }

    return RPS_OK;
}

static void
rps_run(struct application *app) {
    uint32_t i, n;
    struct server *s;
    rps_status_t status;
    uv_thread_t *tid;
    rps_array_t threads;

    upstreams_init(&app->upstreams, &app->cfg.api, &app->cfg.upstreams);

    status = rps_server_load(app);
    if (status != RPS_OK) {
        return;
    }

    n = array_n(&app->servers) + 1; // Add 1 upstream auto-refresh thread
    
    status = array_init(&threads, n , sizeof(uv_thread_t));   
    if (status != RPS_OK) {
        return;
    }

    tid = (uv_thread_t *)array_push(&threads);
    uv_thread_create(tid, (uv_thread_cb)rps_upstream_load, app);
    
    for (i = 0; i < array_n(&app->servers); i++) {
        tid = (uv_thread_t *)array_push(&threads);
        s = (struct server *)array_get(&app->servers, i);
        uv_thread_create(tid, (uv_thread_cb)server_run, s);
    }

    while(array_n(&threads)) {
        uv_thread_join((uv_thread_t *)array_pop(&threads));
    }   

    array_deinit(&threads);

    rps_teardown(app);
}

static void
rps_post_run(struct application *app) {
    if (app->pid_filename != NULL) {
        rps_remove_pidfile(app);
    }
}

int
main(int argc, char **argv) {
    struct application app;
    rps_status_t status;

    rps_init(&app);

    status = rps_get_options(argc, argv, &app);
    if (status != RPS_OK) {
        rps_show_usage();
    }

    status = config_init(app.config_filename, &app.cfg);
    if (status != RPS_OK) {
        exit(1);
    }

    app.daemon = app.daemon ||  app.cfg.daemon;

    if (!string_empty(&app.cfg.pidfile)) {
        app.pid_filename = strndup((const char *)app.cfg.pidfile.data, app.cfg.pidfile.len);
    }

    status = rps_set_log(&app);
    if (status != RPS_OK) {
        exit(1);
    }

    config_dump(&app.cfg);

    status = rps_pre_run(&app);
    if (status != RPS_OK) {
        rps_post_run(&app);
        exit(1);
    }

    rps_run(&app);

    rps_post_run(&app);

    exit(1);
}

