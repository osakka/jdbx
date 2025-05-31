/**
 * Optimized Database Operations
 * 
 * This file provides optimized implementations of database operations
 * with improved locking, reduced cloning operations, and better performance.
 */

#include "database/database.h"
#include "utils/logger.h"
#include "database/document_index.h"
#include "query/query_language.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/**
 * Generate a UUID for document IDs
 */
static char* generate_uuid() {
  static int counter = 0;
  char* uuid = (char*)malloc(37); /* 36 chars + null */
  if (!uuid) return NULL;
  
  time_t now = time(NULL);
  unsigned int random_part = (unsigned int)rand();
  counter++;
  
  snprintf(uuid, 37, "%08lx-%04x-%04x-%04x-%08x%04x",
       now, (unsigned short)(random_part & 0xFFFF),
       (unsigned short)((random_part >> 16) & 0xFFFF),
       (unsigned short)(counter & 0xFFFF),
       (unsigned int)clock(), (unsigned short)(counter >> 16));
  
  return uuid;
}

/**
 * Find a specific collection in the database
 * 
 * Uses read-only lock to minimize contention
 */
json_value_t* find_collection(database_t* db, const char* collection_name) {
  if (!db || !collection_name) {
    return NULL;
  }
  
  // Use a read lock for this operation
  pthread_mutex_lock(&db->lock);
  
  // Find the collection
  json_value_t* collection = json_object_get(db->collections, collection_name);
  if (!collection || collection->type != JSON_ARRAY) {
    pthread_mutex_unlock(&db->lock);
    return NULL;
  }
  
  // Return a reference to the collection (not a clone)
  pthread_mutex_unlock(&db->lock);
  return collection;
}

/**
 * Optimized implementation for listing collections
 * 
 * Minimizes the lock duration and reduces object cloning
 */
json_value_t* optimized_db_list_collections(database_t* db) {
  if (!db) {
    LOG_ERROR("list collections: NULL database");
    return NULL;
  }
  
  LOG_INFO("Listing collections with optimized implementation");
  
  // Create array for collection names
  json_value_t* result = json_create_array();
  if (!result) {
    LOG_ERROR("create result array for collections");
    return NULL;
  }
  
  // Use a read lock to minimize contention
  pthread_mutex_lock(&db->lock);
  
  // Pre-allocate memory for collection names to minimize time with lock held
  char** collection_names = NULL;
  size_t collection_count = db->collections->value.object.size;
  
  if (collection_count > 0) {
    collection_names = (char**)malloc(collection_count * sizeof(char*));
    if (!collection_names) {
      pthread_mutex_unlock(&db->lock);
      json_free(result);
      LOG_ERROR("Out of memory");
      return NULL;
    }
    
    // Get all collection names while holding the lock
    for (size_t i = 0; i < collection_count; i++) {
      collection_names[i] = strdup(db->collections->value.object.entries[i].key);
    }
  }
  
  // Release the lock as quickly as possible
  pthread_mutex_unlock(&db->lock);
  
  // Now build the result array without holding the lock
  if (collection_names) {
    for (size_t i = 0; i < collection_count; i++) {
      if (collection_names[i]) {
        json_value_t* name = json_create_string(collection_names[i]);
        if (name) {
          json_array_append(result, name);
        }
        free(collection_names[i]);
      }
    }
    free(collection_names);
  }
  
  LOG_INFO("Successfully listed %zu collections", json_array_size(result));
  return result;
}

/**
 * Optimized implementation for querying documents
 * 
 * Minimizes lock duration and improves performance for large document sets
 */
