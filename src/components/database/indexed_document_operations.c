/**
 * Indexed Document Operations
 * 
 * This file provides optimized implementations for document operations
 * with proper indexing for fast lookups and reduced lock contention.
 */

#include "database/database.h"
#include "utils/logger.h"
#include "query/query_language.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/* Document index structure for fast lookups by ID */
typedef struct {
    char** ids;               /* Array of document IDs */
    int* positions;           /* Array of positions in the collection array */
    size_t capacity;          /* Capacity of the index */
    size_t size;              /* Current size of the index */
    pthread_mutex_t lock;     /* Lock for index operations */
} document_index_t;

/* Collection of indices */
typedef struct {
    document_index_t** indices;   /* Array of indices by collection */
    char** collection_names;      /* Array of collection names */
    size_t capacity;              /* Capacity of the index collection */
    size_t size;                  /* Current size of the index collection */
    pthread_mutex_t lock;         /* Lock for index collection operations */
} index_collection_t;

/* Global index collection */
static index_collection_t* g_index_collection = NULL;

/**
 * Create a new document index
 * 
 * @param initial_capacity Initial capacity of the index
 * @return Newly created index or NULL on error
 */
static document_index_t* create_document_index(size_t initial_capacity) {
    document_index_t* index = (document_index_t*)malloc(sizeof(document_index_t));
    if (!index) {
        LOG_ERROR("Failed to allocate memory for document index");
        return NULL;
    }
    
    index->ids = (char**)malloc(initial_capacity * sizeof(char*));
    if (!index->ids) {
        LOG_ERROR("Failed to allocate memory for document index IDs");
        free(index);
        return NULL;
    }
    
    index->positions = (int*)malloc(initial_capacity * sizeof(int));
    if (!index->positions) {
        LOG_ERROR("Failed to allocate memory for document index positions");
        free(index->ids);
        free(index);
        return NULL;
    }
    
    index->capacity = initial_capacity;
    index->size = 0;
    pthread_mutex_init(&index->lock, NULL);
    
    return index;
}

/**
 * Free a document index
 * 
 * @param index Index to free
 */
static void free_document_index(document_index_t* index) {
    if (!index) {
        return;
    }
    
    pthread_mutex_lock(&index->lock);
    
    if (index->ids) {
        for (size_t i = 0; i < index->size; i++) {
            if (index->ids[i]) {
                free(index->ids[i]);
            }
        }
        free(index->ids);
    }
    
    if (index->positions) {
        free(index->positions);
    }
    
    pthread_mutex_unlock(&index->lock);
    pthread_mutex_destroy(&index->lock);
    
    free(index);
}

/**
 * Initialize the global index collection
 * 
 * @param initial_capacity Initial capacity for the collection
 * @return 1 on success, 0 on failure
 */
static int initialize_index_collection(size_t initial_capacity) {
    if (g_index_collection) {
        LOG_WARNING("Index collection already initialized");
        return 1;
    }
    
    g_index_collection = (index_collection_t*)malloc(sizeof(index_collection_t));
    if (!g_index_collection) {
        LOG_ERROR("Failed to allocate memory for index collection");
        return 0;
    }
    
    g_index_collection->indices = (document_index_t**)malloc(initial_capacity * sizeof(document_index_t*));
    if (!g_index_collection->indices) {
        LOG_ERROR("Failed to allocate memory for index collection indices");
        free(g_index_collection);
        g_index_collection = NULL;
        return 0;
    }
    
    g_index_collection->collection_names = (char**)malloc(initial_capacity * sizeof(char*));
    if (!g_index_collection->collection_names) {
        LOG_ERROR("Failed to allocate memory for index collection names");
        free(g_index_collection->indices);
        free(g_index_collection);
        g_index_collection = NULL;
        return 0;
    }
    
    g_index_collection->capacity = initial_capacity;
    g_index_collection->size = 0;
    pthread_mutex_init(&g_index_collection->lock, NULL);
    
    LOG_INFO("Index collection initialized with capacity %zu", initial_capacity);
    return 1;
}

/**
 * Free the global index collection
 */
