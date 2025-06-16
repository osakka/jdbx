#include "database/adaptive_indexer.h"
#include "database/query_tracker.h"
#include "database/index_metrics.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Forward declarations and types from database.c */
typedef struct {
    char field_name[128];
    void* btree;  /* btree_disk_t* */
    int active;
} secondary_index_t;

typedef struct {
    char name[256];
    void* storage;      /* mmap_storage_t* */
    void* primary_index; /* hash_index_t* */
    
    /* Secondary indexes */
    secondary_index_t indexes[16];  /* MAX_INDEXES_PER_COLLECTION */
    int num_indexes;
    
    void* cache;  /* generic_cache_t* */
    pthread_rwlock_t lock;
    
    /* Statistics */
    _Atomic uint64_t doc_count;
    _Atomic uint64_t total_size;
} hp_collection_t;

/* Global adaptive indexer instance */
static adaptive_indexer_t* g_adaptive_indexer = NULL;

/* Get global adaptive indexer instance */
adaptive_indexer_t* adaptive_indexer_get_instance(void) {
    return g_adaptive_indexer;
}

/* === Helper Functions === */

/**
 * Generate an appropriate index name for a field
 */
char* generate_adaptive_index_name(const char* collection_name, const char* field_path) {
    if (!collection_name || !field_path) return NULL;
    
    size_t name_len = strlen(collection_name) + strlen(field_path) + 20;
    char* index_name = (char*)BUFFER_ALLOC(name_len);
    if (!index_name) return NULL;
    
    /* Create a safe index name by replacing dots with underscores */
    char* safe_field = BUFFER_STRDUP(field_path);
    for (char* p = safe_field; p && *p; p++) {
        if (*p == '.') *p = '_';
    }
    
    snprintf(index_name, name_len, "adaptive_%s_%s", collection_name, safe_field);
    BUFFER_FREE(safe_field);
    
    return index_name;
}

/**
 * Check collection document count for indexing threshold
 */
size_t get_collection_document_count(database_t* db, const char* collection_name) {
    if (!db || !collection_name) return 0;
    
    pthread_rwlock_rdlock(&db->rwlock);
    
    json_value_t* collection = json_object_get(db->collections, collection_name);
    size_t count = 0;
    
    if (collection && collection->type == JSON_ARRAY) {
        count = collection->value.array.size;
    }
    
    pthread_rwlock_unlock(&db->rwlock);
    
    return count;
}

/**
 * Check if an index already exists for a field
 */
int index_exists_for_field(database_t* db, const char* collection_name, const char* field_path) {
    if (!db || !collection_name || !field_path) return 0;
    
    /* Get collection - cast to hp_collection_t */
    hp_collection_t* coll = (hp_collection_t*)db_get_collection(db, collection_name);
    if (!coll) return 0;
    
    /* Lock collection for reading */
    pthread_rwlock_rdlock(&coll->lock);
    
    /* Check if any index exists for this field path */
    for (int i = 0; i < coll->num_indexes; i++) {
        if (coll->indexes[i].active && 
            strcmp(coll->indexes[i].field_name, field_path) == 0) {
            pthread_rwlock_unlock(&coll->lock);
            return 1;  /* Index exists */
        }
    }
    
    pthread_rwlock_unlock(&coll->lock);
    return 0;  /* No index found */
}

/**
 * Get index count for a collection
 */
size_t get_collection_index_count(database_t* db, const char* collection_name) {
    if (!db || !collection_name) return 0;
    
    /* Get collection - cast to hp_collection_t */
    hp_collection_t* coll = (hp_collection_t*)db_get_collection(db, collection_name);
    if (!coll) return 0;
    
    /* Lock collection for reading */
    pthread_rwlock_rdlock(&coll->lock);
    
    /* Get the count of active indexes */
    size_t count = coll->num_indexes;
    
    pthread_rwlock_unlock(&coll->lock);
    return count;
}

/**
 * Log index creation activity
 */
