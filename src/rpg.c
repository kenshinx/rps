#include "log.h" 
#include "config.h"
#include "util.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>


#ifdef RPG_DEBUG_OPEN
#define RPG_DEFAULT_LOG_LEVEL       LOG_VERBOSE
#else
#define RPG_DEFAULT_LOG_LEVEL       LOG_NOTICE
#endif
#define RPG_DEFAULT_LOG_FILE        NULL
#define RPG_DEFAULT_CONFIG_FILE     "../conf/rpg.yml"
#define RPG_DEFAULT_PID_FILE        NULL


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
rpg_show_usage() {
    log_stderr(
        "Usage: ./rpg -c conf/rpg.yaml" CRLF
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
        RPG_DEFAULT_CONFIG_FILE, 
        RPG_DEFAULT_PID_FILE == NULL ? "off" : RPG_DEFAULT_PID_FILE
    );
    exit(1);
}

static void
rpg_show_version() {
    log_stdout("rpg %s", RPG_VERSION);
    exit(0);
} 

static void
rpg_set_default_options(struct application *app) {
    app->log_level = RPG_DEFAULT_LOG_LEVEL;
    app->log_filename = RPG_DEFAULT_LOG_FILE;
    
    app->pid = (pid_t)-1;
    
    app->config_filename = RPG_DEFAULT_CONFIG_FILE;
    app->cfg = NULL;
}

static rpg_status_t
rpg_get_options(int argc, char **argv, struct application *app) {

    char c;

    for (;;) {
        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) {
            /* no more options */
            break;
        }

        switch (c) {
            case 'h':
                rpg_show_usage();
            case 'V':
                rpg_show_version();
            case 'd':
                app->daemon = 1;
                break;
            case 'v':
                app->verbose = 1;
                app->log_level = MAX(LOG_DEBUG, RPG_DEFAULT_LOG_LEVEL);
                break;
            case 'c':
                app->config_filename = optarg;
                break;
            case 'p':
                app->pid_filename = optarg;
                break;
            case '?':
                /* getopt_long already printed an error message. */
                return RPG_ERROR;
            default:
                return RPG_ERROR;
        }
        
    }
    return RPG_OK;
}

static rpg_status_t 
rpg_load_config(struct application *app) {
    struct config *cfg;

    cfg = config_create(app->config_filename);
    if (cfg == NULL) {
        return RPG_ERROR;
    }

    config_dump(cfg);

    app->cfg = cfg;

    return RPG_OK;
}


int
main(int argc, char **argv) {
    struct application app;
    rpg_status_t status;

    rpg_set_default_options(&app);

    status = rpg_get_options(argc, argv, &app);
    if (status != RPG_OK) {
        rpg_show_usage();
    }

    log_init(app.log_level, app.log_filename);

    status = rpg_load_config(&app);
    if (status != RPG_OK) {
        exit(1);
    }


    exit(1);
}

