#include "rps.h"
#include "core.h"
#include "log.h" 
#include "config.h"
#include "util.h"
#include "server.h"

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
        RPS_DEFAULT_CONFIG_FILE, 
        RPS_DEFAULT_PID_FILE == NULL ? "off" : RPS_DEFAULT_PID_FILE
    );
    exit(1);
}

static void
rps_show_version() {
    log_stdout("rps %s", RPS_VERSION);
    exit(0);
} 

static void
rps_set_default_options(struct application *app) {
    app->log_level = RPS_DEFAULT_LOG_LEVEL;
    app->log_filename = RPS_DEFAULT_LOG_FILE;
    
    app->pid = (pid_t)-1;
    
    app->config_filename = RPS_DEFAULT_CONFIG_FILE;
    app->cfg = NULL;
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
            case 'p':
                app->pid_filename = optarg;
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

static rps_status_t 
rps_load_config(struct application *app) {
    struct config *cfg;

    cfg = config_create(app->config_filename);
    if (cfg == NULL) {
        return RPS_ERROR;
    }

    app->cfg = cfg;

    return RPS_OK;
}

static rps_status_t
rps_set_log(struct application *app) {
    log_level level;

    level = log_level_to_int((char *)app->cfg->log->level.data);
    if (level == LOG_LEVEL_UNDEFINED) {
        log_stderr("invalid log level: %s", app->cfg->log->level.data);
        return RPS_ERROR;
    }
    app->log_level = MAX(app->log_level, level);   

    app->log_filename = strdup((char *)app->cfg->log->file.data);
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
rps_server_setup(struct application *app) {
    uint32_t i, n;
    rps_status_t status;
    struct config_server *cfg;
    struct server *s;

    n = array_n(app->cfg->servers);

    status = array_init(&app->servers, n , sizeof(struct server));   
    if (status != RPS_OK) {
        return status;
    }
    
    for (i = 0; i < n; i++) {
        cfg = (struct config_server *)array_get(app->cfg->servers, i);
        s = (struct server *)array_push(&app->servers);
        if (s == NULL) {
            goto error;
        }
        
        status = server_init(s, cfg);
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
rps_teardown(struct application *app) {
    while (array_n(&app->servers)) {
        server_deinit((struct server *)array_pop(&app->servers));
    }
    array_deinit(&app->servers);
}


static rps_status_t
rps_pre_run(struct application *app) {

    return RPS_OK;
}

static void
rps_run(struct application *app) {
    uint32_t i, n;
    struct server *s;
    rps_status_t status;
    uv_thread_t *tid;
    rps_array_t threads;

    status = rps_server_setup(app);
    if (status != RPS_OK) {
        return;
    }

    /*
     * rps_upstream_setup();
     */

    n = array_n(&app->servers);
    
    status = array_init(&threads, n , sizeof(uv_thread_t));   
    if (status != RPS_OK) {
        return;
    }
    
    for (i = 0; i < n; i++) {
        tid = (uv_thread_t *)array_push(&threads);
        s = (struct server *)array_get(&app->servers, i);
        uv_thread_create(tid, server_run, s);
    }

    while(array_n(&threads)) {
        uv_thread_join((uv_thread_t *)array_pop(&threads));
    }   

    array_deinit(&threads);

    rps_teardown(app);
}

static void
rps_post_run(struct application *app) {
    /*
     * remove pidfile, signal_deinit
     */
    log_deinit();
    config_destroy(app->cfg);
    
}

int
main(int argc, char **argv) {
    struct application app;
    rps_status_t status;

    log_init(RPS_DEFAULT_LOG_LEVEL, RPS_DEFAULT_LOG_FILE);

    rps_set_default_options(&app);

    status = rps_get_options(argc, argv, &app);
    if (status != RPS_OK) {
        rps_show_usage();
    }

    status = rps_load_config(&app);
    if (status != RPS_OK) {
        exit(1);
    }

    status = rps_set_log(&app);
    if (status != RPS_OK) {
        exit(1);
    }

    config_dump(app.cfg);

    app.daemon = app.daemon ||  app.cfg->daemon;

    status = rps_pre_run(&app);
    if (status != RPS_OK) {
        rps_post_run(&app);
        exit(1);
    }

    rps_run(&app);

    rps_post_run(&app);

    exit(1);
}