static void free_index_collection() {
    if (!g_index_collection) {
        return;
    }
    
    pthread_mutex_lock(&g_index_collection->lock);
    
    if (g_index_collection->indices) {
        for (size_t i = 0; i < g_index_collection->size; i++) {
            if (g_index_collection->indices[i]) {
                free_document_index(g_index_collection->indices[i]);
            }
        }
        free(g_index_collection->indices);
    }
    
    if (g_index_collection->collection_names) {
        for (size_t i = 0; i < g_index_collection->size; i++) {
            if (g_index_collection->collection_names[i]) {
                free(g_index_collection->collection_names[i]);
            }
        }
        free(g_index_collection->collection_names);
    }
    
    pthread_mutex_unlock(&g_index_collection->lock);
    pthread_mutex_destroy(&g_index_collection->lock);
    
    free(g_index_collection);
    g_index_collection = NULL;
    
    LOG_INFO("Index collection freed");
}

/**
 * Get or create an index for a collection
 * 
 * @param collection_name Name of the collection
 * @return Index for the collection or NULL on error
 */
static document_index_t* get_or_create_index(const char* collection_name) {
    if (!g_index_collection) {
        if (!initialize_index_collection(10)) {
            return NULL;
        }
    }
    
    pthread_mutex_lock(&g_index_collection->lock);
    
    /* Look for existing index */
    for (size_t i = 0; i < g_index_collection->size; i++) {
        if (strcmp(g_index_collection->collection_names[i], collection_name) == 0) {
            document_index_t* index = g_index_collection->indices[i];
            pthread_mutex_unlock(&g_index_collection->lock);
            return index;
        }
    }
    
    /* Need to create a new index */
    if (g_index_collection->size >= g_index_collection->capacity) {
        size_t new_capacity = g_index_collection->capacity * 2;
        
        document_index_t** new_indices = (document_index_t**)realloc(
            g_index_collection->indices, new_capacity * sizeof(document_index_t*));
        if (!new_indices) {
            pthread_mutex_unlock(&g_index_collection->lock);
            LOG_ERROR("Failed to resize index collection indices");
            return NULL;
        }
        g_index_collection->indices = new_indices;
        
        char** new_names = (char**)realloc(
            g_index_collection->collection_names, new_capacity * sizeof(char*));
        if (!new_names) {
            pthread_mutex_unlock(&g_index_collection->lock);
            LOG_ERROR("Failed to resize index collection names");
            return NULL;
        }
        g_index_collection->collection_names = new_names;
        
        g_index_collection->capacity = new_capacity;
    }
    
    /* Create new index */
    document_index_t* index = create_document_index(100);
    if (!index) {
        pthread_mutex_unlock(&g_index_collection->lock);
        LOG_ERROR("Failed to create document index for collection %s", collection_name);
        return NULL;
    }
    
    /* Add to collection */
    g_index_collection->indices[g_index_collection->size] = index;
    g_index_collection->collection_names[g_index_collection->size] = strdup(collection_name);
    g_index_collection->size++;
    
    pthread_mutex_unlock(&g_index_collection->lock);
    
    LOG_INFO("Created new document index for collection %s", collection_name);
    return index;
}

/**
 * Add a document to the index
 * 
 * @param index Index to add to
 * @param id Document ID
 * @param position Position in the collection array
 * @return 1 on success, 0 on failure
 */
static int add_to_index(document_index_t* index, const char* id, int position) {
    if (!index || !id) {
        return 0;
    }
    
    pthread_mutex_lock(&index->lock);
    
    /* Check if document already exists */
    for (size_t i = 0; i < index->size; i++) {
        if (strcmp(index->ids[i], id) == 0) {
            index->positions[i] = position;
            pthread_mutex_unlock(&index->lock);
            return 1;
        }
    }
    
    /* Check if index needs to be resized */
    if (index->size >= index->capacity) {
        size_t new_capacity = index->capacity * 2;
        
        char** new_ids = (char**)realloc(index->ids, new_capacity * sizeof(char*));
        if (!new_ids) {
            pthread_mutex_unlock(&index->lock);
            LOG_ERROR("Failed to resize document index IDs");
            return 0;
        }
        index->ids = new_ids;
        
        int* new_positions = (int*)realloc(index->positions, new_capacity * sizeof(int));
        if (!new_positions) {
            pthread_mutex_unlock(&index->lock);
            LOG_ERROR("Failed to resize document index positions");
            return 0;
        }
        index->positions = new_positions;
        
        index->capacity = new_capacity;
    }
    
    /* Add document to index */
    index->ids[index->size] = strdup(id);
    index->positions[index->size] = position;
    index->size++;
    
    pthread_mutex_unlock(&index->lock);
    
    return 1;
}

