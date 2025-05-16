#include "database/database.h"
#include "utils/logger.h"
#include "database/simplified_ops.h"
#include "database/optimized_ops.h"
#include "database/indexed_ops.h"

/**
 * This file replaces the complex database operations with optimized, 
 * deadlock-free, indexed implementations that provide better performance.
 */

/* Redirect database operations to indexed versions for documents and optimized versions for collections */

/**
 * Insert document wrapper to use indexed implementation
 */
json_value_t* db_insert_document(database_t* db, const char* collection_name, json_value_t* document) {
    LOG_DEBUG("Using indexed db_insert_document implementation");
    return indexed_db_insert_document(db, collection_name, document);
}

/**
 * Get document wrapper to use indexed implementation
 */
json_value_t* db_get_document(database_t* db, const char* collection_name, const char* id) {
    LOG_DEBUG("Using indexed db_get_document implementation");
    return indexed_db_get_document(db, collection_name, id);
}

/**
 * Update document wrapper to use simplified implementation
 * TODO: Implement indexed version of update
 */
json_value_t* db_update_document(database_t* db, const char* collection_name, const char* id,
                                json_value_t* document) {
    LOG_DEBUG("Using simplified db_update_document implementation");
    return simplified_db_update_document(db, collection_name, id, document);
}

/**
 * Delete document wrapper to use simplified implementation
 * TODO: Implement indexed version of delete
 */
int db_delete_document(database_t* db, const char* collection_name, const char* id) {
    LOG_DEBUG("Using simplified db_delete_document implementation");
    return simplified_db_delete_document(db, collection_name, id);
}

/**
 * Query documents wrapper to use indexed implementation
 */
json_value_t* db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json) {
    LOG_DEBUG("Using indexed db_query_documents implementation");
    return indexed_db_query_documents(db, collection_name, query_json);
}

/**
 * List collections wrapper to use optimized implementation
 */
json_value_t* db_list_collections(database_t* db) {
    LOG_DEBUG("Using optimized db_list_collections implementation");
    return optimized_db_list_collections(db);
}

/**
 * Create collection wrapper to use optimized implementation
 */
int db_create_collection(database_t* db, const char* name) {
    LOG_DEBUG("Using optimized db_create_collection implementation");
    return optimized_db_create_collection(db, name);
}

/**
 * Rebuild indices for all collections
 */
int db_rebuild_indices(database_t* db) {
    LOG_DEBUG("Rebuilding all indices");
    return rebuild_all_indices(db);
}