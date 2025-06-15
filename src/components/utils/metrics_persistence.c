#include "utils/metrics.h"
#include "database/document_storage.h"
#include "database/database.h"
#include "utils/logger.h"
#include "utils/json.h"
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/sysinfo.h>

/* Clone a JSON value - temporary until json_deep_copy is added to json utils */
/* json_deep_copy is now available from json_deep_copy.c */

/* Metrics persistence configuration - PURE DOCUMENTS ARCHITECTURE */
#define METRICS_COLLECTION_NAME "system/metrics" /* Metrics stored in system/metrics collection */
#define OLD_METRICS_COLLECTION "metrics"      /* Old collection to clean up */
#define METRICS_SNAPSHOT_INTERVAL 60 /* Save metrics every 60 seconds */
#define METRICS_RETENTION_DAYS 7   /* Keep metrics for 7 days */
#define METRICS_CLEANUP_INTERVAL 3600 /* Clean old metrics every hour */

/* Global variables to store metric document IDs - will be generated once */
static char* g_metric_id_operations = NULL;
static char* g_metric_id_performance = NULL;
static char* g_metric_id_cache = NULL;
static char* g_metric_id_memory = NULL;
static char* g_metric_id_connections = NULL;

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
static char* find_metric_by_name(metrics_persistence_t* mp, const char* metric_name);

/**
 * Initialize metrics persistence
 */
int metrics_persistence_init(database_t* db) {
  if (!db) {
    LOG_ERROR("Cannot initialize metrics persistence without database.");
    return 0;
  }
  
  if (g_metrics_persistence) {
    LOG_WARNING("Metrics persistence already initialized.");
    return 1;
  }
  
  LOG_INFO("Initializing metrics persistence system.");
  
  /* Allocate persistence structure */
  g_metrics_persistence = (metrics_persistence_t*)malloc(sizeof(metrics_persistence_t));
  if (!g_metrics_persistence) {
    LOG_ERROR("allocate metrics persistence structure.");
    return 0;
  }
  
  /* Initialize structure */
  g_metrics_persistence->db = db;
  g_metrics_persistence->running = 1;
  g_metrics_persistence->last_snapshot = 0;
  g_metrics_persistence->last_cleanup = 0;
  pthread_mutex_init(&g_metrics_persistence->lock, NULL);
  
  /* PURE DOCUMENTS: No collection creation needed - using documents collection */
  LOG_INFO("Using documents collection for metrics storage (pure documents architecture)");
  
  /* PURE DOCUMENTS: Skip metric document lookup for now - create fresh */
  LOG_INFO("Skipping existing metric document lookup - will create fresh metrics");
  
  /* Initialize all metric IDs to NULL so they'll be created fresh */
  g_metric_id_operations = NULL;
  g_metric_id_performance = NULL; 
  g_metric_id_cache = NULL;
  g_metric_id_memory = NULL;
  g_metric_id_connections = NULL;
  
  /* SURGICAL FIX: Add startup delay to prevent early JDBX deadlock */
  g_metrics_persistence->last_snapshot = time(NULL) + 30; /* Delay first snapshot by 30 seconds */
  
  /* Start persistence thread */
  if (pthread_create(&g_metrics_persistence->persistence_thread, NULL, 
           metrics_persistence_thread, g_metrics_persistence) != 0) {
    LOG_ERROR("create metrics persistence thread.");
    free(g_metrics_persistence);
    g_metrics_persistence = NULL;
    return 0;
  }
  
  LOG_INFO("Metrics persistence initialized.");
  return 1;
}

/**
 * Shutdown metrics persistence
 */
void metrics_persistence_shutdown(void) {
  if (!g_metrics_persistence) {
    return;
  }
  
  LOG_INFO("Shutting down metrics persistence.");
  
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
  
  /* Free metric IDs */
  if (g_metric_id_operations) { free(g_metric_id_operations); g_metric_id_operations = NULL; }
  if (g_metric_id_performance) { free(g_metric_id_performance); g_metric_id_performance = NULL; }
  if (g_metric_id_cache) { free(g_metric_id_cache); g_metric_id_cache = NULL; }
  if (g_metric_id_memory) { free(g_metric_id_memory); g_metric_id_memory = NULL; }
  if (g_metric_id_connections) { free(g_metric_id_connections); g_metric_id_connections = NULL; }
  
  LOG_INFO("Metrics persistence shutdown complete.");
}

/**
 * Metrics persistence thread
 */