void log_index_creation(const char* collection_name, const char* field_path, 
                       index_creation_status_t status, const char* reason) {
    const char* status_str = "UNKNOWN";
    const char* level = "INFO";
    
    switch (status) {
        case INDEX_CREATION_SUCCESS:
            status_str = "SUCCESS";
            level = "INFO";
            break;
        case INDEX_CREATION_EXISTS:
            status_str = "EXISTS";
            level = "DEBUG";
            break;
        case INDEX_CREATION_ERROR:
            status_str = "ERROR";
            level = "ERROR";
            break;
        case INDEX_CREATION_SKIPPED_LOW_DOCS:
            status_str = "SKIPPED_LOW_DOCS";
            level = "DEBUG";
            break;
        case INDEX_CREATION_SKIPPED_TOO_MANY:
            status_str = "SKIPPED_TOO_MANY";
            level = "WARNING";
            break;
    }
    
    if (strcmp(level, "ERROR") == 0) {
        LOG_ERROR("ADAPTIVE_INDEX_%s: %s.%s - %s", 
                 status_str, collection_name, field_path, reason ? reason : "");
    } else if (strcmp(level, "WARNING") == 0) {
        LOG_WARNING("ADAPTIVE_INDEX_%s: %s.%s - %s", 
                   status_str, collection_name, field_path, reason ? reason : "");
    } else if (strcmp(level, "INFO") == 0) {
        LOG_INFO("ADAPTIVE_INDEX_%s: %s.%s - %s", 
                status_str, collection_name, field_path, reason ? reason : "");
    } else {
        LOG_DEBUG("ADAPTIVE_INDEX_%s: %s.%s - %s", 
                 status_str, collection_name, field_path, reason ? reason : "");
    }
}

/* === Core Adaptive Indexer Functions === */

/**
 * Check if a field should be indexed based on query patterns
 */
int should_create_index(const char* collection_name, const char* field_path, 
                       uint64_t query_count, double avg_time_ms) {
    if (!collection_name || !field_path) return 0;
    
    /* Determine dynamic thresholds */
    size_t count_threshold = QUERY_TRACKER_INDEX_THRESHOLD_COUNT_DEFAULT;
    double time_threshold = QUERY_TRACKER_INDEX_THRESHOLD_AVG_MS_DEFAULT;
    
    /* For system collections, use lower thresholds */
    if (collection_name[0] == '_') {
        count_threshold = QUERY_TRACKER_INDEX_THRESHOLD_COUNT_MIN;
        time_threshold = QUERY_TRACKER_INDEX_THRESHOLD_AVG_MS_MIN;
    }
    
    /* Apply indexing thresholds */
    if (query_count < count_threshold) {
        return 0;  /* Not enough queries */
    }
    
    if (avg_time_ms < time_threshold) {
        return 0;  /* Queries are already fast enough */
    }
    
    /* Check if collection has enough documents */
    if (!g_adaptive_indexer || !g_adaptive_indexer->database) return 0;
    
    size_t doc_count = get_collection_document_count(g_adaptive_indexer->database, collection_name);
    if (doc_count < ADAPTIVE_INDEX_MIN_DOCUMENTS) {
        return 0;  /* Collection too small to benefit from indexing */
    }
    
    /* Check if index already exists */
    if (index_exists_for_field(g_adaptive_indexer->database, collection_name, field_path)) {
        return 0;  /* Index already exists */
    }
    
    /* Check if collection already has too many indexes */
    size_t index_count = get_collection_index_count(g_adaptive_indexer->database, collection_name);
    if (index_count >= ADAPTIVE_INDEX_MAX_INDEXES_PER_COLLECTION) {
        return 0;  /* Too many indexes already */
    }
    
    return 1;  /* Should create index */
}

/**
 * Create an adaptive index for a specific field
 */