/**
 * Remove a document from the index
 * 
 * @param index Index to remove from
 * @param id Document ID
 * @return 1 on success, 0 if document not found
 */
static int remove_from_index(document_index_t* index, const char* id) {
    if (!index || !id) {
        return 0;
    }
    
    pthread_mutex_lock(&index->lock);
    
    /* Find document in index */
    for (size_t i = 0; i < index->size; i++) {
        if (strcmp(index->ids[i], id) == 0) {
            /* Free ID */
            free(index->ids[i]);
            
            /* Shift remaining items */
            for (size_t j = i; j < index->size - 1; j++) {
                index->ids[j] = index->ids[j + 1];
                index->positions[j] = index->positions[j + 1];
            }
            
            index->size--;
            pthread_mutex_unlock(&index->lock);
            return 1;
        }
    }
    
    pthread_mutex_unlock(&index->lock);
    return 0;
}

/**
 * Find a document in the index
 * 
 * @param index Index to search
 * @param id Document ID
 * @param position Pointer to store position (0 if not found)
 * @return 1 if found, 0 if not found
 */
static int find_in_index(document_index_t* index, const char* id, int* position) {
    if (!index || !id || !position) {
        if (position) *position = -1;
        return 0;
    }
    
    pthread_mutex_lock(&index->lock);
    
    for (size_t i = 0; i < index->size; i++) {
        if (strcmp(index->ids[i], id) == 0) {
            *position = index->positions[i];
            pthread_mutex_unlock(&index->lock);
            return 1;
        }
    }
    
    pthread_mutex_unlock(&index->lock);
    *position = -1;
    return 0;
}

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
 * Rebuild the index for a collection
 * 
 * @param db Database instance
 * @param collection_name Collection name
 * @return 1 on success, 0 on failure
 */
static int rebuild_collection_index(database_t* db, const char* collection_name) {
    if (!db || !collection_name) {
        return 0;
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* Get collection */
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        pthread_mutex_unlock(&db->lock);
        return 0;
    }
    
    /* Create or get index */
    document_index_t* index = get_or_create_index(collection_name);
    if (!index) {
        pthread_mutex_unlock(&db->lock);
        return 0;
    }
    
    /* Clear existing index */
    pthread_mutex_lock(&index->lock);
    for (size_t i = 0; i < index->size; i++) {
        if (index->ids[i]) {
            free(index->ids[i]);
        }
    }
    index->size = 0;
    pthread_mutex_unlock(&index->lock);
    
    /* Rebuild index */
    for (size_t i = 0; i < collection->value.array.size; i++) {
        json_value_t* doc = collection->value.array.items[i];
        if (doc && doc->type == JSON_OBJECT) {
            json_value_t* id_val = json_object_get(doc, "_id");
            if (id_val && id_val->type == JSON_STRING) {
                add_to_index(index, id_val->value.string, i);
            }
        }
    }
    
    pthread_mutex_unlock(&db->lock);
    return 1;
}

/**
 * Optimized implementation of document insertion with indexing
 */
