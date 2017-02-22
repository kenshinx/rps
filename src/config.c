#include "config.h"
#include "core.h"
#include "util.h"
#include "log.h"
#include "array.h"
#include "_string.h"

#include <yaml.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>


#define CONFIG_SERVERS_NUM  3
#define CONFIG_DEFAULT_ARGS  3

#define CONFIG_ROOT_PATH    1
#define CONFIG_MAX_PATH     CONFIG_ROOT_PATH + 1


static  rps_status_t
config_event_next(struct config *cfg) {
    int rv;
    
    rv = yaml_parser_parse(&cfg->parser, &cfg->event);
    if (!rv) {
        log_stderr("config: failed (err %d) to get next event");
        return RPS_ERROR;
    }   

    return RPS_OK;
}

static void
config_event_done(struct config *cfg) {
    yaml_event_delete(&cfg->event);
}

static rps_status_t
config_push_scalar(struct config *cfg) {
    rps_status_t status;
    rps_str_t   *value;

    char *scalar;
    size_t length;

    scalar = (char *)cfg->event.data.scalar.value;
    length = cfg->event.data.scalar.length;
    if (length == 0) {
        return RPS_ERROR;
    }

    log_verb("push '%.*s'", length, scalar);

    value = array_push(cfg->args);
    if (value == NULL) {
        return RPS_ENOMEM;
    }
    string_init(value);

    status = string_duplicate(value, scalar, length);
    if (status != RPS_OK) {
        array_pop(cfg->args);
        return status;
    }

    return RPS_OK;   
}

static rps_str_t *
config_pop_scalar(struct config *cfg) {
    rps_str_t *value;

    value = (rps_str_t *)array_pop(cfg->args);
    log_verb("pop '%.*s'", value->len, value->data);
    return value;
}

static void
config_log_init(struct config_log *log) {
    string_init(&log->file);
    string_init(&log->level);
}

static void
config_log_deinit(struct config_log *log) {
    string_deinit(&log->file);
    string_deinit(&log->level);
}

static void
config_server_init(struct config_server *server) {
    string_init(&server->proxy);
    string_init(&server->listen);
    server->port = 0;
    server->timeout = 0;
    string_init(&server->username);
    string_init(&server->password);
}

static void
config_server_deinit(struct config_server *server) {
    string_deinit(&server->proxy);
    string_deinit(&server->listen);
    string_deinit(&server->username);
    string_deinit(&server->password);
}

static rps_status_t
config_set_daemon(struct config *cfg, rps_str_t *str) {
    if (rps_strcmp(str->data, "true") == 0 ) {
        cfg->daemon = 1;
    } else if (rps_strcmp(str->data, "false") == 0 ) {
        cfg->daemon = 0;
    } else {
        return RPS_ERROR;
    }
    
    return RPS_OK;
}

static rps_status_t
config_handler_map(struct config *cfg, rps_str_t *key, rps_str_t *val, rps_str_t *section) {
    rps_status_t status;
    struct config_server *server;

    status = RPS_OK;

    if (section == NULL) {
        if (rps_strcmp(key->data, "title") == 0 ) {
            status = string_copy(&cfg->title, val);          
        } else if (rps_strcmp(key->data, "pidfile") == 0) {
            status = string_copy(&cfg->pidfile, val);
        } else if (rps_strcmp(key->data, "daemon") == 0) {
            status = config_set_daemon(cfg, val);
        } else {
            status = RPS_ERROR;
        }
    } else if (rps_strcmp(section->data, "servers") == 0 ) {
        server = (struct config_server *)array_head(cfg->servers);
        if (rps_strcmp(key->data, "proxy") == 0) {
            status = string_copy(&server->proxy, val);
        } else if (rps_strcmp(key->data, "listen") == 0) {
            status = string_copy(&server->listen, val);
        } else if (rps_strcmp(key->data, "port") == 0) {
            server->port = atoi((char *)val->data);
        } else if (rps_strcmp(key->data, "username") == 0) {
            status = string_copy(&server->username, val);
        } else if (rps_strcmp(key->data, "password") == 0) {
            status = string_copy(&server->password, val);
        } else if (rps_strcmp(key->data, "timeout") == 0) { 
            /*convert uint from second to millsecond*/
            server->timeout = (atoi((char *)val->data)) * 1000;
        }else {
            status = RPS_ERROR;
        }
    } else if (rps_strcmp(section->data, "log") == 0) {
        if(rps_strcmp(key->data, "file") == 0) {
            status = string_copy(&cfg->log->file, val);
        } else if (rps_strcmp(key->data, "level") == 0) {
            status = string_copy(&cfg->log->level, val);
        } else {
            status = RPS_ERROR;
        }
    } else {
        status = RPS_ERROR;
    }

    return status;
}


static rps_status_t
config_handler(struct config *cfg, rps_str_t *section) {
    rps_status_t status;
    rps_str_t *key, *val;

    ASSERT(array_n(cfg->args) == 2);

    val = config_pop_scalar(cfg);
    key = config_pop_scalar(cfg);

    status = config_handler_map(cfg, key, val, section);
    
    string_deinit(val);
    string_deinit(key);
    return status;
}

static rps_status_t
config_load(char *filename, struct config *cfg) {
    FILE *fd;

    fd = fopen(filename, "r");
    if (fd == NULL) {
        log_stderr("config: failed to open configuration: '%s': '%s'", filename, strerror(errno));
        return RPS_ERROR;
    }
    
    cfg->servers = array_create(CONFIG_SERVERS_NUM, sizeof(struct config_server));
    if (cfg->servers == NULL) {
        goto error;
    }

    cfg->args = array_create(CONFIG_DEFAULT_ARGS, sizeof(rps_str_t));
    if (cfg->args == NULL) {
        goto error;
    }

    cfg->log = rps_alloc(sizeof(struct config_log));
    if (cfg->log == NULL) {
        goto error;
    }
    config_log_init(cfg->log);

    cfg->fname = filename;
    cfg->fd = fd;
    cfg->depth = 0;
    cfg->seq = 0;
    cfg->daemon= 0;
    string_init(&cfg->title);
    string_init(&cfg->pidfile);
    return RPS_OK;

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
        rps_free(cfg->log);
    }

    return RPS_ERROR;
}

