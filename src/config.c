#include "config.h"
#include "util.h"
#include "log.h"
#include "array.h"
#include "string.h"

#include <yaml.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>


#define CONFIG_SERVERS_NUM  3
#define CONFIG_DEFAULT_ARGS  3

#define CONFIG_ROOT_PATH    1
#define CONFIG_MAX_PATH     CONFIG_ROOT_PATH + 1


static  rpg_status_t
config_event_next(struct config *cfg) {
    int rv;
    
    rv = yaml_parser_parse(&cfg->parser, &cfg->event);
    if (!rv) {
        log_stderr("config: failed (err %d) to get next event");
        return RPG_ERROR;
    }   

    return RPG_OK;
}

static void
config_event_done(struct config *cfg) {
    yaml_event_delete(&cfg->event);
}

static rpg_status_t
config_push_scalar(struct config *cfg) {
    rpg_status_t status;
    rpg_str_t   *value;

    char *scalar;
    size_t length;

    scalar = (char *)cfg->event.data.scalar.value;
    length = cfg->event.data.scalar.length;
    if (length == 0) {
        return RPG_ERROR;
    }

    log_verb("push '%.*s'", length, scalar);

    value = array_push(cfg->args);
    if (value == NULL) {
        return RPG_ENOMEM;
    }
    string_init(value);

    status = string_dup(value, scalar, length);
    if (status != RPG_OK) {
        array_pop(cfg->args);
        return status;
    }

    return RPG_OK;   
}

static rpg_str_t *
config_pop_scalar(struct config *cfg) {
    rpg_str_t *value;

    value = (rpg_str_t *)array_pop(cfg->args);
    log_verb("pop '%.*s'", value->len, value->data);
    return value;
}

static rpg_status_t
config_set_daemon(struct config *cfg, rpg_str_t *str) {
    if (rpg_strcmp(str->data, "true") == 0 ) {
        cfg->daemon = 1;
    } else if (rpg_strcmp(str->data, "false") == 0 ) {
        cfg->daemon = 0;
    } else {
        return RPG_ERROR;
    }
    
    return RPG_OK;
}

static rpg_status_t
config_handler_map(struct config *cfg, rpg_str_t *key, rpg_str_t *val, rpg_str_t *section) {
    rpg_status_t status;
    struct config_server *server;

    if (section == NULL) {
        if (rpg_strcmp(key->data, "title") == 0 ) {
            status = string_cpy(&cfg->title, val);          
        } else if(rpg_strcmp(key->data, "pidfile") == 0) {
            status = string_cpy(&cfg->pidfile, val);
        } else if(rpg_strcmp(key->data, "daemon") == 0) {
            status = config_set_daemon(cfg, val);
        } else {
            status = RPG_ERROR;
        }
    } else if (rpg_strcmp(section->data, "servers") == 0 ) {
        server = (struct config_server *)array_head(cfg->servers);
        
        status = RPG_OK;
    } else {
        status = RPG_ERROR;
    }

    return status;
}


static rpg_status_t
config_handler(struct config *cfg, rpg_str_t *section) {
    rpg_status_t status;
    rpg_str_t *key, *val;

    val = config_pop_scalar(cfg);
    key = config_pop_scalar(cfg);

    status = config_handler_map(cfg, key, val, section);
    
    string_deinit(val);
    string_deinit(key);
    return status;
}

static struct config *
config_open(char *filename) {
    struct config *cfg;
    FILE *fd;

    fd = fopen(filename, "r");
    if (fd == NULL) {
        log_stderr("config: failed to open configuration: '%s': '%s'", filename, strerror(errno));
        return NULL;
    }

    cfg= rpg_alloc(sizeof(*cfg));
    if (cfg == NULL) {
        goto error;
    }
    
    cfg->servers = array_create(CONFIG_SERVERS_NUM, sizeof(struct config_server));
    if (cfg->servers == NULL) {
        goto error;
    }

    cfg->args = array_create(CONFIG_DEFAULT_ARGS, sizeof(rpg_str_t));
    if (cfg->args == NULL) {
        goto error;
    }

    cfg->log = rpg_alloc(sizeof(struct config_log));
    if (cfg->log == NULL) {
        goto error;
    }

    cfg->fname = filename;
    cfg->fd = fd;
    cfg->depth = 0;
    cfg->seq = 0;
    return cfg;

error:
    log_stderr("config: initial configuration failed.");

    fclose(fd);

    if (cfg->servers != NULL) {
        array_destroy(cfg->servers);
    }
    if (cfg->args !=  NULL) {
        array_destroy(cfg->args);
    }
    if (cfg->log != NULL) {
        rpg_free(cfg->log);
    }
    
    rpg_free(cfg);

    return NULL;
}

static rpg_status_t
config_yaml_init(struct config *cfg) {
    int rv;
    
    rv = fseek(cfg->fd, 0L, SEEK_SET);
    if (rv < 0) {
        log_stderr("config: failed to seek to the beginning of file '%s': %s",
                cfg->fname, strerror(errno));
        return RPG_ERROR;
    }

    rv = yaml_parser_initialize(&cfg->parser);
    if (!rv) {
        log_stderr("config: failed (err %d) to initialize yaml parser",
                cfg->parser.error);
        return RPG_ERROR;
    }
    
    yaml_parser_set_input_file(&cfg->parser, cfg->fd);

    return RPG_OK;
}

