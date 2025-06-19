#include "utils/metrics.h"
#include "database/database.h"
#include "database/document_storage.h"
#include "database/virtual_layer.h"
#include "utils/logger.h"
#include "utils/json.h"
#include "utils/json_deep_copy.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "utils/buffer_pool.h"

/* Library-specific metrics collection name */
#define LIBRARY_METRICS_SUFFIX "/metrics"

/**
 * Get metrics collection name for a library
 */
static char* get_library_metrics_collection(const char* library_name) {
  if (!library_name) {
    library_name = "default";
  }
  
  size_t len = strlen(library_name) + strlen(LIBRARY_METRICS_SUFFIX) + 1;
  char* collection_name = BUFFER_ALLOC(len);
  if (!collection_name) {
    return NULL;
  }
  
  snprintf(collection_name, len, "%s%s", library_name, LIBRARY_METRICS_SUFFIX);
  return collection_name;
}

/**
 * Initialize metrics for a library
 */
int library_metrics_init(database_t* db, const char* library_name) {
  if (!db) {
    LOG_ERROR("Cannot initialize library metrics without database");
    return 0;
  }
  
  char* collection_name = get_library_metrics_collection(library_name);
  if (!collection_name) {
    LOG_ERROR("Failed to allocate memory for collection name");
    return 0;
  }
  
  LOG_INFO("Creating library metrics collection: %s", collection_name);
  int result = db_create_collection(db, collection_name);
  BUFFER_FREE(collection_name);
  
  if (result != 0 && result != -1) { /* -1 means collection already exists */
    LOG_ERROR("Failed to create library metrics collection");
    return 0;
  }
  
  return 1;
}

/**
 * Record a library-specific metric
 */
int library_metrics_record(database_t* db, const char* library_name, 
                          const char* metric_type, json_value_t* metric_data) {
  if (!db || !metric_type || !metric_data) {
    return 0;
  }
  
  char* collection_name = get_library_metrics_collection(library_name);
  if (!collection_name) {
    LOG_ERROR("Failed to allocate memory for collection name");
    return 0;
  }
  
  /* Create timestamp */
  time_t timestamp = time(NULL);
  char iso_time[32];
  struct tm* tm_info = gmtime(&timestamp);
  strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", tm_info);
  
  /* Create metric document */
  json_value_t* document = json_create_object();
  json_object_set(document, "type", json_create_string("metric"));
  json_object_set(document, "metric_type", json_create_string(metric_type));
  json_object_set(document, "library", json_create_string(library_name ? library_name : "default"));
  json_object_set(document, "owner", json_create_string(SYSTEM_USER_METRICS));
  json_object_set(document, "timestamp", json_create_string(iso_time));
  json_object_set(document, "data", json_deep_copy(metric_data));
  
  /* Insert metric document using virtual layer - single source of truth */
  json_value_t* result = virtual_insert(db, DOC_TYPE_NAME_METRIC, library_name ? library_name : "default", VIRTUAL_COLLECTION_METRICS, document, SYSTEM_USER_METRICS);
  /* CHECKPOINT: json_free(document); */
  BUFFER_FREE(collection_name);
  
  if (!result) {
    LOG_ERROR("Failed to insert library metric");
    return 0;
  }
  
  /* CHECKPOINT: json_free(result); */
  return 1;
}

/**
 * Query library metrics
 */
