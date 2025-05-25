#include "utils/metrics.h"
#include "database/database.h"
#include "utils/logger.h"
#include "utils/json.h"
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

/* Clone a JSON value - temporary until json_deep_copy is added to json utils */
static json_value_t* json_deep_copy(json_value_t* value) {
    if (!value) return NULL;
    
    /* For now, use stringify and parse as a simple deep copy */
    char* json_str = json_stringify(value);
    if (!json_str) return NULL;
    
    json_value_t* copy = json_parse(json_str);
    free(json_str);
    
    return copy;
}

/* Metrics persistence configuration */
#define METRICS_COLLECTION_NAME "_system_metrics"
#define METRICS_SNAPSHOT_INTERVAL 60  /* Save metrics every 60 seconds */
#define METRICS_RETENTION_DAYS 7      /* Keep metrics for 7 days */
#define METRICS_CLEANUP_INTERVAL 3600 /* Clean old metrics every hour */

/* Metrics persistence state */
typedef struct {
    database_t* db;
    pthread_t persistence_thread;
    int running;
    pthread_mutex_t lock;
    time_t last_snapshot;
    time_t last_cleanup;
} metrics_persistence_t;

/* Global metrics persistence instance */
static metrics_persistence_t* g_metrics_persistence = NULL;

/* Forward declarations */
static void* metrics_persistence_thread(void* arg);
static int save_metrics_snapshot(metrics_persistence_t* mp);
static int cleanup_old_metrics(metrics_persistence_t* mp);

/**
 * Initialize metrics persistence
 */
int metrics_persistence_init(database_t* db) {
    if (!db) {
        LOG_ERROR("Cannot initialize metrics persistence without database");
        return 0;
    }
    
    if (g_metrics_persistence) {
        LOG_WARNING("Metrics persistence already initialized");
        return 1;
    }
    
    LOG_INFO("Initializing metrics persistence system");
    
    /* Allocate persistence structure */
    g_metrics_persistence = (metrics_persistence_t*)malloc(sizeof(metrics_persistence_t));
    if (!g_metrics_persistence) {
        LOG_ERROR("Failed to allocate metrics persistence structure");
        return 0;
    }
    
    /* Initialize structure */
    g_metrics_persistence->db = db;
    g_metrics_persistence->running = 1;
    g_metrics_persistence->last_snapshot = 0;
    g_metrics_persistence->last_cleanup = 0;
    pthread_mutex_init(&g_metrics_persistence->lock, NULL);
    
    /* Create metrics collection if it doesn't exist */
    json_value_t* collection = json_object_get(db->collections, METRICS_COLLECTION_NAME);
    if (!collection) {
        LOG_INFO("Creating metrics collection: %s", METRICS_COLLECTION_NAME);
        json_object_set(db->collections, METRICS_COLLECTION_NAME, json_create_array());
    }
    
    /* Start persistence thread */
    if (pthread_create(&g_metrics_persistence->persistence_thread, NULL, 
                      metrics_persistence_thread, g_metrics_persistence) != 0) {
        LOG_ERROR("Failed to create metrics persistence thread");
        free(g_metrics_persistence);
        g_metrics_persistence = NULL;
        return 0;
    }
    
    LOG_INFO("Metrics persistence initialized successfully");
    return 1;
}

/**
 * Shutdown metrics persistence
 */
void metrics_persistence_shutdown(void) {
    if (!g_metrics_persistence) {
        return;
    }
    
    LOG_INFO("Shutting down metrics persistence");
    
    /* Stop persistence thread */
    pthread_mutex_lock(&g_metrics_persistence->lock);
    g_metrics_persistence->running = 0;
    pthread_mutex_unlock(&g_metrics_persistence->lock);
    
    /* Wait for thread to finish */
    pthread_join(g_metrics_persistence->persistence_thread, NULL);
    
    /* Save final snapshot */
    save_metrics_snapshot(g_metrics_persistence);
    
    /* Cleanup */
    pthread_mutex_destroy(&g_metrics_persistence->lock);
    free(g_metrics_persistence);
    g_metrics_persistence = NULL;
    
    LOG_INFO("Metrics persistence shutdown complete");
}

/**
 * Metrics persistence thread
 */
static void* metrics_persistence_thread(void* arg) {
    metrics_persistence_t* mp = (metrics_persistence_t*)arg;
    
    LOG_INFO("Metrics persistence thread started");
    
    while (1) {
        pthread_mutex_lock(&mp->lock);
        int should_run = mp->running;
        pthread_mutex_unlock(&mp->lock);
        
        if (!should_run) {
            break;
        }
        
        time_t now = time(NULL);
        
        /* Check if it's time to save a snapshot */
        if (now - mp->last_snapshot >= METRICS_SNAPSHOT_INTERVAL) {
            LOG_DEBUG("Saving metrics snapshot");
            save_metrics_snapshot(mp);
            mp->last_snapshot = now;
        }
        
        /* Check if it's time to cleanup old metrics */
        if (now - mp->last_cleanup >= METRICS_CLEANUP_INTERVAL) {
            LOG_DEBUG("Cleaning up old metrics");
            cleanup_old_metrics(mp);
            mp->last_cleanup = now;
        }
        
        /* Sleep for a short time */
        sleep(5);
    }
    
    LOG_INFO("Metrics persistence thread stopped");
    return NULL;
}

/**
 * Save current metrics snapshot to database
 */