static void* metrics_persistence_thread(void* arg) {
  metrics_persistence_t* mp = (metrics_persistence_t*)arg;
  
  LOG_INFO("Metrics persistence thread started.");
  
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
      LOG_DEBUG("Saving metrics snapshot.");
      save_metrics_snapshot(mp);
      mp->last_snapshot = now;
    }
    
    /* Check if it's time to cleanup old metrics */
    if (now - mp->last_cleanup >= METRICS_CLEANUP_INTERVAL) {
      LOG_DEBUG("Cleanup started.");
      cleanup_old_metrics(mp);
      mp->last_cleanup = now;
    }
    
    /* Sleep for a short time - reduced from 5s to 100ms for lower latency */
    usleep(100000); /* 100ms */
  }
  
  LOG_INFO("Metrics persistence thread stopped.");
  return NULL;
}

/**
 * Helper function to find metric document by name
 */
static char* find_metric_by_name(metrics_persistence_t* mp, const char* metric_name) {
  /* PURE DOCUMENTS: Query documents collection with type=metric */
  json_value_t* query = json_create_object();
  json_object_set(query, "type", json_create_string("metric"));
  json_object_set(query, "name", json_create_string(metric_name));
  json_object_set(query, "library", json_create_string("system"));
  
  json_value_t* result = db_query_documents(mp->db, STORAGE_LIBRARY, STORAGE_COLLECTION, query);
  json_free(query);
  
  if (result) {
    json_value_t* documents = json_object_get(result, "documents");
    if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
      json_value_t* doc = json_array_get(documents, 0);
      json_value_t* id = json_object_get(doc, "uuid");
      if (!id) {
        id = json_object_get(doc, "uuid");
      }
      if (id && id->type == JSON_STRING) {
        char* id_copy = strdup(json_get_string(id));
        json_free(result);
        return id_copy;
      }
    }
    json_free(result);
  }
  return NULL;
}

/**
 * Helper function to update or create a metric document with time-series data
 */
static int update_metric_document(metrics_persistence_t* mp, char** metric_id_ptr,
                 const char* metric_name, const char* metric_type, json_value_t* current_data) {
  if (!mp || !metric_id_ptr || !metric_name || !metric_type || !current_data) {
    return 0;
  }
  
  /* Create timestamp */
  time_t timestamp = time(NULL);
  
  /* Create data point */
  json_value_t* data_point = json_deep_copy(current_data);
  json_object_set(data_point, "timestamp", json_create_integer(timestamp));
  
  /* Find or get metric ID */
  char* metric_id = *metric_id_ptr;
  if (!metric_id) {
    /* Try to find existing document by name */
    metric_id = find_metric_by_name(mp, metric_name);
    if (metric_id) {
      *metric_id_ptr = metric_id;
    }
  }
  
  /* Check if document exists */
  json_value_t* existing = metric_id ? db_get_document(mp->db, STORAGE_LIBRARY, STORAGE_COLLECTION, metric_id) : NULL;
  json_value_t* document_to_save = NULL;
  
  if (existing) {
    /* Document exists - check if values have changed */
    json_value_t* current_values = json_object_get(existing, "current");
    int values_changed = !current_values || !json_equals(current_values, current_data);
    
    /* Always update the timestamp */
    json_object_set(existing, "updated_at", json_create_integer(timestamp));
    
    /* Only append new data point if values have changed */
    if (values_changed) {
      json_value_t* data_array = json_object_get(existing, "data");
      json_value_t* max_entries_val = json_object_get(existing, "max_entries");
      
      int max_entries = (max_entries_val && max_entries_val->type == JSON_INTEGER) ? 
               json_get_integer(max_entries_val) : 15;
      
      if (!data_array || data_array->type != JSON_ARRAY) {
        data_array = json_create_array();
        json_object_set(existing, "data", data_array);
      }
      
      /* Append new data point */
      json_array_append(data_array, data_point);
      
      /* Trim old entries (keep only last max_entries) */
      size_t array_size = json_array_size(data_array);
      if (array_size > (size_t)max_entries) {
        /* Create new array with only recent entries */
        json_value_t* new_array = json_create_array();
        for (size_t i = array_size - max_entries; i < array_size; i++) {
          json_value_t* item = json_array_get(data_array, i);
          if (item) {
            json_array_append(new_array, json_deep_copy(item));
          }
        }
        json_object_set(existing, "data", new_array);
      }
      
      /* Update current values */
      json_object_set(existing, "current", json_deep_copy(current_data));
    } else {
      /* Values haven't changed, free the unused data point */
      json_free(data_point);
    }
    
    document_to_save = existing;
  } else {
    /* Create new document - let db_insert_document generate the ID */
    json_value_t* new_doc = json_create_object();
    
    /* PURE DOCUMENTS: Add document type fields */
    json_object_set(new_doc, "type", json_create_string("metric"));
    json_object_set(new_doc, "library", json_create_string("system"));
    json_object_set(new_doc, "collection", json_create_string("metrics"));
    
    /* Metric-specific fields */
    json_object_set(new_doc, "name", json_create_string(metric_name));
    json_object_set(new_doc, "metric_type", json_create_string(metric_type));
    json_object_set(new_doc, "retention_minutes", json_create_integer(15));
    json_object_set(new_doc, "max_entries", json_create_integer(15));
    
    /* Create data array with first entry */
    json_value_t* data_array = json_create_array();
    json_array_append(data_array, data_point);
    json_object_set(new_doc, "data", data_array);
    
    /* Set current values and timestamps */
    json_object_set(new_doc, "current", json_deep_copy(current_data));
    json_object_set(new_doc, "created_at", json_create_integer(timestamp));
    json_object_set(new_doc, "updated_at", json_create_integer(timestamp));
    
    document_to_save = new_doc;
  }
  
  /* Save the document */
  json_value_t* result = NULL;
  if (existing && metric_id) {
    /* Update existing document */
    result = db_update_document(mp->db, STORAGE_LIBRARY, STORAGE_COLLECTION, metric_id, document_to_save);
  } else {
    /* Insert new document */
    result = db_insert_document(mp->db, STORAGE_LIBRARY, STORAGE_COLLECTION, document_to_save);
    if (result && !*metric_id_ptr) {
      /* Get and store the generated ID */
      json_value_t* id_val = json_object_get(result, "uuid");
      if (id_val && id_val->type == JSON_STRING) {
        *metric_id_ptr = strdup(json_get_string(id_val));
      }
    }
  }
  
  /* Clean up */
  if (existing) {
    json_free(existing);
  } else {
    json_free(document_to_save);
  }
  
  if (result) {
    LOG_DEBUG("Updated: %s", metric_id);
    json_free(result);
    return 1;
  } else {
    LOG_ERROR("update metric document: %s", metric_id);
    return 0;
  }
}

