#include "config.h"
#include "util.h"

#include <yaml.h>

struct config *
config_create(const char *config_file) {
    struct config *conf;

    conf = rpg_alloc(sizeof(*conf));
    if (conf == NULL) {
        return NULL;
    }

    

    return conf;
}