json_value_t* indexed_db_insert_document(database_t* db, const char* collection_name, json_value_t* document) {
    LOG_INFO("Starting indexed document insertion for collection '%s'", collection_name ? collection_name : "NULL");
    
    if (!db || !collection_name || !document || document->type != JSON_OBJECT) {
        LOG_ERROR("Invalid parameters for indexed_db_insert_document");
        return NULL;
    }
    
    /* Clone document outside of lock */
    json_value_t* doc_copy = json_clone(document);
    if (!doc_copy) {
        LOG_ERROR("Failed to clone document for insertion");
        return NULL;
    }
    
    /* Generate document ID if needed */
    const char* id_str = NULL;
    char* generated_id = NULL;
    
    json_value_t* id = json_object_get(doc_copy, "_id");
    if (!id || id->type != JSON_STRING) {
        generated_id = generate_uuid();
        if (!generated_id) {
            LOG_ERROR("Failed to generate UUID for document");
            json_free(doc_copy);
            return NULL;
        }
        
        json_object_set(doc_copy, "_id", json_create_string(generated_id));
        id_str = generated_id;
    } else {
        id_str = id->value.string;
    }
    
    /* Get or create index */
    document_index_t* index = get_or_create_index(collection_name);
    if (!index) {
        LOG_ERROR("Failed to get or create index for collection %s", collection_name);
        json_free(doc_copy);
        if (generated_id) free(generated_id);
        return NULL;
    }
    
    /* Create result object */
    json_value_t* result = json_create_object();
    if (!result) {
        LOG_ERROR("Failed to create result object");
        json_free(doc_copy);
        if (generated_id) free(generated_id);
        return NULL;
    }
    json_object_set(result, "_id", json_create_string(id_str));
    
    /* Acquire database lock */
    pthread_mutex_lock(&db->lock);
    
    /* Get collection */
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        LOG_ERROR("Collection '%s' not found or not an array", collection_name);
        pthread_mutex_unlock(&db->lock);
        json_free(result);
        json_free(doc_copy);
        if (generated_id) free(generated_id);
        return NULL;
    }
    
    /* Add document to collection */
    int position = collection->value.array.size;
    json_array_append(collection, doc_copy);
    db->is_modified = 1;
    
    /* Release database lock */
    pthread_mutex_unlock(&db->lock);
    
    /* Add to index */
    add_to_index(index, id_str, position);
    
    if (generated_id) free(generated_id);
    
    LOG_INFO("Document inserted successfully with ID: %s", id_str);
    return result;
}

/**
 * Optimized implementation of document retrieval with indexing
 */
json_value_t* indexed_db_get_document(database_t* db, const char* collection_name, const char* id) {
    LOG_INFO("Starting indexed document retrieval for collection '%s', ID '%s'", 
             collection_name ? collection_name : "NULL", 
             id ? id : "NULL");
    
    if (!db || !collection_name || !id) {
        LOG_ERROR("Invalid parameters for indexed_db_get_document");
        return NULL;
    }
    
    /* Get index */
    document_index_t* index = get_or_create_index(collection_name);
    if (!index) {
        LOG_ERROR("Failed to get or create index for collection %s", collection_name);
        return NULL;
    }
    
    /* Look up document in index */
    int position = -1;
    if (!find_in_index(index, id, &position)) {
        LOG_WARNING("Document with ID '%s' not found in index for collection '%s'", id, collection_name);
        
        /* Rebuild index and try again */
        LOG_INFO("Rebuilding index for collection '%s'", collection_name);
        if (rebuild_collection_index(db, collection_name) && find_in_index(index, id, &position)) {
            LOG_INFO("Document found in rebuilt index at position %d", position);
        } else {
            LOG_WARNING("Document still not found after rebuilding index");
            return NULL;
        }
    }
    
    if (position < 0) {
        LOG_WARNING("Document with ID '%s' not found in collection '%s'", id, collection_name);
        return NULL;
    }
    
    /* Acquire database lock */
    pthread_mutex_lock(&db->lock);
    
    /* Get collection */
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        LOG_ERROR("Collection '%s' not found or not an array", collection_name);
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }
    
    /* Verify position is valid */
    if (position >= collection->value.array.size) {
        LOG_ERROR("Invalid position %d in collection of size %zu", position, collection->value.array.size);
        pthread_mutex_unlock(&db->lock);
        
        /* Rebuild index - position is out of bounds */
        LOG_INFO("Rebuilding index for collection '%s' due to invalid position", collection_name);
        rebuild_collection_index(db, collection_name);
        return NULL;
    }
    
    /* Get document */
    json_value_t* doc = collection->value.array.items[position];
    if (!doc || doc->type != JSON_OBJECT) {
        LOG_ERROR("Document at position %d is not a valid object", position);
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }
    
    /* Verify ID matches */
    json_value_t* doc_id = json_object_get(doc, "_id");
    if (!doc_id || doc_id->type != JSON_STRING || strcmp(doc_id->value.string, id) != 0) {
        LOG_ERROR("Document ID mismatch: expected '%s', found '%s'", id, 
                 doc_id && doc_id->type == JSON_STRING ? doc_id->value.string : "NULL");
        pthread_mutex_unlock(&db->lock);
        
        /* Rebuild index - ID mismatch */
        LOG_INFO("Rebuilding index for collection '%s' due to ID mismatch", collection_name);
        rebuild_collection_index(db, collection_name);
        return NULL;
    }
    
    /* Clone document */
    json_value_t* doc_clone = json_clone(doc);
    
    /* Release database lock */
    pthread_mutex_unlock(&db->lock);
    
    if (!doc_clone) {
        LOG_ERROR("Failed to clone document");
        return NULL;
    }
    
    LOG_INFO("Document retrieved successfully with ID: %s", id);
    return doc_clone;
}