index_creation_status_t create_adaptive_index(const char* collection_name, 
                                             const char* field_path,
                                             const char* reason) {
    if (!collection_name || !field_path || !g_adaptive_indexer) {
        return INDEX_CREATION_ERROR;
    }
    
    /* Check document count threshold */
    size_t doc_count = get_collection_document_count(g_adaptive_indexer->database, collection_name);
    if (doc_count < ADAPTIVE_INDEX_MIN_DOCUMENTS) {
        log_index_creation(collection_name, field_path, INDEX_CREATION_SKIPPED_LOW_DOCS, 
                          "Collection has too few documents");
        return INDEX_CREATION_SKIPPED_LOW_DOCS;
    }
    
    /* Skip creating indexes during server startup to avoid initialization issues */
    time_t now = time(NULL);
    if (now - g_adaptive_indexer->last_check < 10) {
        LOG_INFO("Skipping index creation during startup period.");
        return INDEX_CREATION_SKIPPED_LOW_DOCS;
    }
    
    /* Check index limit */
    size_t index_count = get_collection_index_count(g_adaptive_indexer->database, collection_name);
    if (index_count >= ADAPTIVE_INDEX_MAX_INDEXES_PER_COLLECTION) {
        log_index_creation(collection_name, field_path, INDEX_CREATION_SKIPPED_TOO_MANY,
                          "Collection already has maximum number of indexes");
        return INDEX_CREATION_SKIPPED_TOO_MANY;
    }
    
    /* Check if index already exists */
    if (index_exists_for_field(g_adaptive_indexer->database, collection_name, field_path)) {
        log_index_creation(collection_name, field_path, INDEX_CREATION_EXISTS,
                          "Index already exists for field");
        return INDEX_CREATION_EXISTS;
    }
    
    /* Generate index name */
    char* index_name = generate_adaptive_index_name(collection_name, field_path);
    if (!index_name) {
        log_index_creation(collection_name, field_path, INDEX_CREATION_ERROR,
                          "Failed to generate index name");
        return INDEX_CREATION_ERROR;
    }
    
    /* Create the actual database index using the real index system */
    LOG_INFO("Creating index: %s on %s.%s", index_name, collection_name, field_path);
    
    index_t* new_index = db_create_index(g_adaptive_indexer->database, 
                                         collection_name, index_name, 
                                         field_path, INDEX_TYPE_NON_UNIQUE);
    
    if (!new_index) {
        BUFFER_FREE(index_name);
        log_index_creation(collection_name, field_path, INDEX_CREATION_ERROR,
                          "Failed to create database index");
        return INDEX_CREATION_ERROR;
    }
    
    LOG_INFO("created index: %s", index_name);
    
    /* Create adaptive index info record */
    adaptive_index_info_t* index_info = (adaptive_index_info_t*)BUFFER_ALLOC(sizeof(adaptive_index_info_t));
    if (!index_info) {
        BUFFER_FREE(index_name);
        log_index_creation(collection_name, field_path, INDEX_CREATION_ERROR,
                          "Failed to allocate index info");
        return INDEX_CREATION_ERROR;
    }
    
    /* Initialize index info */
    index_info->collection_name = BUFFER_STRDUP(collection_name);
    index_info->field_path = BUFFER_STRDUP(field_path);
    index_info->index_name = index_name;
    index_info->created_at = time(NULL);
    index_info->queries_before_index = 0;  /* Would be populated from query tracker */
    index_info->avg_time_before_ms = 0.0;  /* Would be populated from query tracker */
    index_info->avg_time_after_ms = 0.0;   /* Will be measured after creation */
    index_info->is_effective = -1;          /* Not yet evaluated */
    index_info->next = NULL;
    
    /* Add to adaptive indexer's list */
    pthread_mutex_lock(&g_adaptive_indexer->indexes_lock);
    index_info->next = g_adaptive_indexer->indexes;
    g_adaptive_indexer->indexes = index_info;
    g_adaptive_indexer->indexes_created++;
    pthread_mutex_unlock(&g_adaptive_indexer->indexes_lock);
    
    /* Mark pattern as indexed in query tracker */
    query_tracker_mark_indexed(collection_name, field_path);
    
    /* Start tracking index metrics for this new index */
    index_metrics_start_tracking(collection_name, field_path, index_name);
    
    log_index_creation(collection_name, field_path, INDEX_CREATION_SUCCESS, reason);
    
    return INDEX_CREATION_SUCCESS;
}

/**
 * Process query tracker candidates and create indexes
 */
