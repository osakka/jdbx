#include "database/query_tracker.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include "utils/memory_manager.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Global query tracker instance */
static query_tracker_t* g_query_tracker = NULL;

/* === Hash Table Functions === */

/**
 * Hash function for pattern lookup (FNV-1a algorithm)
 */
uint32_t query_pattern_hash(const char* collection_name, const char* field_path) {
    uint32_t hash = 2166136261u; /* FNV offset basis */
    
    /* Hash collection name */
    const char* str = collection_name;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619; /* FNV prime */
    }
    
    /* Add separator */
    hash ^= (uint8_t)'.';
    hash *= 16777619;
    
    /* Hash field path */
    str = field_path;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619;
    }
    
    return hash;
}

/**
 * Calculate time difference in milliseconds
 */
double timespec_diff_ms(const struct timespec* start, const struct timespec* end) {
    double start_ms = start->tv_sec * 1000.0 + start->tv_nsec / 1000000.0;
    double end_ms = end->tv_sec * 1000.0 + end->tv_nsec / 1000000.0;
    return end_ms - start_ms;
}

/* === Field Path Extraction === */

/**
 * Extract field paths from a query JSON object recursively
 */
static void extract_field_paths_recursive(json_value_t* obj, const char* prefix, 
                                         char*** paths, size_t* count, size_t* capacity) {
    if (!obj || !paths || !count || !capacity) return;
    
    if (obj->type == JSON_OBJECT) {
        for (size_t i = 0; i < obj->value.object.size; i++) {
            json_object_entry_t* entry = &obj->value.object.entries[i];
            
            /* Build field path */
            size_t path_len = (prefix ? strlen(prefix) + 1 : 0) + strlen(entry->key) + 1;
            char* field_path = (char*)BUFFER_ALLOC(path_len);
            if (!field_path) continue;
            
            if (prefix) {
                snprintf(field_path, path_len, "%s.%s", prefix, entry->key);
            } else {
                strcpy(field_path, entry->key);
            }
            
            /* Add to paths array */
            if (*count >= *capacity) {
                *capacity = (*capacity == 0) ? 16 : *capacity * 2;
                *paths = (char**)BUFFER_REALLOC(*paths, *capacity * sizeof(char*));
                if (!*paths) {
                    BUFFER_FREE(field_path);
                    return;
                }
            }
            
            (*paths)[(*count)++] = field_path;
            
            /* Recurse into nested objects */
            if (entry->value && entry->value->type == JSON_OBJECT) {
                extract_field_paths_recursive(entry->value, field_path, paths, count, capacity);
            }
        }
    }
}

/**
 * Extract field paths from a query JSON object
 */
char** extract_query_field_paths(json_value_t* query_json, size_t* count) {
    if (!query_json || !count) return NULL;
    
    char** paths = NULL;
    size_t capacity = 0;
    *count = 0;
    
    extract_field_paths_recursive(query_json, NULL, &paths, count, &capacity);
    
    return paths;
}

/**
 * Free array of field paths
 */
void free_field_paths(char** paths, size_t count) {
    if (!paths) return;
    
    for (size_t i = 0; i < count; i++) {
        BUFFER_FREE(paths[i]);
    }
    BUFFER_FREE(paths);
}

/* === Query Tracker Core Functions === */

/**
 * Initialize the global query tracker
 */
int query_tracker_init(void) {
    if (g_query_tracker) {
        LOG_WARNING("Query tracker already initialized.");
        return 1;
    }
    
    g_query_tracker = (query_tracker_t*)BUFFER_ALLOC(sizeof(query_tracker_t));
    if (!g_query_tracker) {
        LOG_ERROR("Cannot allocate query tracker.");
        return 0;
    }
    memory_promote(g_query_tracker);  /* Global query tracker survives checkpoints */
    
    /* Initialize hash table */
    g_query_tracker->num_buckets = 1024; /* Start with 1024 buckets */
    g_query_tracker->patterns = (query_pattern_t**)BUFFER_ALLOC(g_query_tracker->num_buckets * sizeof(query_pattern_t*));
    if (!g_query_tracker->patterns) {
        BUFFER_FREE(g_query_tracker);
        g_query_tracker = NULL;
        LOG_ERROR("Cannot allocate query tracker hash table.");
        return 0;
    }
    memory_promote(g_query_tracker->patterns);  /* Part of query tracker */
    
    /* Initialize other fields */
    g_query_tracker->pattern_count = 0;
    g_query_tracker->last_cleanup = time(NULL);
    
    /* Initialize lock */
    if (pthread_rwlock_init(&g_query_tracker->lock, NULL) != 0) {
        BUFFER_FREE(g_query_tracker->patterns);
        BUFFER_FREE(g_query_tracker);
        g_query_tracker = NULL;
        LOG_ERROR("Cannot initialize query tracker lock.");
        return 0;
    }
    
    LOG_INFO("Query tracker initialized with %zu buckets", g_query_tracker->num_buckets);
    return 1;
}

