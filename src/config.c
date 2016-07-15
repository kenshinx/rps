#include "config.h"
#include "util.h"
#include "log.h"
#include "array.h"

#include <yaml.h>

#include <errno.h>
#include <string.h>


#define CONFIG_SERVERS_NUM  3

static struct config *
config_open(char *filename) {
    struct config *cfg;
    FILE *fd;
    rpg_array_t *array;

    fd = fopen(filename, "r");
    if (fd == NULL) {
        log_stderr("config: failed to open configuration: '%s': '%s'", filename, strerror(errno));
        return NULL;
    }

    cfg= rpg_alloc(sizeof(*cfg));
    if (cfg == NULL) {
        fclose(fd);
        return NULL;
    }
    
    cfg->servers = array_create(CONFIG_SERVERS_NUM, sizeof(struct config_server));
    if (cfg->servers == NULL) {
        rpg_free(cfg);
        fclose(fd);
        return NULL;
    }

    cfg->fname = filename;
    cfg->fd = fd;
    return cfg;
}

static rpg_status_t
config_parse(struct config *cfg){
    rpg_status_t status;
    
    return RPG_OK;
}

struct config *
config_create(char *filename) {
    struct config *cfg;
    
    cfg = config_open(filename);
    
    return cfg;
}