void process_index_candidates(void) {
    if (!g_adaptive_indexer) return;
    
    /* Get index candidates from query tracker */
    size_t candidate_count = 0;
    query_pattern_t** candidates = query_tracker_get_index_candidates(&candidate_count);
    
    if (!candidates || candidate_count == 0) {
        LOG_DEBUG("No index candidates found.");
        return;
    }
    
    LOG_INFO("Processing %zu index candidates", candidate_count);
    
    /* Process each candidate */
    for (size_t i = 0; i < candidate_count; i++) {
        query_pattern_t* pattern = candidates[i];
        
        if (!pattern || !pattern->collection_name || !pattern->field_path) {
            continue;
        }
        
        /* Check if we should create an index for this pattern */
        if (should_create_index(pattern->collection_name, pattern->field_path,
                               pattern->query_count, pattern->avg_time_ms)) {
            
            /* Create reason string */
            char reason[256];
            snprintf(reason, sizeof(reason), 
                    "queries=%lu, avg_time=%.2fms", 
                    pattern->query_count, pattern->avg_time_ms);
            
            /* Create the index */
            index_creation_status_t status = create_adaptive_index(
                pattern->collection_name, 
                pattern->field_path, 
                reason
            );
            
            if (status == INDEX_CREATION_SUCCESS) {
                LOG_INFO("Created adaptive index for %s.%s", 
                        pattern->collection_name, pattern->field_path);
            }
        } else {
            g_adaptive_indexer->indexes_skipped++;
        }
    }
    
    /* Free candidates array */
    BUFFER_FREE(candidates);
}

/**
 * Background thread function for adaptive indexing
 */
void* adaptive_indexer_thread_func(void* arg) {
    (void)arg;  /* Unused parameter */
    
    LOG_INFO("Adaptive indexer thread started.");
    
    while (g_adaptive_indexer && !g_adaptive_indexer->should_stop) {
        /* Sleep for the check interval */
        for (int i = 0; i < ADAPTIVE_INDEX_CHECK_INTERVAL && !g_adaptive_indexer->should_stop; i++) {
            sleep(1);
        }
        
        if (g_adaptive_indexer->should_stop) break;
        
        /* Process index candidates */
        LOG_DEBUG("Adaptive indexer checking for new index candidates...");
        process_index_candidates();
        
        /* Update last check time */
        g_adaptive_indexer->last_check = time(NULL);
    }
    
    LOG_INFO("Adaptive indexer thread stopped.");
    return NULL;
}

/* === Public API Functions === */

/**
 * Initialize the adaptive indexer
 */
int adaptive_indexer_init(database_t* database) {
    if (g_adaptive_indexer) {
        LOG_WARNING("Adaptive indexer already initialized.");
        return 1;
    }
    
    if (!database) {
        LOG_ERROR("Cannot initialize adaptive indexer: NULL database.");
        return 0;
    }
    
    g_adaptive_indexer = (adaptive_indexer_t*)BUFFER_ALLOC(sizeof(adaptive_indexer_t));
    if (!g_adaptive_indexer) {
        LOG_ERROR("Cannot allocate adaptive indexer.");
        return 0;
    }
    
    /* Initialize structure */
    g_adaptive_indexer->database = database;
    g_adaptive_indexer->indexes = NULL;
    g_adaptive_indexer->should_stop = 0;
    g_adaptive_indexer->last_check = time(NULL);
    g_adaptive_indexer->indexes_created = 0;
    g_adaptive_indexer->indexes_skipped = 0;
    
    /* Initialize mutex */
    if (pthread_mutex_init(&g_adaptive_indexer->indexes_lock, NULL) != 0) {
        BUFFER_FREE(g_adaptive_indexer);
        g_adaptive_indexer = NULL;
        LOG_ERROR("Cannot initialize adaptive indexer mutex.");
        return 0;
    }
    
    LOG_INFO("Adaptive indexer initialized.");
    return 1;
}

/**
 * Start the background indexer thread
 */
