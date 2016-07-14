#include "config.h"

#include <yaml.h>

struct config *
config_create(const char *config_file) {
    struct config *conf;

    conf = malloc(sizeof(*conf));
    if (conf == NULL) {
        return NULL;
    }
    return conf;

};
