#include "log.h" 
#include "config.h"
#include "util.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>



static struct option long_options[] = {
    { "help",        no_argument,        NULL,   'h' },
    { "version",     no_argument,        NULL,   'V' },
    { "daemonize",   no_argument,        NULL,   'd' },
    { "verbose",     no_argument,        NULL,   'v' },
    { "conf-file",   required_argument,  NULL,   'c' },
    { "pid-file",    required_argument,  NULL,   'p' },
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
        "   -p, --pidfile=S      :set pid file (default: %s)" CRLF
        "",
        CONFIG_DEFAULT_FILE, 
        CONFIG_DEFAULT_PIDFILE == NULL ? "off" : CONFIG_DEFAULT_PIDFILE
    );
    exit(1);
}

static void
rpg_show_version() {
    log_stdout("rpg %s", RPG_VERSION);
    exit(0);
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
                app->log_level = LOG_DEBUG;
                break;
            case 'c':
                app->config_file = optarg;
                break;
            case 'p':
                app->pid_file = optarg;
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
    struct config *conf;

    conf = config_create(app->config_file);
    if (conf == NULL) {
        log_stderr("configure file '%s' syntax is invalid", app->config_file);
        return RPG_ERROR;
    }
    
    return RPG_OK;
}


int
main(int argc, char **argv) {
    struct application app;
    rpg_status_t status;

    status = rpg_get_options(argc, argv, &app);
    if (status != RPG_OK) {
        rpg_show_usage();
    }

    status = rpg_load_config(&app);
    if (status != RPG_OK) {
        exit(1);
    }
    

    exit(1);
}