int adaptive_indexer_start(void) {
    if (!g_adaptive_indexer) {
        LOG_ERROR("Cannot start adaptive indexer: not initialized.");
        return 0;
    }
    
    /* Create background thread */
    if (pthread_create(&g_adaptive_indexer->indexer_thread, NULL, 
                      adaptive_indexer_thread_func, NULL) != 0) {
        LOG_ERROR("Cannot create adaptive indexer thread.");
        return 0;
    }
    
    LOG_INFO("Adaptive indexer background thread started.");
    return 1;
}

/**
 * Stop the background indexer thread
 */
void adaptive_indexer_stop(void) {
    if (!g_adaptive_indexer) return;
    
    /* Signal thread to stop */
    g_adaptive_indexer->should_stop = 1;
    
    /* Wait for thread to finish */
    pthread_join(g_adaptive_indexer->indexer_thread, NULL);
    
    LOG_INFO("Adaptive indexer thread stopped.");
}

/**
 * Force an immediate check for new indexes to create
 */
void adaptive_indexer_force_check(void) {
    if (!g_adaptive_indexer) return;
    
    LOG_INFO("Forcing adaptive indexer check.");
    process_index_candidates();
}

/**
 * Cleanup the adaptive indexer
 */
void adaptive_indexer_cleanup(void) {
    if (!g_adaptive_indexer) return;
    
    /* Stop background thread */
    adaptive_indexer_stop();
    
    /* Free all adaptive index info records */
    pthread_mutex_lock(&g_adaptive_indexer->indexes_lock);
    
    adaptive_index_info_t* current = g_adaptive_indexer->indexes;
    while (current) {
        adaptive_index_info_t* next = current->next;
        BUFFER_FREE(current->collection_name);
        BUFFER_FREE(current->field_path);
        BUFFER_FREE(current->index_name);
        BUFFER_FREE(current);
        current = next;
    }
    
    pthread_mutex_unlock(&g_adaptive_indexer->indexes_lock);
    
    /* Destroy mutex */
    pthread_mutex_destroy(&g_adaptive_indexer->indexes_lock);
    
    /* Free indexer structure */
    BUFFER_FREE(g_adaptive_indexer);
    g_adaptive_indexer = NULL;
    
    LOG_INFO("Adaptive indexer cleaned up.");
}

/**
 * Get statistics about adaptive indexing
 */
json_value_t* adaptive_indexer_get_stats(void) {
    if (!g_adaptive_indexer) return NULL;
    
    json_value_t* stats = json_create_object();
    if (!stats) return NULL;
    
    pthread_mutex_lock(&g_adaptive_indexer->indexes_lock);
    
    /* Basic statistics */
    json_object_set(stats, "indexes_created", 
                    json_create_integer(g_adaptive_indexer->indexes_created));
    json_object_set(stats, "indexes_skipped", 
                    json_create_integer(g_adaptive_indexer->indexes_skipped));
    json_object_set(stats, "last_check", 
                    json_create_integer(g_adaptive_indexer->last_check));
    json_object_set(stats, "check_interval_seconds", 
                    json_create_integer(ADAPTIVE_INDEX_CHECK_INTERVAL));
    
    /* Index list */
    json_value_t* indexes_array = json_create_array();
    adaptive_index_info_t* current = g_adaptive_indexer->indexes;
    
    while (current) {
        json_value_t* index_json = json_create_object();
        
        json_object_set(index_json, "collection", json_create_string(current->collection_name));
        json_object_set(index_json, "field", json_create_string(current->field_path));
        json_object_set(index_json, "index_name", json_create_string(current->index_name));
        json_object_set(index_json, "created_at", json_create_integer(current->created_at));
        json_object_set(index_json, "queries_before", json_create_integer(current->queries_before_index));
        json_object_set(index_json, "avg_time_before_ms", json_create_number(current->avg_time_before_ms));
        json_object_set(index_json, "avg_time_after_ms", json_create_number(current->avg_time_after_ms));
        json_object_set(index_json, "is_effective", json_create_integer(current->is_effective));
        
        json_array_append(indexes_array, index_json);
        current = current->next;
    }
    
    json_object_set(stats, "adaptive_indexes", indexes_array);
    
    pthread_mutex_unlock(&g_adaptive_indexer->indexes_lock);
    
    return stats;
}