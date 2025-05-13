#include "components/database/database.h"
#include "components/utils/logger.h"
#include "simplified_ops.h"

/**
 * This file replaces the complex database operations with simplified, 
 * deadlock-free implementations that don't use caching.
 */

/* Redirect database operations to simplified versions */

/**
 * Insert document wrapper to use simplified implementation
 */
json_value_t* db_insert_document(database_t* db, const char* collection_name, json_value_t* document) {
    LOG_DEBUG("Using simplified db_insert_document implementation");
    return simplified_db_insert_document(db, collection_name, document);
}

/**
 * Get document wrapper to use simplified implementation
 */
json_value_t* db_get_document(database_t* db, const char* collection_name, const char* id) {
    LOG_DEBUG("Using simplified db_get_document implementation");
    return simplified_db_get_document(db, collection_name, id);
}

/**
 * Update document wrapper to use simplified implementation
 */
json_value_t* db_update_document(database_t* db, const char* collection_name, const char* id,
                                json_value_t* document) {
    LOG_DEBUG("Using simplified db_update_document implementation");
    return simplified_db_update_document(db, collection_name, id, document);
}

/**
 * Delete document wrapper to use simplified implementation
 */
int db_delete_document(database_t* db, const char* collection_name, const char* id) {
    LOG_DEBUG("Using simplified db_delete_document implementation");
    return simplified_db_delete_document(db, collection_name, id);
}

/**
 * Query documents wrapper to use simplified implementation
 */
json_value_t* db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json) {
    LOG_DEBUG("Using simplified db_query_documents implementation");
    return simplified_db_query_documents(db, collection_name, query_json);
}