/**
 * Simplified Implementations for Database Operations
 * These implementations are designed to be thread-safe and efficient
 * without relying on complex caching mechanisms that might cause deadlocks.
 */

#include "database/database.h"
#include "utils/logger.h"
#include "query/query_language.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/**
 * Simplified Implementation for Inserting a Document
 * 
 * This implementation avoids cache operations entirely and uses
 * a single lock for thread safety. It performs all operations
 * in a clear, step-by-step manner with detailed logging.
 */
json_value_t* simplified_db_insert_document(database_t* db, const char* collection_name, json_value_t* document) {
    LOG_INFO("Starting document insertion for collection '%s'", collection_name ? collection_name : "NULL");
    
    if (!db || !collection_name || !document || document->type != JSON_OBJECT) {
        LOG_ERROR("Invalid parameters for db_insert_document");
        return NULL;
    }
    
    LOG_DEBUG("Cloning document for insertion");
    json_value_t* doc_copy = json_clone(document);
    if (!doc_copy) {
        LOG_ERROR("Failed to clone document for insertion");
        return NULL;
    }
    
    LOG_DEBUG("Generating document ID if needed");
    if (!json_object_has(doc_copy, "_id")) {
        char* id = generate_uuid();
        if (id) {
            LOG_DEBUG("Generated new UUID: %s", id);
            json_object_set(doc_copy, "_id", json_create_string(id));
            free(id);
        } else {
            LOG_ERROR("Failed to generate UUID for document");
            json_free(doc_copy);
            return NULL;
        }
    }
    
    LOG_DEBUG("Verifying document has valid ID");
    json_value_t* id = json_object_get(doc_copy, "_id");
    if (!id || id->type != JSON_STRING) {
        LOG_ERROR("Document has no valid ID after preparation");
        json_free(doc_copy);
        return NULL;
    }
    const char* id_str = id->value.string;
    
    LOG_DEBUG("Creating result object with ID: %s", id_str);
    json_value_t* result = json_create_object();
    if (!result) {
        LOG_ERROR("Failed to create result object");
        json_free(doc_copy);
        return NULL;
    }
    json_object_set(result, "_id", json_create_string(id_str));
    
    LOG_DEBUG("Acquiring database lock");
    pthread_mutex_lock(&db->lock);
    
    LOG_DEBUG("Getting collection: %s", collection_name);
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        LOG_ERROR("Collection '%s' not found or not an array", collection_name);
        pthread_mutex_unlock(&db->lock);
        json_free(result);
        json_free(doc_copy);
        return NULL;
    }
    
    LOG_DEBUG("Adding document to collection");
    json_array_append(collection, doc_copy);
    db->is_modified = 1;
    
    LOG_DEBUG("Releasing database lock");
    pthread_mutex_unlock(&db->lock);
    
    LOG_INFO("Document inserted successfully with ID: %s", id_str);
    
    /* No cache operations performed */
    
    return result;
}

/**
 * Simplified Implementation for Document Query
 * 
 * This implementation avoids cache operations entirely and uses
 * a single lock for thread safety. It creates a copy of all needed
 * data before releasing the lock to prevent deadlocks.
 */
