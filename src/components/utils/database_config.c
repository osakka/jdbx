/*
 * database_config.c - Database configuration management
 * 
 * Implements the highest priority configuration tier, allowing
 * runtime configuration changes through the database.
 */

#include "utils/config_loader.h"
#include "utils/buffer_pool.h"
#include "database/database.h"
#include "database/document_storage.h"
#include "utils/logger.h"
#include "utils/json.h"
#include <string.h>
#include <pthread.h>
#include <time.h>

/* Unified Documents Configuration - All documents go to default/documents */
#define CONFIG_DOCUMENT_ID "system_config"
#define CONFIG_DOCUMENT_TYPE DOC_TYPE_NAME_CONFIG
#define CONFIG_LIBRARY VIRTUAL_LIBRARY_SYSTEM
#define CONFIG_COLLECTION VIRTUAL_COLLECTION_CONFIGS

/* Configuration change callback */
typedef struct config_callback {
    void (*callback)(const char* key, json_value_t* old_value, json_value_t* new_value);
    void* user_data;
    struct config_callback* next;
} config_callback_t;

/* Global state */
static pthread_mutex_t config_mutex = PTHREAD_MUTEX_INITIALIZER;
static config_callback_t* callbacks = NULL;
static json_value_t* cached_config = NULL;

/* Register a configuration change callback */
int config_register_callback(void (*callback)(const char*, json_value_t*, json_value_t*), void* user_data) {
    if (!callback) {
        return -1;
    }
    
    config_callback_t* cb = BUFFER_ALLOC(sizeof(config_callback_t));
    if (!cb) {
        LOG_ERROR("Cannot allocate configuration callback.");
        return -1;
    }
    
    cb->callback = callback;
    cb->user_data = user_data;
    
    pthread_mutex_lock(&config_mutex);
    cb->next = callbacks;
    callbacks = cb;
    pthread_mutex_unlock(&config_mutex);
    
    return 0;
}

/* Load configuration from database */
json_value_t* config_load_from_database(database_t* db) {
    if (!db) {
        return NULL;
    }
    
    /* Load configuration document from unified storage */
    json_value_t* config = db_get_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, CONFIG_DOCUMENT_ID);
    if (!config) {
        LOG_DEBUG("Configuration document not found.");
        return NULL;
    }
    
    if (config->type != JSON_OBJECT) {
        LOG_ERROR("Invalid configuration document type.");
        json_free(config);
        return NULL;
    }
    
    /* Extract settings object */
    json_value_t* settings = json_object_get(config, "settings");
    if (!settings || settings->type != JSON_OBJECT) {
        LOG_ERROR("Configuration missing settings object.");
        json_free(config);
        return NULL;
    }
    
    /* Return cloned settings */
    json_value_t* result = json_clone(settings);
    json_free(config);
    
    return result;
}

/* Save configuration to database */
int config_save_to_database(database_t* db, json_value_t* config) {
    if (!db || !config || config->type != JSON_OBJECT) {
        return -1;
    }
    
    /* Create configuration document with unified documents fields */
    json_value_t* doc = json_create_object();
    json_object_set(doc, "uuid", json_create_string(CONFIG_DOCUMENT_ID));
    json_object_set(doc, "type", json_create_string(CONFIG_DOCUMENT_TYPE));
    json_object_set(doc, "library", json_create_string(CONFIG_LIBRARY));
    json_object_set(doc, "collection", json_create_string(CONFIG_COLLECTION));
    json_object_set(doc, "owner", json_create_string(SYSTEM_USER_ADMIN));
    json_object_set(doc, "version", json_create_integer(1));
    json_object_set(doc, "settings", json_clone(config));
    json_object_set(doc, "created_at", json_create_integer(time(NULL)));
    json_object_set(doc, "modified_at", json_create_integer(time(NULL)));
    
    /* Check if document exists */
    json_value_t* existing = db_get_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, CONFIG_DOCUMENT_ID);
    json_value_t* result = NULL;
    
    if (existing) {
        /* Update existing */
        json_free(existing);
        result = storage_update_document(db, CONFIG_DOCUMENT_ID, doc);
    } else {
        /* Insert new */
        result = storage_insert_document(db, doc);
    }
    
    json_free(doc);
    
    if (!result) {
        LOG_ERROR("Cannot save configuration to database.");
        return -1;
    }
    
    json_free(result);
    LOG_INFO("Configuration saved to database.");
    return 0;
}

