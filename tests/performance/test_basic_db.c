#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "database/database.h"
#include "utils/logger.h"
#include "utils/json.h"
#include "utils/buffer_pool.h"

int main() {
    // Initialize logger
    logger_init("/tmp/test_basic.log", LOG_LEVEL_DEBUG);
    LOG_INFO("Testing basic database operations...");
    
    // Initialize database
    database_t* db = db_init("/tmp/test_basic_db");
    if (!db) {
        LOG_ERROR("Failed to initialize database");
        return 1;
    }
    LOG_INFO("Database initialized");
    
    // Create collection
    int rc = db_create_collection(db, "test");
    if (rc != 0) {
        LOG_ERROR("Failed to create collection");
        return 1;
    }
    LOG_INFO("Collection created");
    
    // Insert a simple document
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Test"));
    json_object_set(doc, "value", json_create_integer(42));
    
    LOG_INFO("Inserting document...");
    json_value_t* result = db_insert_document(db, "test", doc);
    if (!result) {
        LOG_ERROR("Failed to insert document - result is NULL");
        json_free(doc);
        return 1;
    }
    
    // Debug: print the result
    char* result_str = json_stringify(result);
    LOG_INFO("Insert result: %s", result_str ? result_str : "NULL");
    if (result_str) buffer_pool_free_safe(result_str);
    
    // Get the ID
    json_value_t* id_val = json_object_get(result, "_id");
    if (!id_val || id_val->type != JSON_STRING) {
        LOG_ERROR("No _id in result");
        json_free(doc);
        json_free(result);
        return 1;
    }
    
    char* doc_id = strdup(id_val->value.string);
    LOG_INFO("Document inserted with ID: %s", doc_id);
    
    json_free(doc);
    json_free(result);
    
    // Retrieve the document
    LOG_INFO("Retrieving document...");
    json_value_t* retrieved = db_get_document(db, "test", doc_id);
    if (!retrieved) {
        LOG_ERROR("Failed to retrieve document");
        free(doc_id);
        return 1;
    }
    
    char* json_str = json_stringify(retrieved);
    LOG_INFO("Retrieved document: %s", json_str);
    buffer_pool_free_safe(json_str);
    json_free(retrieved);
    
    // Delete the document
    LOG_INFO("Deleting document...");
    rc = db_delete_document(db, "test", doc_id);
    if (rc != 0) {
        LOG_ERROR("Failed to delete document");
        free(doc_id);
        return 1;
    }
    LOG_INFO("Document deleted");
    
    free(doc_id);
    
    // Close database
    db_close(db);
    LOG_INFO("Test completed successfully!");
    
    logger_close();
    return 0;
}