json_value_t* simplified_db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json) {
    LOG_INFO("Starting document query for collection '%s'", collection_name ? collection_name : "NULL");
    
    if (!db || !collection_name) {
        LOG_ERROR("Invalid parameters for db_query_documents");
        return NULL;
    }
    
    /* Create query object if not provided */
    int empty_query = 0;
    if (!query_json) {
        LOG_DEBUG("Creating empty query object");
        query_json = json_create_object();
        empty_query = 1;
    }
    
    /* Parse the query */
    LOG_DEBUG("Parsing query");
    query_parse_result_t query_result = query_parse(query_json);
    if (query_result.error) {
        LOG_ERROR("Query parse error: %s", query_result.error);
        query_free_parse_result(&query_result);
        if (empty_query) {
            json_free(query_json);
        }
        return NULL;
    }
    
    LOG_DEBUG("Acquiring database lock");
    pthread_mutex_lock(&db->lock);
    
    LOG_DEBUG("Getting collection: %s", collection_name);
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        LOG_ERROR("Collection '%s' not found or not an array", collection_name);
        pthread_mutex_unlock(&db->lock);
        query_free_parse_result(&query_result);
        if (empty_query) {
            json_free(query_json);
        }
        return NULL;
    }
    
    LOG_DEBUG("Creating copy of collection documents");
    json_value_t* documents_copy = json_create_array();
    if (!documents_copy) {
        LOG_ERROR("Failed to create documents copy");
        pthread_mutex_unlock(&db->lock);
        query_free_parse_result(&query_result);
        if (empty_query) {
            json_free(query_json);
        }
        return NULL;
    }
    
    LOG_DEBUG("Cloning all documents in collection");
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc && doc->type == JSON_OBJECT) {
            json_array_append(documents_copy, json_clone(doc));
        }
    }
    
    LOG_DEBUG("Releasing database lock");
    pthread_mutex_unlock(&db->lock);
    
    LOG_DEBUG("Executing query against documents");
    query_result_t execute_result = query_execute(query_result.expr, documents_copy, &query_result.options);
    
    LOG_DEBUG("Query execution complete, freeing document copy");
    json_free(documents_copy);
    
    LOG_DEBUG("Creating response object");
    json_value_t* response = json_create_object();
    if (!response) {
        LOG_ERROR("Failed to create response object");
        query_free_parse_result(&query_result);
        if (empty_query) {
            json_free(query_json);
        }
        return NULL;
    }
    
    LOG_DEBUG("Adding documents to response");
    json_object_set(response, "documents", execute_result.documents);
    
    LOG_DEBUG("Adding count information");
    json_object_set(response, "count", json_create_integer(execute_result.count));
    json_object_set(response, "total_count", json_create_integer(execute_result.total_count));
    
    LOG_DEBUG("Adding pagination information if available");
    if (execute_result.pagination) {
        json_value_t* pagination_json = query_pagination_to_json(execute_result.pagination);
        if (pagination_json) {
            json_object_set(response, "pagination", pagination_json);
        }
        query_free_pagination_info(execute_result.pagination);
    }
    
    LOG_DEBUG("Freeing query parse result");
    query_free_parse_result(&query_result);
    
    if (empty_query) {
        LOG_DEBUG("Freeing empty query object");
        json_free(query_json);
    }
    
    LOG_INFO("Query completed successfully");
    
    return response;
}

/**
 * Simplified Implementation for Document Update
 * 
 * This implementation avoids cache operations entirely and uses
 * a single lock for thread safety.
 */
json_value_t* simplified_db_update_document(database_t* db, const char* collection_name, const char* id,
                                         json_value_t* document) {
    LOG_INFO("Starting document update for collection '%s', ID '%s'", 
             collection_name ? collection_name : "NULL", 
             id ? id : "NULL");
    
    if (!db || !collection_name || !id || !document || document->type != JSON_OBJECT) {
        LOG_ERROR("Invalid parameters for db_update_document");
        return NULL;
    }
    
    LOG_DEBUG("Cloning document for update");
    json_value_t* doc_copy = json_clone(document);
    if (!doc_copy) {
        LOG_ERROR("Failed to clone document for update");
        return NULL;
    }
    
    LOG_DEBUG("Adding ID to document: %s", id);
    json_object_set(doc_copy, "_id", json_create_string(id));
    
    LOG_DEBUG("Creating result object");
    json_value_t* result = json_create_object();
    if (!result) {
        LOG_ERROR("Failed to create result object");
        json_free(doc_copy);
        return NULL;
    }
    json_object_set(result, "_id", json_create_string(id));
    
    LOG_DEBUG("Acquiring database lock");
    pthread_mutex_lock(&db->lock);
    
    LOG_DEBUG("Getting collection: %s", collection_name);
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        LOG_ERROR("Collection '%s' not found or not an array", collection_name);
        pthread_mutex_unlock(&db->lock);
        json_free(result);
        json_free(doc_copy);
        return NULL;
    }
    
    LOG_DEBUG("Finding document by ID: %s", id);
    int found = 0;
    
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc && doc->type == JSON_OBJECT) {
            json_value_t* doc_id = json_object_get(doc, "_id");
            
            if (doc_id && doc_id->type == JSON_STRING &&
                strcmp(doc_id->value.string, id) == 0) {
                
                LOG_DEBUG("Document found, replacing with updated version");
                json_free(doc);
                collection->value.array.items[i] = doc_copy;
                
                found = 1;
                db->is_modified = 1;
                break;
            }
        }
    }
    
    LOG_DEBUG("Releasing database lock");
    pthread_mutex_unlock(&db->lock);
    
    if (!found) {
        LOG_ERROR("Document with ID '%s' not found in collection '%s'", id, collection_name);
        json_free(doc_copy);
        json_free(result);
        return NULL;
    }
    
    LOG_INFO("Document updated successfully");
    
    return result;
}

