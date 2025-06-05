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
#include <time.h>

/**
 * Generate a UUID for document IDs
 * This is a simple implementation that generates a reasonably unique ID
 * Not a proper RFC-compliant UUID but sufficient for our needs
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
 * Simplified Implementation for Inserting a Document
 * 
 * This implementation avoids cache operations entirely and uses
 * a single lock for thread safety. It performs all operations
 * in a clear, step-by-step manner with detailed logging.
 */
json_value_t* db_insert_document(database_t* db, const char* collection_name, json_value_t* document) {
  LOG_INFO("Collection '%s'", collection_name ? collection_name : "NULL");
  
  if (!db || !collection_name || !document || document->type != JSON_OBJECT) {
    LOG_ERROR("Invalid parameters");
    return NULL;
  }
  
  LOG_DEBUG("Cloning document");
  json_value_t* doc_copy = json_clone(document);
  if (!doc_copy) {
    LOG_ERROR("clone document for insertion");
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
      LOG_ERROR("generate UUID for document");
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
    LOG_ERROR("create result object");
    json_free(doc_copy);
    return NULL;
  }
  json_object_set(result, "_id", json_create_string(id_str));
  
  LOG_DEBUG("Acquiring lock");
  pthread_mutex_lock(&db->lock);
  
  LOG_DEBUG("Collection: %s", collection_name);
  json_value_t* collection = json_object_get(db->collections, collection_name);
  if (!collection || collection->type != JSON_ARRAY) {
    LOG_ERROR("Collection not found: %s", collection_name);
    pthread_mutex_unlock(&db->lock);
    json_free(result);
    json_free(doc_copy);
    return NULL;
  }
  
  LOG_DEBUG("Adding document to collection");
  json_array_append(collection, doc_copy);
  db->is_modified = 1;
  
  LOG_DEBUG("Releasing lock");
  pthread_mutex_unlock(&db->lock);
  
  LOG_INFO("Inserted: %s", id_str);
  
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
json_value_t* db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json) {
  LOG_INFO("Collection '%s'" ? collection_name : "NULL");
  
  if (!db || !collection_name) {
    LOG_ERROR("Invalid parameters");
    return NULL;
  }
  
  /* Create query object if not provided */
  int empty_query = 0;
  if (!query_json) {
    LOG_DEBUG("Empty query");
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
  
  LOG_DEBUG("Acquiring lock");
  pthread_mutex_lock(&db->lock);
  
  LOG_DEBUG("Collection: %s", collection_name);
  json_value_t* collection = json_object_get(db->collections, collection_name);
  if (!collection || collection->type != JSON_ARRAY) {
    LOG_ERROR("Collection not found: %s", collection_name);
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
    LOG_ERROR("create documents copy");
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
  
  LOG_DEBUG("Releasing lock");
  pthread_mutex_unlock(&db->lock);
  
  LOG_DEBUG("Executing query against documents");
  query_result_t execute_result = query_execute(query_result.expr, documents_copy, &query_result.options);
  
  LOG_DEBUG("Query execution complete, freeing document copy");
  json_free(documents_copy);
  
  LOG_DEBUG("Creating response object");
  json_value_t* response = json_create_object();
  if (!response) {
    LOG_ERROR("create response object");
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
  
  LOG_INFO("Query completed");
  
  return response;
}

/**
 * Simplified Implementation for Document Update
 * 
 * This implementation avoids cache operations entirely and uses
 * a single lock for thread safety.
 */
json_value_t* db_update_document(database_t* db, const char* collection_name, const char* id,
                     json_value_t* document) {
  LOG_INFO("Collection '%s', ID '%s'", 
       collection_name ? collection_name : "NULL", 
       id ? id : "NULL");
  
  if (!db || !collection_name || !id || !document || document->type != JSON_OBJECT) {
    LOG_ERROR("Invalid parameters");
    return NULL;
  }
  
  LOG_DEBUG("Cloning document");
  json_value_t* doc_copy = json_clone(document);
  if (!doc_copy) {
    LOG_ERROR("clone document for update");
    return NULL;
  }
  
  LOG_DEBUG("Adding ID to document: %s", id);
  json_object_set(doc_copy, "_id", json_create_string(id));
  
  LOG_DEBUG("Creating result object");
  json_value_t* result = json_create_object();
  if (!result) {
    LOG_ERROR("create result object");
    json_free(doc_copy);
    return NULL;
  }
  json_object_set(result, "_id", json_create_string(id));
  
  LOG_DEBUG("Acquiring lock");
  pthread_mutex_lock(&db->lock);
  
  LOG_DEBUG("Collection: %s", collection_name);
  json_value_t* collection = json_object_get(db->collections, collection_name);
  if (!collection || collection->type != JSON_ARRAY) {
    LOG_ERROR("Collection not found: %s", collection_name);
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
  
  LOG_DEBUG("Releasing lock");
  pthread_mutex_unlock(&db->lock);
  
  if (!found) {
    LOG_ERROR("Document with ID '%s' not found in collection '%s'", id, collection_name);
    json_free(doc_copy);
    json_free(result);
    return NULL;
  }
  
  LOG_INFO("Updated");
  
  return result;
}

/**
 * Enhanced Implementation for Document Deletion with Crash Detection
 * 
 * This implementation includes comprehensive logging and safety checks
 * to investigate crashes when deleting documents.
 */
int db_delete_document(database_t* db, const char* collection_name, const char* id) {
  /* Get thread information for debugging */
  pthread_t thread_id = pthread_self();
  
  if (g_logger) {
    LOG_INFO("DELETE_START: collection='%s', id='%s', thread=%lu", 
         collection_name ? collection_name : "NULL", 
         id ? id : "NULL", (unsigned long)thread_id);
    LOG_TRACE("DELETE_PARAMS: db=%p, db->collections=%p, db->lock=%p", 
         (void*)db, db ? (void*)db->collections : NULL, 
         db ? (void*)&db->lock : NULL);
  }
  
  if (!db || !collection_name || !id) {
    if (g_logger) {
      LOG_ERROR("DELETE_FAILED: Invalid parameters - db=%p, collection_name=%p, id=%p", 
           (void*)db, (void*)collection_name, (void*)id);
    }
    return 0;
  }
  
  /* Validate database structure before proceeding */
  if (!db->collections) {
    if (g_logger) {
      LOG_ERROR("DELETE_FAILED: Database collections is NULL - db=%p", (void*)db);
    }
    return 0;
  }
  
  /* Pre-lock validation */
  if (g_logger) {
    LOG_TRACE("DELETE_PRE_LOCK: attempting to acquire database lock, thread=%lu", 
         (unsigned long)thread_id);
  }
  
  /* Acquire lock with enhanced error handling */
  int lock_result = pthread_mutex_lock(&db->lock);
  if (lock_result != 0) {
    if (g_logger) {
      LOG_ERROR("DELETE_FAILED: Unable to acquire database lock - error=%d, thread=%lu", 
           lock_result, (unsigned long)thread_id);
    }
    return 0;
  }
  
  if (g_logger) {
    LOG_TRACE("DELETE_LOCK_ACQUIRED: database lock acquired successfully, thread=%lu", 
         (unsigned long)thread_id);
  }
  
  /* Validate collections structure under lock */
  if (!db->collections || db->collections->type != JSON_OBJECT) {
    if (g_logger) {
      LOG_ERROR("DELETE_FAILED: Invalid collections structure - collections=%p, type=%d", 
           (void*)db->collections, db->collections ? (int)db->collections->type : -1);
    }
    pthread_mutex_unlock(&db->lock);
    return 0;
  }
  
  if (g_logger) {
    LOG_DEBUG("DELETE_COLLECTION_LOOKUP: searching for collection '%s'", collection_name);
  }
  
  json_value_t* collection = json_object_get(db->collections, collection_name);
  if (!collection || collection->type != JSON_ARRAY) {
    if (g_logger) {
      LOG_ERROR("DELETE_FAILED: Collection not found or invalid - collection='%s', found=%p, type=%d", 
           collection_name, (void*)collection, collection ? (int)collection->type : -1);
    }
    pthread_mutex_unlock(&db->lock);
    return 0;
  }
  
  size_t collection_size = collection->value.array.size;
  if (g_logger) {
    LOG_DEBUG("DELETE_COLLECTION_FOUND: collection='%s', size=%zu, items=%p", 
         collection_name, collection_size, (void*)collection->value.array.items);
  }
  
  /* Validate collection array structure */
  if (collection_size > 0 && !collection->value.array.items) {
    if (g_logger) {
      LOG_ERROR("DELETE_FAILED: Collection has size but no items array - size=%zu, items=%p", 
           collection_size, (void*)collection->value.array.items);
    }
    pthread_mutex_unlock(&db->lock);
    return 0;
  }
  
  if (g_logger) {
    LOG_DEBUG("DELETE_SEARCH_START: searching %zu documents for id='%s'", collection_size, id);
  }
  
  int found = 0;
  size_t found_index = 0;
  
  /* Enhanced document search with bounds checking */
  for (size_t i = 0; i < collection_size; i++) {
    if (g_logger) {
      LOG_TRACE("DELETE_SEARCH_ITEM: checking document %zu/%zu", i + 1, collection_size);
    }
    
    /* Validate array bounds */
    if (i >= collection->value.array.size) {
      if (g_logger) {
        LOG_ERROR("DELETE_FAILED: Array index out of bounds - i=%zu, size=%zu", 
             i, collection->value.array.size);
      }
      break;
    }
    
    json_value_t* doc = collection->value.array.items[i];
    if (!doc) {
      if (g_logger) {
        LOG_TRACE("DELETE_SEARCH_ITEM: document %zu is NULL, skipping", i);
      }
      continue;
    }
    
    if (doc->type != JSON_OBJECT) {
      if (g_logger) {
        LOG_WARNING("DELETE_SEARCH_ITEM: document %zu is not an object (type=%d), skipping", i, doc->type);
      }
      continue;
    }
    
    json_value_t* doc_id = json_object_get(doc, "_id");
    if (!doc_id || doc_id->type != JSON_STRING) {
      if (g_logger) {
        LOG_TRACE("DELETE_SEARCH_ITEM: document %zu has no valid _id field", i);
      }
      continue;
    }
    
    if (strcmp(doc_id->value.string, id) == 0) {
      if (g_logger) {
        LOG_INFO("DELETE_DOCUMENT_FOUND: found document at index %zu, id='%s'", i, id);
      }
      found = 1;
      found_index = i;
      break;
    }
  }
  
  if (!found) {
    if (g_logger) {
      LOG_WARNING("DELETE_NOT_FOUND: document with id='%s' not found in collection '%s'", id, collection_name);
    }
    pthread_mutex_unlock(&db->lock);
    return 0;
  }
  
  /* Enhanced document removal with safety checks */
  if (g_logger) {
    LOG_INFO("DELETE_REMOVING: removing document at index %zu from collection '%s'", found_index, collection_name);
  }
  
  /* Validate the document pointer before freeing */
  json_value_t* doc_to_delete = collection->value.array.items[found_index];
  if (!doc_to_delete) {
    if (g_logger) {
      LOG_ERROR("DELETE_FAILED: Document pointer is NULL at found index %zu", found_index);
    }
    pthread_mutex_unlock(&db->lock);
    return 0;
  }
  
  if (g_logger) {
    LOG_TRACE("DELETE_FREEING: freeing document structure at %p", (void*)doc_to_delete);
  }
  
  /* Free the document with error handling */
  json_free(doc_to_delete);
  
  if (g_logger) {
    LOG_TRACE("DELETE_SHIFTING: shifting remaining documents, from_index=%zu, total_size=%zu", 
         found_index, collection_size);
  }
  
  /* Enhanced array shifting with bounds checking */
  for (size_t j = found_index; j < collection_size - 1; j++) {
    if (j + 1 >= collection->value.array.size) {
      if (g_logger) {
        LOG_ERROR("DELETE_FAILED: Array shift out of bounds - j+1=%zu, size=%zu", 
             j + 1, collection->value.array.size);
      }
      break;
    }
    collection->value.array.items[j] = collection->value.array.items[j + 1];
  }
  
  /* Update collection size with validation */
  if (collection->value.array.size == 0) {
    if (g_logger) {
      LOG_ERROR("DELETE_FAILED: Cannot decrease array size from 0");
    }
    pthread_mutex_unlock(&db->lock);
    return 0;
  }
  
  collection->value.array.size--;
  
  /* Set null terminator in the array to prevent access to freed memory */
  if (collection->value.array.size < collection_size) {
    collection->value.array.items[collection->value.array.size] = NULL;
  }
  
  /* Mark database as modified */
  db->is_modified = 1;
  
  if (g_logger) {
    LOG_INFO("DELETE_SUCCESS: document removed, new_collection_size=%zu", collection->value.array.size);
    LOG_TRACE("DELETE_RELEASING_LOCK: releasing database lock, thread=%lu", (unsigned long)thread_id);
  }
  
  /* Release lock */
  int unlock_result = pthread_mutex_unlock(&db->lock);
  if (unlock_result != 0) {
    if (g_logger) {
      LOG_ERROR("DELETE_WARNING: Failed to release database lock - error=%d, thread=%lu", 
           unlock_result, (unsigned long)thread_id);
    }
  }
  
  if (g_logger) {
    LOG_INFO("DELETE_COMPLETE: successfully deleted document id='%s' from collection '%s', thread=%lu", 
         id, collection_name, (unsigned long)thread_id);
  }
  
  return 1;
}

/**
 * Simplified Implementation for Getting a Document by ID
 * 
 * This implementation avoids cache operations entirely and uses
 * a single lock for thread safety.
 */
json_value_t* get_document(database_t* db, const char* collection_name, const char* id) {
  LOG_INFO("Collection '%s', ID '%s'", 
       collection_name ? collection_name : "NULL", 
       id ? id : "NULL");
  
  if (!db || !collection_name || !id) {
    LOG_ERROR("Invalid parameters");
    return NULL;
  }
  
  LOG_DEBUG("Acquiring lock");
  pthread_mutex_lock(&db->lock);
  
  LOG_DEBUG("Collection: %s", collection_name);
  json_value_t* collection = json_object_get(db->collections, collection_name);
  if (!collection || collection->type != JSON_ARRAY) {
    LOG_ERROR("Collection not found: %s", collection_name);
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
  
  LOG_DEBUG("Releasing lock");
  pthread_mutex_unlock(&db->lock);
  
  if (document) {
    LOG_INFO("Document retrieved");
  } else {
    LOG_WARNING("Document with ID '%s' not found in collection '%s'", id, collection_name);
  }
  
  return document;
}