json_value_t* optimized_db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json) {
  LOG_INFO("Collection '%s'" ? collection_name : "NULL");
  
  if (!db || !collection_name) {
    LOG_ERROR("Invalid parameters");
    return NULL;
  }

  // Create empty query if not provided
  int empty_query = 0;
  if (!query_json) {
    LOG_DEBUG("Empty query");
    query_json = json_create_object();
    empty_query = 1;
  }
  
  // Parse query outside of lock
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
  
  // Get collection with read lock
  json_value_t* collection = NULL;
  pthread_mutex_lock(&db->lock);
  
  // Find the collection
  collection = json_object_get(db->collections, collection_name);
  if (!collection || collection->type != JSON_ARRAY) {
    LOG_ERROR("Collection not found: %s", collection_name);
    pthread_mutex_unlock(&db->lock);
    query_free_parse_result(&query_result);
    if (empty_query) {
      json_free(query_json);
    }
    return NULL;
  }
  
  // Create a shallow copy of the array
  size_t doc_count = collection->value.array.size;
  json_value_t** doc_refs = NULL;
  
  if (doc_count > 0) {
    doc_refs = (json_value_t**)malloc(doc_count * sizeof(json_value_t*));
    if (!doc_refs) {
      LOG_ERROR("Out of memory");
      pthread_mutex_unlock(&db->lock);
      query_free_parse_result(&query_result);
      if (empty_query) {
        json_free(query_json);
      }
      return NULL;
    }
    
    // Copy document references (not the documents themselves)
    for (size_t i = 0; i < doc_count; i++) {
      doc_refs[i] = collection->value.array.items[i];
    }
  }
  
  // Release the lock as quickly as possible
  pthread_mutex_unlock(&db->lock);
  
  // Create documents array for the query
  json_value_t* documents_copy = json_create_array();
  if (!documents_copy) {
    LOG_ERROR("create documents array");
    free(doc_refs);
    query_free_parse_result(&query_result);
    if (empty_query) {
      json_free(query_json);
    }
    return NULL;
  }
  
  // Clone documents outside of the lock
  if (doc_refs) {
    for (size_t i = 0; i < doc_count; i++) {
      if (doc_refs[i] && doc_refs[i]->type == JSON_OBJECT) {
        json_array_append(documents_copy, json_clone(doc_refs[i]));
      }
    }
    free(doc_refs);
  }
  
  // Execute query with cloned documents
  LOG_DEBUG("Executing query against %zu documents", json_array_size(documents_copy));
  query_result_t execute_result = query_execute(query_result.expr, documents_copy, &query_result.options);
  
  // Free the cloned documents once query execution is complete
  json_free(documents_copy);
  
  // Create response object
  json_value_t* response = json_create_object();
  if (!response) {
    LOG_ERROR("create response object");
    query_free_parse_result(&query_result);
    if (empty_query) {
      json_free(query_json);
    }
    return NULL;
  }
  
  // Add documents to response
  json_object_set(response, "documents", execute_result.documents);
  
  // Add count information
  json_object_set(response, "count", json_create_integer(execute_result.count));
  json_object_set(response, "total_count", json_create_integer(execute_result.total_count));
  
  // Add pagination information if available
  if (execute_result.pagination) {
    json_value_t* pagination_json = query_pagination_to_json(execute_result.pagination);
    if (pagination_json) {
      json_object_set(response, "pagination", pagination_json);
    }
    query_free_pagination_info(execute_result.pagination);
  }
  
  // Free resources
  query_free_parse_result(&query_result);
  if (empty_query) {
    json_free(query_json);
  }
  
  LOG_INFO("Query completed, returning %d documents", execute_result.count);
  return response;
}

/**
 * Optimized implementation for inserting a document
 * 
 * Minimizes lock duration and reduces cloning operations
 */