/**
 * Simplified Implementation for Document Deletion
 * 
 * This implementation avoids cache operations entirely and uses
 * a single lock for thread safety.
 */
int simplified_db_delete_document(database_t* db, const char* collection_name, const char* id) {
    LOG_INFO("Starting document deletion for collection '%s', ID '%s'", 
             collection_name ? collection_name : "NULL", 
             id ? id : "NULL");
    
    if (!db || !collection_name || !id) {
        LOG_ERROR("Invalid parameters for db_delete_document");
        return 0;
    }
    
    LOG_DEBUG("Acquiring database lock");
    pthread_mutex_lock(&db->lock);
    
    LOG_DEBUG("Getting collection: %s", collection_name);
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        LOG_ERROR("Collection '%s' not found or not an array", collection_name);
        pthread_mutex_unlock(&db->lock);
        return 0;
    }
    
    LOG_DEBUG("Finding document by ID: %s", id);
    int found = 0;
    
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc && doc->type == JSON_OBJECT) {
            json_value_t* doc_id = json_object_get(doc, "_id");
            if (doc_id && doc_id->type == JSON_STRING &&
                strcmp(doc_id->value.string, id) == 0) {
                
                LOG_DEBUG("Document found, removing from collection");
                json_free(doc);
                
                /* Move remaining documents */
                for (size_t j = i; j < collection->value.array.size - 1; j++) {
                    collection->value.array.items[j] = collection->value.array.items[j + 1];
                }
                
                collection->value.array.size--;
                
                found = 1;
                db->is_modified = 1;
                break;
            }
        }
    }
    
    LOG_DEBUG("Releasing database lock");
    pthread_mutex_unlock(&db->lock);
    
    if (found) {
        LOG_INFO("Document deleted successfully");
    } else {
        LOG_WARNING("Document with ID '%s' not found in collection '%s'", id, collection_name);
    }
    
    return found;
}

/**
 * Simplified Implementation for Getting a Document by ID
 * 
 * This implementation avoids cache operations entirely and uses
 * a single lock for thread safety.
 */
json_value_t* simplified_db_get_document(database_t* db, const char* collection_name, const char* id) {
    LOG_INFO("Starting document retrieval for collection '%s', ID '%s'", 
             collection_name ? collection_name : "NULL", 
             id ? id : "NULL");
    
    if (!db || !collection_name || !id) {
        LOG_ERROR("Invalid parameters for db_get_document");
        return NULL;
    }
    
    LOG_DEBUG("Acquiring database lock");
    pthread_mutex_lock(&db->lock);
    
    LOG_DEBUG("Getting collection: %s", collection_name);
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        LOG_ERROR("Collection '%s' not found or not an array", collection_name);
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }
    
    LOG_DEBUG("Finding document by ID: %s", id);
    json_value_t* document = NULL;
    
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc && doc->type == JSON_OBJECT) {
            json_value_t* doc_id = json_object_get(doc, "_id");
            if (doc_id && doc_id->type == JSON_STRING &&
                strcmp(doc_id->value.string, id) == 0) {
                
                LOG_DEBUG("Document found, creating clone");
                document = json_clone(doc);
                break;
            }
        }
    }
    
    LOG_DEBUG("Releasing database lock");
    pthread_mutex_unlock(&db->lock);
    
    if (document) {
        LOG_INFO("Document retrieved successfully");
    } else {
        LOG_WARNING("Document with ID '%s' not found in collection '%s'", id, collection_name);
    }
    
    return document;
}