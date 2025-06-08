/**
 * @file document_index.h
 * @brief Functions for working with document indices
 * 
 * This header provides functions for managing document indices,
 * which are used for fast document lookups.
 */

#ifndef JSONDB_DOCUMENT_INDEX_H
#define JSONDB_DOCUMENT_INDEX_H

#include "database/indexed_ops.h"
#include "database/document_index_types.h"

/**
 * Free the global index collection and release all resources
 */
void free_index_collection();

/**
 * Remove a document from an index
 * 
 * @param index Document index
 * @param id Document ID to remove
 * @return 1 on success, 0 if document not found
 */
int remove_from_index(document_index_t* index, const char* id);

/**
 * Find a specific collection in the database
 * 
 * @param db Database instance
 * @param collection_name Collection name to find
 * @return The collection object or NULL if not found
 */
json_value_t* find_collection(database_t* db, const char* collection_name);

#endif /* JSONDB_DOCUMENT_INDEX_H */