/* Apply database configuration to server */
int config_apply_database_settings(server_config_t* config, database_t* db) {
    if (!config || !db) {
        return -1;
    }
    
    pthread_mutex_lock(&config_mutex);
    
    /* Load configuration from database */
    json_value_t* db_config = config_load_from_database(db);
    if (!db_config) {
        pthread_mutex_unlock(&config_mutex);
        return 0; /* No database config, not an error */
    }
    
    /* Keep old config for change notifications */
    json_value_t* old_config = cached_config;
    cached_config = json_clone(db_config);
    
    /* Apply server settings */
    json_value_t* server = json_object_get(db_config, "server");
    if (server && server->type == JSON_OBJECT) {
        json_value_t* val;
        
        val = json_object_get(server, "host");
        if (val && val->type == JSON_STRING) {
            BUFFER_FREE(config->host);
            config->host = BUFFER_STRDUP(val->value.string);
        }
        
        val = json_object_get(server, "port");
        if (val && val->type == JSON_INTEGER) {
            config->port = val->value.integer;
        }
        
        val = json_object_get(server, "verbose");
        if (val && val->type == JSON_BOOLEAN) {
            config->verbose_mode = val->value.boolean;
        }
        
        /* SSL settings */
        json_value_t* ssl = json_object_get(server, "ssl");
        if (ssl && ssl->type == JSON_OBJECT) {
            val = json_object_get(ssl, "enabled");
            if (val && val->type == JSON_BOOLEAN) {
                config->use_ssl = val->value.boolean;
            }
            
            val = json_object_get(ssl, "cert_path");
            if (val && val->type == JSON_STRING) {
                BUFFER_FREE(config->cert_path);
                config->cert_path = BUFFER_STRDUP(val->value.string);
            }
            
            val = json_object_get(ssl, "key_path");
            if (val && val->type == JSON_STRING) {
                BUFFER_FREE(config->key_path);
                config->key_path = BUFFER_STRDUP(val->value.string);
            }
        }
    }
    
    /* Apply thread pool settings */
    json_value_t* thread_pool = json_object_get(db_config, "thread_pool");
    if (thread_pool && thread_pool->type == JSON_OBJECT) {
        json_value_t* val;
        
        val = json_object_get(thread_pool, "min_threads");
        if (val && val->type == JSON_INTEGER) {
            config->thread_pool_min = val->value.integer;
        }
        
        val = json_object_get(thread_pool, "max_threads");
        if (val && val->type == JSON_INTEGER) {
            config->thread_pool_max = val->value.integer;
        }
        
        val = json_object_get(thread_pool, "queue_size");
        if (val && val->type == JSON_INTEGER) {
            config->thread_pool_queue_size = val->value.integer;
        }
        
        val = json_object_get(thread_pool, "idle_timeout");
        if (val && val->type == JSON_INTEGER) {
            config->thread_pool_idle_timeout = val->value.integer;
        }
    }
    
    /* Apply cache settings */
    json_value_t* cache = json_object_get(db_config, "cache");
    if (cache && cache->type == JSON_OBJECT) {
        json_value_t* val;
        
        val = json_object_get(cache, "enabled");
        if (val && val->type == JSON_BOOLEAN) {
            config->cache_enabled = val->value.boolean;
        }
        
        val = json_object_get(cache, "max_size");
        if (val && val->type == JSON_INTEGER) {
            config->cache_max_size = val->value.integer;
        }
        
        val = json_object_get(cache, "ttl");
        if (val && val->type == JSON_INTEGER) {
            config->cache_ttl = val->value.integer;
        }
    }
    
    /* Apply metrics settings */
    json_value_t* metrics = json_object_get(db_config, "metrics");
    if (metrics && metrics->type == JSON_OBJECT) {
        json_value_t* val;
        
        val = json_object_get(metrics, "enabled");
        if (val && val->type == JSON_BOOLEAN) {
            config->metrics_enabled = val->value.boolean;
        }
        
        val = json_object_get(metrics, "retention");
        if (val && val->type == JSON_INTEGER) {
            config->metrics_retention = val->value.integer;
        }
    }
    
    /* Apply indexing settings */
    json_value_t* indexing = json_object_get(db_config, "indexing");
    if (indexing && indexing->type == JSON_OBJECT) {
        json_value_t* val;
        
        val = json_object_get(indexing, "query_threshold");
        if (val && val->type == JSON_INTEGER) {
            config->index_query_threshold = val->value.integer;
        }
        
        val = json_object_get(indexing, "time_threshold");
        if (val && val->type == JSON_INTEGER) {
            config->index_time_threshold = val->value.integer;
        }
        
        val = json_object_get(indexing, "system_query_threshold");
        if (val && val->type == JSON_INTEGER) {
            config->index_query_threshold_system = val->value.integer;
        }
        
        val = json_object_get(indexing, "system_time_threshold");
        if (val && val->type == JSON_INTEGER) {
            config->index_time_threshold_system = val->value.integer;
        }
        
        val = json_object_get(indexing, "startup_delay");
        if (val && val->type == JSON_INTEGER) {
            config->index_startup_delay = val->value.integer;
        }
        
        val = json_object_get(indexing, "check_interval");
        if (val && val->type == JSON_INTEGER) {
            config->index_check_interval = val->value.integer;
        }
    }
    
    /* Apply logging settings */
    json_value_t* logging = json_object_get(db_config, "logging");
    if (logging && logging->type == JSON_OBJECT) {
        json_value_t* val;
        
        val = json_object_get(logging, "level");
        if (val && val->type == JSON_STRING) {
            config->log_level = logger_parse_level(val->value.string);
        }
    }
    
    /* Apply JWT settings (highest priority in three-tier system) */
    json_value_t* jwt = json_object_get(db_config, "jwt");
    if (jwt && jwt->type == JSON_OBJECT) {
        json_value_t* val;
        
        val = json_object_get(jwt, "secret");
        if (val && val->type == JSON_STRING && strlen(val->value.string) >= 16) {
            BUFFER_FREE(config->jwt_secret);
            config->jwt_secret = BUFFER_STRDUP(val->value.string);
            LOG_INFO("JWT secret updated from database configuration (highest priority)");
            
            /* Security validation */
            if (strlen(val->value.string) < 32) {
                LOG_WARNING("JWT secret from database is shorter than recommended 32 characters");
            }
        } else if (val) {
            LOG_ERROR("Invalid JWT secret in database configuration (too short or wrong type)");
        }
    }
    
    /* Notify callbacks of changes */
    config_callback_t* cb = callbacks;
    while (cb) {
        cb->callback("*", old_config, db_config);
        cb = cb->next;
    }
    
    if (old_config) {
        json_free(old_config);
    }
    
    json_free(db_config);
    pthread_mutex_unlock(&config_mutex);
    
    LOG_INFO("Applied configuration from database.");
    return 0;
}