/**
 * Save current metrics snapshot to database
 */
static int save_metrics_snapshot(metrics_persistence_t* mp) {
  if (!mp || !mp->db || !g_metrics_registry) {
    return 0;
  }
  
  int success = 1;
  
  /* Update operations metrics */
  metric_t* request_counter = get_server_requests_metric();
  metric_t* db_ops_counter = get_db_operations_metric();
  metric_t* db_read_ops = get_db_read_operations_metric();
  metric_t* db_write_ops = get_db_write_operations_metric();
  
  json_value_t* operations = json_create_object();
  json_object_set(operations, "total", json_create_integer(request_counter ? request_counter->value.counter : 0));
  json_object_set(operations, "database", json_create_integer(db_ops_counter ? db_ops_counter->value.counter : 0));
  json_object_set(operations, "read", json_create_integer(db_read_ops ? db_read_ops->value.counter : 0));
  json_object_set(operations, "write", json_create_integer(db_write_ops ? db_write_ops->value.counter : 0));
  
  if (!update_metric_document(mp, &g_metric_id_operations, "operations", "operations", operations)) {
    success = 0;
  }
  json_free(operations);
  
  /* Update performance metrics */
  metric_t* request_timer = get_server_request_duration_metric();
  metric_t* active_conns = get_active_connections_metric();
  
  json_value_t* performance = json_create_object();
  if (request_timer && request_timer->value.timer.count > 0) {
    double avg_ms = (request_timer->value.timer.sum / request_timer->value.timer.count) * 1000.0;
    json_object_set(performance, "avg_response_time_ms", json_create_number(avg_ms));
    json_object_set(performance, "min_response_time_ms", json_create_number(request_timer->value.timer.min * 1000.0));
    json_object_set(performance, "max_response_time_ms", json_create_number(request_timer->value.timer.max * 1000.0));
  } else {
    json_object_set(performance, "avg_response_time_ms", json_create_number(0.0));
    json_object_set(performance, "min_response_time_ms", json_create_number(0.0));
    json_object_set(performance, "max_response_time_ms", json_create_number(0.0));
  }
  json_object_set(performance, "active_connections", json_create_integer(active_conns ? (int64_t)active_conns->value.gauge : 0));
  
  if (!update_metric_document(mp, &g_metric_id_performance, "performance", "performance", performance)) {
    success = 0;
  }
  json_free(performance);
  
  /* Update cache metrics */
  metric_t* cache_hits = get_cache_hits_metric();
  metric_t* cache_misses = get_cache_misses_metric();
  metric_t* cache_evictions = get_cache_evictions_metric();
  metric_t* cache_size = get_cache_size_metric();
  
  double hits = cache_hits ? cache_hits->value.counter : 0.0;
  double misses = cache_misses ? cache_misses->value.counter : 0.0;
  double total_requests = hits + misses;
  double hit_rate = (total_requests > 0) ? (hits / total_requests) * 100.0 : 0.0;
  
  json_value_t* cache = json_create_object();
  json_object_set(cache, "hit_rate", json_create_number(hit_rate));
  json_object_set(cache, "hits", json_create_integer((int64_t)hits));
  json_object_set(cache, "misses", json_create_integer((int64_t)misses));
  json_object_set(cache, "evictions", json_create_integer(cache_evictions ? cache_evictions->value.counter : 0));
  json_object_set(cache, "size_bytes", json_create_integer(cache_size ? (int64_t)cache_size->value.gauge : 0));
  
  if (!update_metric_document(mp, &g_metric_id_cache, "cache", "cache", cache)) {
    success = 0;
  }
  json_free(cache);
  
  /* Update memory metrics */
  struct sysinfo mem_info;
  if (sysinfo(&mem_info) == 0) {
    json_value_t* memory = json_create_object();
    json_object_set(memory, "total_kb", json_create_integer(mem_info.totalram / 1024));
    json_object_set(memory, "free_kb", json_create_integer(mem_info.freeram / 1024));
    json_object_set(memory, "used_kb", json_create_integer((mem_info.totalram - mem_info.freeram) / 1024));
    
    /* Get process memory */
    unsigned long process_mem = 0;
    FILE* f = fopen("/proc/self/status", "r");
    if (f) {
      char line[256];
      while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
          sscanf(line, "VmRSS: %lu kB", &process_mem);
          break;
        }
      }
      fclose(f);
    }
    json_object_set(memory, "process_kb", json_create_integer(process_mem));
    
    if (!update_metric_document(mp, &g_metric_id_memory, "memory", "memory", memory)) {
      success = 0;
    }
    json_free(memory);
  }
  
  /* Update connections metrics */
  json_value_t* connections = json_create_object();
  json_object_set(connections, "active", json_create_integer(active_conns ? (int64_t)active_conns->value.gauge : 0));
  json_object_set(connections, "total", json_create_integer(request_counter ? request_counter->value.counter : 0));
  
  if (!update_metric_document(mp, &g_metric_id_connections, "connections", "connections", connections)) {
    success = 0;
  }
  json_free(connections);
  
  if (success) {
    LOG_DEBUG("Metrics saved.");
  } else {
    LOG_ERROR("Some metrics failed to save.");
  }
  
  return success;
}