/**
 * Optimized implementation of document query with indexing
 */
json_value_t* indexed_db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json) {
    LOG_INFO("Starting indexed document query for collection '%s'", collection_name ? collection_name : "NULL");
    
    if (!db || !collection_name) {
        LOG_ERROR("Invalid parameters for indexed_db_query_documents");
        return NULL;
    }
    
    /* Create empty query if not provided */
    int empty_query = 0;
    if (!query_json) {
        LOG_DEBUG("Creating empty query object");
        query_json = json_create_object();
        empty_query = 1;
    }
    
    /* Parse query */
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
    
    /* Acquire database lock */
    pthread_mutex_lock(&db->lock);
    
    /* Get collection */
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
    
    /* Create documents array for query */
    json_value_t* documents_copy = json_create_array();
    if (!documents_copy) {
        LOG_ERROR("Failed to create documents array");
        pthread_mutex_unlock(&db->lock);
        query_free_parse_result(&query_result);
        if (empty_query) {
            json_free(query_json);
        }
        return NULL;
    }
    
    /* Fast path: direct ID lookup if query is for a specific ID */
    int used_index = 0;
    if (query_result.expr && query_result.expr->op == OP_AND && query_result.expr->num_children == 1) {
        query_expr_t* child = query_result.expr->children[0];
        if (child && child->op == OP_EQ && child->field_path && strcmp(child->field_path, "_id") == 0 && 
            child->value && child->value->type == JSON_STRING) {
            /* This is an ID lookup query */
            const char* query_id = child->value->value.string;
            LOG_INFO("Optimizing query as direct ID lookup for ID: %s", query_id);
            
            /* Get document by ID using index */
            json_value_t* doc = indexed_db_get_document(db, collection_name, query_id);
            if (doc) {
                json_array_append(documents_copy, doc);
                used_index = 1;
                
                LOG_DEBUG("Direct ID lookup succeeded via index");
            }
        }
    }
    
    /* If not using index, perform full scan but with optimized copying */
    if (!used_index) {
        /* Get collection size once outside the loop */
        size_t collection_size = collection->value.array.size;
        
        /* Pre-allocate batch memory outside the lock */
        json_value_t** batch = NULL;
        size_t batch_size = 0;
        
        /* Using a batching approach to reduce lock time */
        const size_t MAX_BATCH_SIZE = 100;
        batch = (json_value_t**)malloc(MAX_BATCH_SIZE * sizeof(json_value_t*));
        
        if (batch) {
            size_t processed = 0;
            while (processed < collection_size) {
                /* Determine batch size */
                batch_size = (collection_size - processed > MAX_BATCH_SIZE) ? 
                            MAX_BATCH_SIZE : (collection_size - processed);
                
                /* Copy batch references while holding the lock */
                pthread_mutex_lock(&db->lock);
                
                /* Verify collection size hasn't changed */
                if (collection->value.array.size < processed + batch_size) {
                    batch_size = collection->value.array.size - processed;
                    if (batch_size <= 0) {
                        pthread_mutex_unlock(&db->lock);
                        break;
                    }
                }
                
                /* Copy references */
                for (size_t i = 0; i < batch_size; i++) {
                    batch[i] = collection->value.array.items[processed + i];
                }
                
                pthread_mutex_unlock(&db->lock);
                
                /* Clone documents outside the lock with better error handling */
                for (size_t i = 0; i < batch_size; i++) {
                    if (batch[i] && batch[i]->type == JSON_OBJECT) {
                        /* Use stringify/parse for safer cloning */
                        char* doc_str = json_stringify(batch[i]);
                        if (doc_str) {
                            json_value_t* cloned_doc = json_parse(doc_str);
                            free(doc_str);
                            
                            if (cloned_doc) {
                                json_array_append(documents_copy, cloned_doc);
                            } else {
                                LOG_ERROR("Failed to parse document JSON in batch processing");
                            }
                        } else {
                            LOG_ERROR("Failed to stringify document in batch processing");
                        }
                    } else {
                        LOG_WARNING("Skipping invalid document in collection at index %zu", processed + i);
                    }
                }
                
                processed += batch_size;
            }
            
            free(batch);
        } else {
            /* Fallback to traditional one-by-one copying with safer handling */
            LOG_WARNING("Using fallback document copying method - this may be slower");
            for (size_t i = 0; i < collection_size; i++) {
                pthread_mutex_lock(&db->lock);
                
                json_value_t* doc = NULL;
                if (i < collection->value.array.size) {
                    doc = collection->value.array.items[i];
                    if (doc && doc->type == JSON_OBJECT) {
                        /* Just extract the document pointer and stringify while locked */
                        char* doc_str = json_stringify(doc);
                        pthread_mutex_unlock(&db->lock);
                        
                        if (doc_str) {
                            /* Parse and append outside the lock */
                            json_value_t* cloned_doc = json_parse(doc_str);
                            free(doc_str);
                            
                            if (cloned_doc) {
                                json_array_append(documents_copy, cloned_doc);
                            } else {
                                LOG_ERROR("Failed to parse document JSON in fallback processing");
                            }
                        } else {
                            LOG_ERROR("Failed to stringify document at index %zu", i);
                        }
                    } else {
                        LOG_WARNING("Skipping invalid document in collection at index %zu", i);
                        pthread_mutex_unlock(&db->lock);
                    }
                } else {
                    pthread_mutex_unlock(&db->lock);
                }
            }
        }
    }
    
    /* Release database lock if still held */
    if (pthread_mutex_trylock(&db->lock) == 0) {
        pthread_mutex_unlock(&db->lock);
    }
    
    /* Execute query */
    LOG_DEBUG("Executing query against %zu documents", json_array_size(documents_copy));
    query_result_t execute_result = query_execute(query_result.expr, documents_copy, &query_result.options);
    
    /* Free the cloned documents once query execution is complete */
    json_free(documents_copy);
    
    /* Create response object */
    json_value_t* response = json_create_object();
    if (!response) {
        LOG_ERROR("Failed to create response object");
        query_free_parse_result(&query_result);
        if (empty_query) {
            json_free(query_json);
        }
        return NULL;
    }
    
    /* Add documents to response */
    json_object_set(response, "documents", execute_result.documents);
    
    /* Add count information */
    json_object_set(response, "count", json_create_integer(execute_result.count));
    json_object_set(response, "total_count", json_create_integer(execute_result.total_count));
    
    /* Add pagination information if available */
    if (execute_result.pagination) {
        json_value_t* pagination_json = query_pagination_to_json(execute_result.pagination);
        if (pagination_json) {
            json_object_set(response, "pagination", pagination_json);
        }
        query_free_pagination_info(execute_result.pagination);
    }
    
    /* Free resources */
    query_free_parse_result(&query_result);
    if (empty_query) {
        json_free(query_json);
    }
    
    LOG_INFO("Query completed successfully, returning %d documents", execute_result.count);
    return response;
}

/**
 * API to rebuild all indices
 */
int rebuild_all_indices(database_t* db) {
    if (!db) {
        return 0;
    }
    
    pthread_mutex_lock(&db->lock);
    
    /* Get all collection names */
    size_t collection_count = db->collections->value.object.size;
    char** collection_names = NULL;
    
    if (collection_count > 0) {
        collection_names = (char**)malloc(collection_count * sizeof(char*));
        if (!collection_names) {
            pthread_mutex_unlock(&db->lock);
            return 0;
        }
        
        for (size_t i = 0; i < collection_count; i++) {
            collection_names[i] = strdup(db->collections->value.object.entries[i].key);
        }
    }
    
    pthread_mutex_unlock(&db->lock);
    
    /* Rebuild indices */
    int success = 1;
    for (size_t i = 0; i < collection_count; i++) {
        if (!rebuild_collection_index(db, collection_names[i])) {
            success = 0;
        }
        free(collection_names[i]);
    }
    
    if (collection_names) {
        free(collection_names);
    }
    
    return success;
}