/**
 * @file cache_helpers.h
 * @brief Helper functions for caching operations
 * 
 * This header provides utility functions for cache operations and key generation,
 * which are used by the database and cache subsystems.
 */

#ifndef JDBX_CACHE_HELPERS_H
#define JDBX_CACHE_HELPERS_H

#include "database/database.h"
#include <time.h>

/**
 * Generate a simple UUID for use in document IDs
 * 
 * @return Newly allocated UUID string (caller must free)
 */
char* generate_simple_uuid();

/**
 * Generate a cache key for a document
 * 
 * @param collection Collection name
 * @param id Document ID
 * @return Newly allocated cache key (caller must free)
 */
char* generate_cache_key(const char* collection, const char* id);

/**
 * Generate a cache key for a collection
 * 
 * @param collection Collection name
 * @return Newly allocated cache key (caller must free)
 */
char* generate_collection_cache_key(const char* collection);

/**
 * Store a query result in the cache
 * 
 * @param db Database instance
 * @param collection_name Collection name
 * @param query_json Query that generated the result
 * @param result Result to cache
 * @param ttl Time to live in seconds (0 for default)
 * @return 1 on success, 0 on failure
 */
int store_query_result(database_t* db, const char* collection_name,
                      json_value_t* query_json, json_value_t* result, time_t ttl);

/**
 * Process cache invalidations for collections
 * 
 * @param db Database instance
 * @return Number of collections processed
 */
int process_cache_invalidations(database_t* db);

#endif /* JDBX_CACHE_HELPERS_H */