/**
 * Cleanup the global query tracker
 */
void query_tracker_cleanup(void) {
    if (!g_query_tracker) return;
    
    pthread_rwlock_wrlock(&g_query_tracker->lock);
    
    /* Free all patterns */
    for (size_t i = 0; i < g_query_tracker->num_buckets; i++) {
        query_pattern_t* pattern = g_query_tracker->patterns[i];
        while (pattern) {
            query_pattern_t* next = pattern->next;
            BUFFER_FREE(pattern->collection_name);
            BUFFER_FREE(pattern->field_path);
            BUFFER_FREE(pattern);
            pattern = next;
        }
    }
    
    /* Free hash table */
    BUFFER_FREE(g_query_tracker->patterns);
    
    pthread_rwlock_unlock(&g_query_tracker->lock);
    pthread_rwlock_destroy(&g_query_tracker->lock);
    
    BUFFER_FREE(g_query_tracker);
    g_query_tracker = NULL;
    
    LOG_INFO("Query tracker cleaned up.");
}

/**
 * Find or create a query pattern
 */
static query_pattern_t* find_or_create_pattern(const char* collection_name, const char* field_path) {
    if (!g_query_tracker || !collection_name || !field_path) return NULL;
    
    uint32_t hash = query_pattern_hash(collection_name, field_path);
    size_t bucket = hash % g_query_tracker->num_buckets;
    
    /* Look for existing pattern */
    query_pattern_t* pattern = g_query_tracker->patterns[bucket];
    while (pattern) {
        if (strcmp(pattern->collection_name, collection_name) == 0 &&
            strcmp(pattern->field_path, field_path) == 0) {
            return pattern;
        }
        pattern = pattern->next;
    }
    
    /* Create new pattern */
    pattern = (query_pattern_t*)BUFFER_ALLOC(sizeof(query_pattern_t));
    if (!pattern) return NULL;
    
    /* Initialize pattern */
    pattern->collection_name = BUFFER_STRDUP(collection_name);
    pattern->field_path = BUFFER_STRDUP(field_path);
    pattern->query_count = 0;
    pattern->total_time_ms = 0.0;
    pattern->avg_time_ms = 0.0;
    pattern->max_time_ms = 0.0;
    pattern->first_seen = time(NULL);
    pattern->last_seen = time(NULL);
    pattern->has_index = 0;
    pattern->next = g_query_tracker->patterns[bucket];
    
    if (!pattern->collection_name || !pattern->field_path) {
        BUFFER_FREE(pattern->collection_name);
        BUFFER_FREE(pattern->field_path);
        BUFFER_FREE(pattern);
        return NULL;
    }
    
    /* Add to hash table */
    g_query_tracker->patterns[bucket] = pattern;
    g_query_tracker->pattern_count++;
    
    TRACE_DB("Created new query pattern: %s.%s", collection_name, field_path);
    
    return pattern;
}

/**
 * Start timing a query
 */
query_timing_t* query_tracker_start_timing(const char* collection_name, json_value_t* query_json) {
    if (!g_query_tracker || !collection_name) return NULL;
    
    query_timing_t* timing = (query_timing_t*)BUFFER_ALLOC(sizeof(query_timing_t));
    if (!timing) return NULL;
    
    /* Record start time */
    clock_gettime(CLOCK_MONOTONIC, &timing->start_time);
    
    /* Store query information */
    timing->collection_name = collection_name; /* Assume string persists during query */
    timing->query_json = query_json;          /* Assume JSON persists during query */
    
    return timing;
}

/**
 * End timing a query and record the pattern
 */