static int save_metrics_snapshot(metrics_persistence_t* mp) {
    if (!mp || !mp->db || !g_metrics_registry) {
        return 0;
    }
    
    /* Get current metrics as JSON */
    char* metrics_json_str = metrics_get_json(g_metrics_registry);
    if (!metrics_json_str) {
        LOG_ERROR("Failed to get metrics JSON");
        return 0;
    }
    
    /* Parse metrics JSON */
    json_value_t* metrics_json = json_parse(metrics_json_str);
    free(metrics_json_str);
    
    if (!metrics_json) {
        LOG_ERROR("Failed to parse metrics JSON");
        return 0;
    }
    
    /* Create snapshot document */
    json_value_t* snapshot = json_create_object();
    if (!snapshot) {
        json_free(metrics_json);
        LOG_ERROR("Failed to create snapshot document");
        return 0;
    }
    
    /* Add timestamp */
    time_t timestamp = time(NULL);
    json_object_set(snapshot, "timestamp", json_create_integer(timestamp));
    
    /* Add ISO timestamp for easier querying */
    char iso_time[32];
    struct tm* tm_info = gmtime(&timestamp);
    strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    json_object_set(snapshot, "time", json_create_string(iso_time));
    
    /* Add metrics data */
    json_value_t* metrics_array = json_object_get(metrics_json, "metrics");
    if (metrics_array) {
        /* Convert metrics array to object for easier querying */
        json_value_t* metrics_obj = json_create_object();
        
        for (size_t i = 0; i < metrics_array->value.array.size; i++) {
            json_value_t* metric = json_array_get(metrics_array, i);
            json_value_t* name = json_object_get(metric, "name");
            json_value_t* value = json_object_get(metric, "value");
            
            if (name && name->type == JSON_STRING && value) {
                json_object_set(metrics_obj, json_get_string(name), json_deep_copy(value));
            }
        }
        
        json_object_set(snapshot, "metrics", metrics_obj);
    }
    
    json_free(metrics_json);
    
    /* Insert snapshot into database */
    json_value_t* result = db_insert_document(mp->db, METRICS_COLLECTION_NAME, snapshot);
    
    if (result) {
        LOG_DEBUG("Metrics snapshot saved successfully");
        json_free(result);
        return 1;
    } else {
        LOG_ERROR("Failed to save metrics snapshot");
        return 0;
    }
}

/**
 * Clean up old metrics data
 */
static int cleanup_old_metrics(metrics_persistence_t* mp) {
    if (!mp || !mp->db) {
        return 0;
    }
    
    /* Calculate cutoff timestamp */
    time_t cutoff = time(NULL) - (METRICS_RETENTION_DAYS * 24 * 60 * 60);
    
    /* Create query to find old metrics */
    json_value_t* query = json_create_object();
    json_value_t* timestamp_query = json_create_object();
    json_object_set(timestamp_query, "$lt", json_create_integer(cutoff));
    json_object_set(query, "timestamp", timestamp_query);
    
    /* Query old metrics */
    json_value_t* result = db_query_documents(mp->db, METRICS_COLLECTION_NAME, query);
    json_free(query);
    
    if (!result) {
        LOG_ERROR("Failed to query old metrics");
        return 0;
    }
    
    json_value_t* documents = json_object_get(result, "documents");
    if (!documents || documents->type != JSON_ARRAY) {
        json_free(result);
        return 0;
    }
    
    int deleted_count = 0;
    
    /* Delete each old metric document */
    for (size_t i = 0; i < documents->value.array.size; i++) {
        json_value_t* doc = json_array_get(documents, i);
        json_value_t* id = json_object_get(doc, "_id");
        
        if (id && id->type == JSON_STRING) {
            if (db_delete_document(mp->db, METRICS_COLLECTION_NAME, json_get_string(id))) {
                deleted_count++;
            }
        }
    }
    
    json_free(result);
    
    if (deleted_count > 0) {
        LOG_INFO("Cleaned up %d old metrics records", deleted_count);
    }
    
    return 1;
}

/**
 * Get historical metrics for a specific time range
 */
json_value_t* metrics_get_historical(time_t start_time, time_t end_time, const char* metric_name) {
    if (!g_metrics_persistence || !g_metrics_persistence->db) {
        return NULL;
    }
    
    /* Create query for time range */
    json_value_t* query = json_create_object();
    json_value_t* timestamp_query = json_create_object();
    json_object_set(timestamp_query, "$gte", json_create_integer(start_time));
    json_object_set(timestamp_query, "$lte", json_create_integer(end_time));
    json_object_set(query, "timestamp", timestamp_query);
    
    /* Query metrics */
    json_value_t* result = db_query_documents(g_metrics_persistence->db, 
                                             METRICS_COLLECTION_NAME, query);
    json_free(query);
    
    if (!result) {
        return NULL;
    }
    
    json_value_t* documents = json_object_get(result, "documents");
    if (!documents || documents->type != JSON_ARRAY) {
        json_free(result);
        return NULL;
    }
    
    /* Create response array */
    json_value_t* response = json_create_array();
    
    /* Process each snapshot */
    for (size_t i = 0; i < documents->value.array.size; i++) {
        json_value_t* doc = json_array_get(documents, i);
        json_value_t* timestamp = json_object_get(doc, "timestamp");
        json_value_t* metrics = json_object_get(doc, "metrics");
        
        if (timestamp && metrics) {
            json_value_t* point = json_create_object();
            json_object_set(point, "timestamp", json_deep_copy(timestamp));
            
            if (metric_name) {
                /* Get specific metric */
                json_value_t* metric_value = json_object_get(metrics, metric_name);
                if (metric_value) {
                    json_object_set(point, "value", json_deep_copy(metric_value));
                    json_array_append(response, point);
                } else {
                    json_free(point);
                }
            } else {
                /* Get all metrics */
                json_object_set(point, "metrics", json_deep_copy(metrics));
                json_array_append(response, point);
            }
        }
    }
    
    json_free(result);
    return response;
}