/**
 * Clean up old metrics data
 */
static int cleanup_old_metrics(metrics_persistence_t* mp) {
  if (!mp || !mp->db) {
    return 0;
  }
  
  int deleted_count = 0;
  
  /* PURE DOCUMENTS: Skip old metrics collection cleanup - it doesn't exist in pure documents architecture */
  LOG_DEBUG("Skipping old metrics collection cleanup - using pure documents architecture");
  
  /* Clean up old metrics collection entirely - DISABLED for pure documents */
  /* json_value_t* result = db_query_documents(mp->db, STORAGE_LIBRARY, STORAGE_COLLECTION, json_create_object()); */
  json_value_t* result = NULL;
  
  if (result) {
    json_value_t* documents = json_object_get(result, "documents");
    if (documents && documents->type == JSON_ARRAY) {
      /* Delete all documents in old metrics collection */
      for (size_t i = 0; i < documents->value.array.size; i++) {
        json_value_t* doc = json_array_get(documents, i);
        json_value_t* id = json_object_get(doc, "uuid");
        
        if (id && id->type == JSON_STRING) {
          const char* id_str = json_get_string(id);
          if (db_delete_document(mp->db, STORAGE_LIBRARY, STORAGE_COLLECTION, id_str)) {
            deleted_count++;
          }
        }
      }
    }
    json_free(result);
  }
  
  if (deleted_count > 0) {
    LOG_INFO("Cleaned up %d legacy metrics documents from old collection", deleted_count);
  }
  
  /* No need to clean up system_metrics collection - it uses fixed document IDs with time-series data */
  
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
                       STORAGE_LIBRARY, STORAGE_COLLECTION, query);
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