void query_tracker_end_timing(query_timing_t* timing) {
    if (!timing || !g_query_tracker) {
        BUFFER_FREE(timing);
        return;
    }
    
    /* Calculate query duration */
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double duration_ms = timespec_diff_ms(&timing->start_time, &end_time);
    
    /* Extract field paths from query */
    size_t field_count = 0;
    char** field_paths = extract_query_field_paths(timing->query_json, &field_count);
    
    if (field_paths && field_count > 0) {
        pthread_rwlock_wrlock(&g_query_tracker->lock);
        
        /* Record pattern for each queried field */
        for (size_t i = 0; i < field_count; i++) {
            query_pattern_t* pattern = find_or_create_pattern(timing->collection_name, field_paths[i]);
            if (pattern) {
                /* Update statistics */
                pattern->query_count++;
                pattern->total_time_ms += duration_ms;
                pattern->avg_time_ms = pattern->total_time_ms / pattern->query_count;
                if (duration_ms > pattern->max_time_ms) {
                    pattern->max_time_ms = duration_ms;
                }
                pattern->last_seen = time(NULL);
                
                TRACE_DB("Updated pattern %s.%s: count=%lu, avg=%.2fms", 
                         pattern->collection_name, pattern->field_path, 
                         pattern->query_count, pattern->avg_time_ms);
            }
        }
        
        pthread_rwlock_unlock(&g_query_tracker->lock);
        
        /* Cleanup field paths */
        free_field_paths(field_paths, field_count);
    }
    
    /* Free timing context */
    BUFFER_FREE(timing);
}

/**
 * Get query patterns that should be indexed
 */
query_pattern_t** query_tracker_get_index_candidates(size_t* count) {
    if (!g_query_tracker || !count) return NULL;
    
    *count = 0;
    query_pattern_t** candidates = NULL;
    size_t capacity = 0;
    
    pthread_rwlock_rdlock(&g_query_tracker->lock);
    
    /* Scan all patterns for indexing candidates */
    size_t total_patterns = 0;
    for (size_t i = 0; i < g_query_tracker->num_buckets; i++) {
        query_pattern_t* pattern = g_query_tracker->patterns[i];
        while (pattern) {
            total_patterns++;
            
            /* Debug log each pattern - moved after threshold calculation */
            
            /* Determine dynamic thresholds based on collection size */
            size_t count_threshold = QUERY_TRACKER_INDEX_THRESHOLD_COUNT_DEFAULT;
            double time_threshold = QUERY_TRACKER_INDEX_THRESHOLD_AVG_MS_DEFAULT;
            
            /* For system collections, use lower thresholds */
            if (pattern->collection_name[0] == '_') {
                count_threshold = QUERY_TRACKER_INDEX_THRESHOLD_COUNT_MIN;
                time_threshold = QUERY_TRACKER_INDEX_THRESHOLD_AVG_MS_MIN;
            }
            
            /* Debug log with actual thresholds */
            LOG_DEBUG("Checking pattern: %s.%s (has_index=%d, count=%lu, avg=%.2fms) vs thresholds (count>=%lu, avg>=%.1f)", 
                     pattern->collection_name, pattern->field_path, pattern->has_index,
                     pattern->query_count, pattern->avg_time_ms,
                     (unsigned long)count_threshold, time_threshold);
            
            /* Check if pattern meets indexing thresholds */
            if (!pattern->has_index &&
                pattern->query_count >= count_threshold &&
                pattern->avg_time_ms >= time_threshold) {
                
                /* Add to candidates array */
                if (*count >= capacity) {
                    capacity = (capacity == 0) ? 16 : capacity * 2;
                    candidates = (query_pattern_t**)BUFFER_REALLOC(candidates, capacity * sizeof(query_pattern_t*));
                    if (!candidates) break;
                }
                
                candidates[(*count)++] = pattern;
                
                LOG_INFO("Index candidate: %s.%s (count=%lu, avg=%.2fms)", 
                        pattern->collection_name, pattern->field_path,
                        pattern->query_count, pattern->avg_time_ms);
            }
            
            pattern = pattern->next;
        }
    }
    
    pthread_rwlock_unlock(&g_query_tracker->lock);
    
    LOG_INFO("Found %zu index candidates from %zu total patterns", *count, total_patterns);
    return candidates;
}

/**
 * Mark a field as having an index
 */