json_value_t* optimized_db_insert_document(database_t* db, const char* collection_name, json_value_t* document) {
  LOG_INFO("Collection '%s'" ? collection_name : "NULL");
  
  if (!db || !collection_name || !document || document->type != JSON_OBJECT) {
    LOG_ERROR("Invalid parameters");
    return NULL;
  }
  
  // Prepare document with ID outside of lock
  json_value_t* doc_copy = json_clone(document);
  if (!doc_copy) {
    LOG_ERROR("clone document for insertion");
    return NULL;
  }
  
  // Generate ID if needed
  const char* id_str = NULL;
  char* generated_id = NULL;
  
  json_value_t* id = json_object_get(doc_copy, "_id");
  if (!id || id->type != JSON_STRING) {
    generated_id = generate_uuid();
    if (!generated_id) {
      LOG_ERROR("generate UUID for document");
      json_free(doc_copy);
      return NULL;
    }
    
    json_object_set(doc_copy, "_id", json_create_string(generated_id));
    id_str = generated_id;
  } else {
    id_str = id->value.string;
  }
  
  // Create result object outside of lock
  json_value_t* result = json_create_object();
  if (!result) {
    LOG_ERROR("create result object");
    json_free(doc_copy);
    if (generated_id) free(generated_id);
    return NULL;
  }
  
  json_object_set(result, "_id", json_create_string(id_str));
  
  // Find collection and insert document with write lock
  pthread_mutex_lock(&db->lock);
  
  // Get collection
  json_value_t* collection = json_object_get(db->collections, collection_name);
  if (!collection || collection->type != JSON_ARRAY) {
    LOG_ERROR("Collection not found: %s", collection_name);
    pthread_mutex_unlock(&db->lock);
    json_free(result);
    json_free(doc_copy);
    if (generated_id) free(generated_id);
    return NULL;
  }
  
  // Add document to collection
  json_array_append(collection, doc_copy);
  db->is_modified = 1;
  
  pthread_mutex_unlock(&db->lock);
  
  if (generated_id) free(generated_id);
  
  LOG_INFO("Inserted: %s", id_str);
  return result;
}

/**
 * Optimized implementation for getting a document by ID
 * 
 * Uses optimized locking and reduces cloning operations
 */
json_value_t* optimized_db_get_document(database_t* db, const char* collection_name, const char* id) {
  LOG_INFO("Collection '%s', ID '%s'", 
       collection_name ? collection_name : "NULL", 
       id ? id : "NULL");
  
  if (!db || !collection_name || !id) {
    LOG_ERROR("Invalid parameters");
    return NULL;
  }
  
  // Get collection with read lock
  json_value_t* document = NULL;
  pthread_mutex_lock(&db->lock);
  
  // Find collection
  json_value_t* collection = json_object_get(db->collections, collection_name);
  if (!collection || collection->type != JSON_ARRAY) {
    LOG_ERROR("Collection not found: %s", collection_name);
    pthread_mutex_unlock(&db->lock);
    return NULL;
  }
  
  // Search for document by ID
  for (size_t i = 0; i < collection->value.array.size; i++) {
    json_value_t* doc = collection->value.array.items[i];
    if (doc && doc->type == JSON_OBJECT) {
      json_value_t* doc_id = json_object_get(doc, "_id");
      if (doc_id && doc_id->type == JSON_STRING &&
        strcmp(doc_id->value.string, id) == 0) {
        
        // Found the document - clone it outside the lock
        document = json_clone(doc);
        break;
      }
    }
  }
  
  pthread_mutex_unlock(&db->lock);
  
  if (document) {
    LOG_INFO("Document retrieved");
  } else {
    LOG_WARNING("Document with ID '%s' not found in collection '%s'", id, collection_name);
  }
  
  return document;
}

/**
 * Create a collection with less locking
 */
int optimized_db_create_collection(database_t* db, const char* name) {
  if (!db || !name) {
    return 0;
  }
  
  // Create new collection object outside lock
  json_value_t* collection = json_create_array();
  if (!collection) {
    return 0;
  }
  
  pthread_mutex_lock(&db->lock);

  // Check if collection already exists
  if (json_object_has(db->collections, name)) {
    json_free(collection);
    pthread_mutex_unlock(&db->lock);
    return 0;
  }

  // Add collection to database
  json_object_set(db->collections, name, collection);
  db->is_modified = 1;

  pthread_mutex_unlock(&db->lock);
  
  return 1;
}