json_value_t* library_metrics_query(database_t* db, const char* library_name,
                                   const char* metric_type, int limit) {
  if (!db) {
    return NULL;
  }
  
  char* collection_name = get_library_metrics_collection(library_name);
  if (!collection_name) {
    LOG_ERROR("Failed to allocate memory for collection name");
    return NULL;
  }
  
  /* Build query */
  json_value_t* query = json_create_object();
  if (metric_type) {
    json_object_set(query, "metric_type", json_create_string(metric_type));
  }
  
  /* Query metrics - TODO: Add sorting and limit support to db_query_documents */
  json_value_t* results = db_query_documents(db, STORAGE_LIBRARY, collection_name, query);
  /* CHECKPOINT: json_free(query); */
  
  /* Apply limit manually if results exist */
  if (results && limit > 0) {
    json_value_t* documents = json_object_get(results, "documents");
    if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > (size_t)limit) {
      /* Create new limited array */
      json_value_t* limited_docs = json_create_array();
      for (int i = 0; i < limit && i < (int)json_array_size(documents); i++) {
        json_array_append(limited_docs, json_clone(json_array_get(documents, i)));
      }
      json_object_set(results, "documents", limited_docs);
    }
  }
  
  BUFFER_FREE(collection_name);
  
  return results;
}

/**
 * Get aggregated library metrics
 */
json_value_t* library_metrics_aggregate(database_t* db, const char* library_name) {
  if (!db) {
    return NULL;
  }
  
  /* Create aggregate response */
  json_value_t* response = json_create_object();
  json_object_set(response, "library", json_create_string(library_name ? library_name : "default"));
  
  /* Get latest metrics for each type */
  const char* metric_types[] = {"operations", "performance", "connections", "storage", NULL};
  
  for (int i = 0; metric_types[i] != NULL; i++) {
    json_value_t* results = library_metrics_query(db, library_name, metric_types[i], 1);
    if (results) {
      json_value_t* documents = json_object_get(results, "documents");
      if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
        json_value_t* latest = json_array_get(documents, 0);
        json_value_t* data = json_object_get(latest, "data");
        if (data) {
          json_object_set(response, metric_types[i], json_deep_copy(data));
        }
      }
      /* CHECKPOINT: json_free(results); */
    }
  }
  
  /* Add timestamp */
  time_t timestamp = time(NULL);
  char iso_time[32];
  struct tm* tm_info = gmtime(&timestamp);
  strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", tm_info);
  json_object_set(response, "generated_at", json_create_string(iso_time));
  
  return response;
}

/**
 * Clean up old library metrics
 */
int library_metrics_cleanup(database_t* db, const char* library_name, int retention_days) {
  if (!db || retention_days <= 0) {
    return 0;
  }
  
  char* collection_name = get_library_metrics_collection(library_name);
  if (!collection_name) {
    LOG_ERROR("Failed to allocate memory for collection name");
    return 0;
  }
  
  /* Calculate cutoff timestamp */
  time_t cutoff = time(NULL) - (retention_days * 24 * 60 * 60);
  char cutoff_time[32];
  struct tm* tm_info = gmtime(&cutoff);
  strftime(cutoff_time, sizeof(cutoff_time), "%Y-%m-%dT%H:%M:%SZ", tm_info);
  
  /* Query old metrics */
  json_value_t* query = json_create_object();
  json_value_t* timestamp_filter = json_create_object();
  json_object_set(timestamp_filter, "$lt", json_create_string(cutoff_time));
  json_object_set(query, "timestamp", timestamp_filter);
  
  json_value_t* results = db_query_documents(db, STORAGE_LIBRARY, collection_name, query);
  /* CHECKPOINT: json_free(query); */
  
  int deleted_count = 0;
  if (results) {
    json_value_t* documents = json_object_get(results, "documents");
    if (documents && documents->type == JSON_ARRAY) {
      size_t count = json_array_size(documents);
      for (size_t i = 0; i < count; i++) {
        json_value_t* doc = json_array_get(documents, i);
        json_value_t* id = json_object_get(doc, "uuid");
        if (id && id->type == JSON_STRING) {
          if (db_delete_document(db, STORAGE_LIBRARY, collection_name, id->value.string) == 0) {
            deleted_count++;
          }
        }
      }
    }
    /* CHECKPOINT: json_free(results); */
  }
  
  BUFFER_FREE(collection_name);
  
  if (deleted_count > 0) {
    LOG_INFO("Cleaned up %d old metrics from library %s", deleted_count, 
             library_name ? library_name : "default");
  }
  
  return deleted_count;
}