#include "log.h" 

#include <stdio.h>
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

static char short_options[] = "hVdv:c:p:";

static rpg_status_t
rpg_get_options(int argc, char **argv, struct application *app) {

    int c;


    for (;;) {
        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) {
            /* no more options */
            break;
        }
        printf("c is %s\n", c);
    }
    return RPG_OK;
}


int
main(int argc, char **argv) {
    struct application app;
    rpg_status_t stat;
    stat = rpg_get_options(argc, argv, &app);

    exit(1);
}