static rpg_status_t
config_begin_parse(struct config *cfg) {
    rpg_status_t status;
    bool done;
    
    status = config_yaml_init(cfg);
    if (status != RPG_OK) {
        return status;
    }

    done = false;
    while (!done) {
        status = config_event_next(cfg);
        if (status != RPG_OK) {
            return status;
        }
        
        log_verb("next begin event %d", cfg->event.type);

        switch (cfg->event.type) {
        case YAML_STREAM_START_EVENT:
        case YAML_DOCUMENT_START_EVENT:
            break;
        case YAML_MAPPING_START_EVENT:
            cfg->depth++;
            done = true;
            break;
        default:
            NOT_REACHED();
        }

        config_event_done(cfg);
    }

    return RPG_OK;
}

static rpg_status_t
config_parse_core(struct config *cfg, rpg_str_t *section) {
    rpg_status_t status;
    struct config_server *server;
    bool done, leaf;

    status = config_event_next(cfg);
    if(status != RPG_OK) {
        return status;
    }

    log_verb("next event %d depth %d seq %d args.length %d", cfg->event.type,cfg->depth, cfg->seq, array_n(cfg->args));

    done = false;
    leaf = false;

    switch (cfg->event.type) {
    case YAML_MAPPING_START_EVENT:
        cfg->depth++;

        if (cfg->depth == CONFIG_MAX_PATH && array_n(cfg->args)) {
            ASSERT(array_n(cfg->args) == 1);   
            section = string_new();
            status = string_cpy(section, config_pop_scalar(cfg));
            if (status != RPG_OK) {
                break;
            }
        } 

        if (cfg->seq) {
            if (rpg_strcmp(section->data, "servers") == 0 ) {
                /* new server block */
                server = (struct config_server *)array_push(cfg->servers);
                if (server == NULL) {
                    status = RPG_ENOMEM;
                }
            }
        }
        break;

    case YAML_MAPPING_END_EVENT:
        cfg->depth--;
        if (!cfg->seq) {
            if(section != NULL) {
                string_free(section);
                section = NULL;
            }
        }
        if (cfg->depth == 0) {
            done = true;
        }
        break;

    case YAML_SEQUENCE_START_EVENT:
        cfg->seq = 1;
        break;

    case YAML_SEQUENCE_END_EVENT:
        cfg->seq = 0;
        ASSERT(section != NULL);
        string_free(section);
        break;

    case YAML_SCALAR_EVENT:
        status = config_push_scalar(cfg);
        
        if (status != RPG_OK) {
            break;
        }

        if (array_n(cfg->args) == CONFIG_MAX_PATH) {
            leaf = true;
        }
        break;

    default:
        NOT_REACHED();
    }

    config_event_done(cfg);
    
    if (status != RPG_OK) {
        return status;
    }

    if (done) {
        return RPG_OK;
    }

    if (leaf) {
        status = config_handler(cfg, section);
    }

    return config_parse_core(cfg, section);
    
}

static rpg_status_t
config_parse(struct config *cfg){
    rpg_status_t status;
    
    ASSERT(array_n(cfg->servers) == 0);   

    status = config_begin_parse(cfg);
    if (status != RPG_OK) {
        return status;
    }


    status = config_parse_core(cfg, NULL);
    if (status != RPG_OK) {
        return status;
    }
    
    return RPG_OK;
}

static void
config_destroy_log(struct config_log *log) {
    string_deinit(&log->file);
    string_deinit(&log->level);
    rpg_free(log);
}

void
config_dump(struct config *cfg) {
    log_notice("[%s Configuration]", cfg->title.data);
    log_notice("pidfile: %s", cfg->pidfile.data);
    log_notice("demon: %d", cfg->daemon);

    log_notice("servers len: %d", array_n(cfg->servers));
}

struct config *
config_create(char *filename) {
    struct config *cfg;
    rpg_status_t status;
    
    cfg = config_open(filename);
    if (cfg == NULL) {
        return NULL;
    }

    status = config_parse(cfg);
    if (status != RPG_OK) {
        log_stderr("config: configuration file '%s' syntax is invalid", filename);
        fclose(cfg->fd);
        cfg->fd = NULL;
        config_destroy(cfg);
        return NULL;
    }

    config_dump(cfg);
    

    rpg_str_t str2 = rpg_string("Hello Kenshin");
    printf("%s len:%zu\n", str2.data, str2.len);

    fclose(cfg->fd);
    cfg->fd = NULL;
    
    return cfg;
}

void
config_destroy(struct config *cfg) {
    ASSERT(array_n(cfg->args) == 0);   

    string_deinit(&cfg->title);
    string_deinit(&cfg->pidfile);

    array_destroy(cfg->args);
    array_destroy(cfg->servers);

    config_destroy_log(cfg->log);

    rpg_free(cfg);
}