static rps_status_t
config_yaml_init(struct config *cfg) {
    int rv;
    
    rv = fseek(cfg->fd, 0L, SEEK_SET);
    if (rv < 0) {
        log_stderr("config: failed to seek to the beginning of file '%s': %s",
                cfg->fname, strerror(errno));
        return RPS_ERROR;
    }

    rv = yaml_parser_initialize(&cfg->parser);
    if (!rv) {
        log_stderr("config: failed (err %d) to initialize yaml parser",
                cfg->parser.error);
        return RPS_ERROR;
    }
    
    yaml_parser_set_input_file(&cfg->parser, cfg->fd);

    return RPS_OK;
}

void
config_yaml_deinit(struct config *cfg) {
     yaml_parser_delete(&cfg->parser);
}

static rps_status_t
config_begin_parse(struct config *cfg) {
    rps_status_t status;
    bool done;
    
    status = config_yaml_init(cfg);
    if (status != RPS_OK) {
        return status;
    }

    done = false;
    while (!done) {
        status = config_event_next(cfg);
        if (status != RPS_OK) {
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

    return RPS_OK;
}

static rps_status_t
config_parse_core(struct config *cfg, rps_str_t *section) {
    rps_status_t status;
    rps_str_t *node;
    struct config_server *server;
    bool done, leaf;

    status = config_event_next(cfg);
    if(status != RPS_OK) {
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
            node = config_pop_scalar(cfg);
            status = string_copy(section, node);
            string_deinit(node);
            if (status != RPS_OK) {
                break;
            }
        } 

        if (cfg->seq) {
            if (rps_strcmp(section->data, "servers") == 0 ) {
                /* new server block */
                server = (struct config_server *)array_push(cfg->servers);
                if (server == NULL) {
                    status = RPS_ENOMEM;
                }
                config_server_init(server);
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
        
        if (status != RPS_OK) {
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
    
    if (status != RPS_OK) {
        return status;
    }

    if (done) {
        return RPS_OK;
    }

    if (leaf) {
        status = config_handler(cfg, section);
    }

    return config_parse_core(cfg, section);
}

static rps_status_t
config_end_parse(struct config *cfg) {
    rps_status_t status;
    bool done;

    ASSERT(cfg->depth == 0);
    
    done = false;
    while(!done) {
        status = config_event_next(cfg);
        if (status != RPS_OK) {
            return status;
        }

        log_verb("next end event %d", cfg->event.type);
        
        switch (cfg->event.type) {
        case YAML_STREAM_END_EVENT:
            done = true;
            break;
        
        case YAML_DOCUMENT_END_EVENT:
            break;
        default:
            NOT_REACHED();

        config_event_done(cfg);
        }
    }

    config_yaml_deinit(cfg);

    return RPS_OK;
}

static rps_status_t
config_parse(struct config *cfg){
    rps_status_t status;
    
    ASSERT(array_n(cfg->servers) == 0);   

    status = config_begin_parse(cfg);
    if (status != RPS_OK) {
        return status;
    }


    status = config_parse_core(cfg, NULL);
    if (status != RPS_OK) {
        return status;
    }
    
    status = config_end_parse(cfg);
    if (status != RPS_OK) {
        return status;
    }
    
    return RPS_OK;
}

static void
config_dump_server(void *data) {
    struct config_server *server = data;    

    log_debug("\t - proxy: %s", server->proxy.data);
    log_debug("\t listen: %s", server->listen.data);
    log_debug("\t port: %d", server->port);
    log_debug("\t username: %s", server->username.data);
    log_debug("\t password: %s", server->password.data);
    log_debug("\t timeout: %d (millseconds)", server->timeout);
    log_debug("");
}

void
config_dump(struct config *cfg) {
    log_debug("[%s Configuration]", cfg->title.data);
    log_debug("pidfile: %s", cfg->pidfile.data);
    log_debug("demon: %d", cfg->daemon);

    log_debug("[servers]");
    array_foreach(cfg->servers, config_dump_server);
    
    log_debug("[log]");
    log_debug("\t file: %s", cfg->log->file.data);
    log_debug("\t level: %s", cfg->log->level.data);
   
}

rps_status_t
config_init(char *filename, struct config *cfg) {
    rps_status_t status;
    
    status = config_load(filename, cfg);
    if (status != RPS_OK) {
        return status;
    }

    status = config_parse(cfg);
    if (status != RPS_OK) {
        log_stderr("config: configuration file '%s' syntax is invalid", filename);
        fclose(cfg->fd);
        cfg->fd = NULL;
        config_deinit(cfg);
        return status;
    }

    fclose(cfg->fd);
    cfg->fd = NULL;
    
    return RPS_OK;
}

void
config_deinit(struct config *cfg) {

    string_deinit(&cfg->title);
    string_deinit(&cfg->pidfile);

    while (array_n(cfg->args)) {
        rps_str_t *arg = config_pop_scalar(cfg);
        string_deinit(arg);
    }
    array_destroy(cfg->args);

    while (array_n(cfg->servers)) {
        config_server_deinit((struct config_server *)array_pop(cfg->servers));
    }
    array_destroy(cfg->servers);

    config_log_deinit(cfg->log);
    rps_free(cfg->log);
}
