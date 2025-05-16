/**
 * @file optimized_ops.h
 * @brief Optimized database operations with improved locking and performance
 * 
 * This header defines optimized implementations of database operations
 * that minimize lock contention and improve performance.
 */

#ifndef JSONDB_OPTIMIZED_OPS_H
#define JSONDB_OPTIMIZED_OPS_H

#include "database/database.h"

/**
 * @brief List collections with optimized locking
 * 
 * This implementation minimizes the lock duration and object cloning.
 * 
 * @param db Database instance
 * @return A JSON array of collection names or NULL on error
 */
json_value_t* optimized_db_list_collections(database_t* db);

/**
 * @brief Query documents with optimized performance
 * 
 * This implementation minimizes lock duration and improves performance for large document sets.
 * 
 * @param db Database instance
 * @param collection_name Collection name
 * @param query_json Query JSON or NULL for all documents
 * @return Query result object or NULL on error
 */
json_value_t* optimized_db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json);

/**
 * @brief Insert a document with optimized locking
 * 
 * This implementation minimizes lock duration and reduces unnecessary cloning.
 * 
 * @param db Database instance
 * @param collection_name Collection name
 * @param document Document to insert
 * @return Result object with ID or NULL on error
 */
json_value_t* optimized_db_insert_document(database_t* db, const char* collection_name, json_value_t* document);

/**
 * @brief Get a document by ID with optimized locking
 * 
 * This implementation minimizes lock duration.
 * 
 * @param db Database instance
 * @param collection_name Collection name
 * @param id Document ID
 * @return Document object or NULL if not found or error
 */
json_value_t* optimized_db_get_document(database_t* db, const char* collection_name, const char* id);

/**
 * @brief Create a collection with optimized locking
 * 
 * This implementation minimizes lock duration.
 * 
 * @param db Database instance
 * @param name Collection name
 * @return 1 on success, 0 on failure
 */
int optimized_db_create_collection(database_t* db, const char* name);

#endif /* JSONDB_OPTIMIZED_OPS_H */