/* Get current configuration as JSON */
json_value_t* config_to_json(server_config_t* config) {
    if (!config) {
        return NULL;
    }
    
    json_value_t* root = json_create_object();
    
    /* Server settings */
    json_value_t* server = json_create_object();
    json_object_set(server, "host", json_create_string(config->host));
    json_object_set(server, "port", json_create_integer(config->port));
    json_object_set(server, "verbose", json_create_boolean(config->verbose_mode));
    
    json_value_t* ssl = json_create_object();
    json_object_set(ssl, "enabled", json_create_boolean(config->use_ssl));
    json_object_set(ssl, "cert_path", json_create_string(config->cert_path ? config->cert_path : ""));
    json_object_set(ssl, "key_path", json_create_string(config->key_path ? config->key_path : ""));
    json_object_set(server, "ssl", ssl);
    
    json_object_set(root, "server", server);
    
    /* Path settings */
    json_value_t* paths = json_create_object();
    json_object_set(paths, "db", json_create_string(config->db_file ? config->db_file : ""));
    json_object_set(paths, "log", json_create_string(config->log_file ? config->log_file : ""));
    json_object_set(paths, "web_root", json_create_string(config->web_root ? config->web_root : ""));
    json_object_set(root, "paths", paths);
    
    /* Thread pool settings */
    json_value_t* thread_pool = json_create_object();
    json_object_set(thread_pool, "min_threads", json_create_integer(config->thread_pool_min));
    json_object_set(thread_pool, "max_threads", json_create_integer(config->thread_pool_max));
    json_object_set(thread_pool, "queue_size", json_create_integer(config->thread_pool_queue_size));
    json_object_set(thread_pool, "idle_timeout", json_create_integer(config->thread_pool_idle_timeout));
    json_object_set(root, "thread_pool", thread_pool);
    
    /* Cache settings */
    json_value_t* cache = json_create_object();
    json_object_set(cache, "enabled", json_create_boolean(config->cache_enabled));
    json_object_set(cache, "max_size", json_create_integer(config->cache_max_size));
    json_object_set(cache, "ttl", json_create_integer(config->cache_ttl));
    json_object_set(root, "cache", cache);
    
    /* Metrics settings */
    json_value_t* metrics = json_create_object();
    json_object_set(metrics, "enabled", json_create_boolean(config->metrics_enabled));
    json_object_set(metrics, "retention", json_create_integer(config->metrics_retention));
    json_object_set(root, "metrics", metrics);
    
    /* Indexing settings */
    json_value_t* indexing = json_create_object();
    json_object_set(indexing, "query_threshold", json_create_integer(config->index_query_threshold));
    json_object_set(indexing, "time_threshold", json_create_integer(config->index_time_threshold));
    json_object_set(indexing, "system_query_threshold", json_create_integer(config->index_query_threshold_system));
    json_object_set(indexing, "system_time_threshold", json_create_integer(config->index_time_threshold_system));
    json_object_set(indexing, "startup_delay", json_create_integer(config->index_startup_delay));
    json_object_set(indexing, "check_interval", json_create_integer(config->index_check_interval));
    json_object_set(root, "indexing", indexing);
    
    /* Logging settings */
    json_value_t* logging = json_create_object();
    json_object_set(logging, "level", json_create_string(logger_level_string(config->log_level)));
    json_object_set(root, "logging", logging);
    
    /* JWT settings (NOTE: JWT secret is NOT included in serialization for security) */
    json_value_t* jwt = json_create_object();
    json_object_set(jwt, "secret_configured", json_create_boolean(config->jwt_secret && strlen(config->jwt_secret) > 0));
    json_object_set(jwt, "secret_length", json_create_integer(config->jwt_secret ? strlen(config->jwt_secret) : 0));
    json_object_set(root, "jwt", jwt);
    
    return root;
}

/* Cleanup */
void config_cleanup(void) {
    pthread_mutex_lock(&config_mutex);
    
    /* Free callbacks */
    config_callback_t* cb = callbacks;
    while (cb) {
        config_callback_t* next = cb->next;
        BUFFER_FREE(cb);
        cb = next;
    }
    callbacks = NULL;
    
    /* Free cached config */
    if (cached_config) {
        json_free(cached_config);
        cached_config = NULL;
    }
    
    pthread_mutex_unlock(&config_mutex);
}