void query_tracker_mark_indexed(const char* collection_name, const char* field_path) {
    if (!g_query_tracker || !collection_name || !field_path) return;
    
    pthread_rwlock_wrlock(&g_query_tracker->lock);
    
    uint32_t hash = query_pattern_hash(collection_name, field_path);
    size_t bucket = hash % g_query_tracker->num_buckets;
    
    query_pattern_t* pattern = g_query_tracker->patterns[bucket];
    while (pattern) {
        if (strcmp(pattern->collection_name, collection_name) == 0 &&
            strcmp(pattern->field_path, field_path) == 0) {
            pattern->has_index = 1;
            LOG_INFO("Marked %s.%s as indexed", collection_name, field_path);
            break;
        }
        pattern = pattern->next;
    }
    
    pthread_rwlock_unlock(&g_query_tracker->lock);
}

/**
 * Get query statistics for monitoring
 */
json_value_t* query_tracker_get_stats(void) {
    if (!g_query_tracker) return NULL;
    
    json_value_t* stats = json_create_object();
    if (!stats) return NULL;
    
    pthread_rwlock_rdlock(&g_query_tracker->lock);
    
    /* Overall statistics */
    json_object_set(stats, "total_patterns", json_create_integer(g_query_tracker->pattern_count));
    json_object_set(stats, "hash_buckets", json_create_integer(g_query_tracker->num_buckets));
    
    /* Top patterns by query count */
    json_value_t* top_patterns = json_create_array();
    
    /* Simple approach: collect all patterns and sort by query count */
    query_pattern_t* all_patterns[1000]; /* Limit to top 1000 */
    size_t pattern_count = 0;
    
    for (size_t i = 0; i < g_query_tracker->num_buckets && pattern_count < 1000; i++) {
        query_pattern_t* pattern = g_query_tracker->patterns[i];
        while (pattern && pattern_count < 1000) {
            all_patterns[pattern_count++] = pattern;
            pattern = pattern->next;
        }
    }
    
    /* Simple bubble sort by query count (top 10) */
    for (size_t i = 0; i < pattern_count && i < 10; i++) {
        for (size_t j = i + 1; j < pattern_count; j++) {
            if (all_patterns[j]->query_count > all_patterns[i]->query_count) {
                query_pattern_t* temp = all_patterns[i];
                all_patterns[i] = all_patterns[j];
                all_patterns[j] = temp;
            }
        }
        
        /* Add top pattern to JSON */
        json_value_t* pattern_json = json_create_object();
        json_object_set(pattern_json, "collection", json_create_string(all_patterns[i]->collection_name));
        json_object_set(pattern_json, "field", json_create_string(all_patterns[i]->field_path));
        json_object_set(pattern_json, "query_count", json_create_integer(all_patterns[i]->query_count));
        json_object_set(pattern_json, "avg_time_ms", json_create_number(all_patterns[i]->avg_time_ms));
        json_object_set(pattern_json, "has_index", json_create_boolean(all_patterns[i]->has_index));
        
        json_array_append(top_patterns, pattern_json);
    }
    
    json_object_set(stats, "top_patterns", top_patterns);
    
    pthread_rwlock_unlock(&g_query_tracker->lock);
    
    return stats;
}

/**
 * Force cleanup of old/unused patterns
 */
void query_tracker_force_cleanup(void) {
    if (!g_query_tracker) return;
    
    time_t now = time(NULL);
    time_t cutoff = now - (30 * 24 * 60 * 60); /* 30 days old */
    
    pthread_rwlock_wrlock(&g_query_tracker->lock);
    
    size_t removed = 0;
    
    for (size_t i = 0; i < g_query_tracker->num_buckets; i++) {
        query_pattern_t** current = &g_query_tracker->patterns[i];
        
        while (*current) {
            query_pattern_t* pattern = *current;
            
            /* Remove old patterns with low query count */
            if (pattern->last_seen < cutoff && pattern->query_count < 10) {
                *current = pattern->next;
                BUFFER_FREE(pattern->collection_name);
                BUFFER_FREE(pattern->field_path);
                BUFFER_FREE(pattern);
                removed++;
                g_query_tracker->pattern_count--;
            } else {
                current = &pattern->next;
            }
        }
    }
    
    g_query_tracker->last_cleanup = now;
    
    pthread_rwlock_unlock(&g_query_tracker->lock);
    
    LOG_INFO("Query tracker cleanup: removed %zu old patterns", removed);
}