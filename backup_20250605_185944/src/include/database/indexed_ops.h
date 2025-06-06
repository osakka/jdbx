/**
 * @file indexed_ops.h
 * @brief Indexed document operations with improved performance
 * 
 * This header defines indexed document operations with fast lookups
 * and optimized performance for large collections.
 */

#ifndef JSONDB_INDEXED_OPS_H
#define JSONDB_INDEXED_OPS_H

#include "database/database.h"

/**
 * @brief Insert a document with indexing for fast lookups
 * 
 * This implementation uses indexed lookups for improved performance.
 * 
 * @param db Database instance
 * @param collection_name Collection name
 * @param document Document to insert
 * @return Result object with ID or NULL on error
 */
json_value_t* indexed_db_insert_document(database_t* db, const char* collection_name, json_value_t* document);

/**
 * @brief Get a document with indexing for fast lookups
 * 
 * This implementation uses indexed lookups for improved performance.
 * 
 * @param db Database instance
 * @param collection_name Collection name
 * @param id Document ID
 * @return Document object or NULL if not found or error
 */
json_value_t* indexed_db_get_document(database_t* db, const char* collection_name, const char* id);

/**
 * @brief Query documents with indexing for fast lookups
 * 
 * This implementation uses indexed lookups for improved performance.
 * 
 * @param db Database instance
 * @param collection_name Collection name
 * @param query_json Query JSON or NULL for all documents
 * @return Query result object or NULL on error
 */
json_value_t* indexed_db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json);

/**
 * @brief Rebuild all indices
 * 
 * This function rebuilds all indices for improved performance.
 * 
 * @param db Database instance
 * @return 1 on success, 0 on failure
 */
int rebuild_all_indices(database_t* db);

#endif /* JSONDB_INDEXED_OPS_H */