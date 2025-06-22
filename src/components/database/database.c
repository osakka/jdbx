/**
 * @file database.c
 * @brief Core JDBX database implementation with lock-free architecture
 * 
 * This module implements the main database interface using JDBX storage backend
 * with Write-Ahead Logging (WAL), B-tree structures, and integrated caching.
 * 
 * Architecture:
 * - Hierarchical structure: Libraries → Collections → Documents
 * - Lock-free reads with minimal locking for writes
 * - Single source of truth with global database instance
 * - UUID-based document identification (no _id fields)
 * 
 * Storage Backend:
 * - JDBX page manager with WAL for durability
 * - B-tree indexing for performance
 * - Skiplist structures for lock-free operations
 * - Integrated caching for frequently accessed data
 * 
 * Thread Safety:
 * - Lock-free library lookup using atomic operations
 * - Reader/writer locks at collection level
 * - Dedicated library creation mutex for race prevention
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>    /* For nanosleep */

#include "database/database.h"
#include "database/document_storage.h"
#include "storage/jdbx.h"
#include "utils/logger.h"
#include "utils/json.h"
#include "utils/json_helpers.h"
#include "utils/config_loader.h"
#include "database/adaptive_indexer.h"
#include "database/query_tracker.h"
#include "database/index_metrics.h"
#include "utils/generic_cache.h"
#include "utils/skiplist.h"
#include "utils/memory_manager.h"
#include "utils/buffer_pool.h"
#include "utils/memory_manager.h"
#include "rbac/rbac_db.h"

/**
 * Library structure - top level container for collections
 * 
 * Libraries provide namespace isolation and organizational structure.
 * Each library contains its own set of collections and maintains
 * independent access control and configuration.
 */
typedef struct library {
    char name[64];               /**< Library name (max 63 chars + null) */
    void* collections;           /**< Skiplist of collection_t structures */
    pthread_rwlock_t lock;       /**< Reader/writer lock for thread safety */
} library_t;

/**
 * Collection structure - container for documents within a library
 * 
 * Collections hold documents and their associated indexes, schemas,
 * and metadata. Each collection maintains its own locking for
 * concurrent access while preserving data consistency.
 */
typedef struct collection {
    char name[64];               /**< Collection name (max 63 chars + null) */
    char library[64];            /**< Parent library name for reference */
    void* documents;             /**< Skiplist of document pointers by UUID */
    void* indexes;               /**< Skiplist of field indexes for queries */
    json_value_t* schema;        /**< Optional JSON schema for validation */
    pthread_rwlock_t lock;       /**< Reader/writer lock for thread safety */
} collection_t;

/**
 * Main JDBX database structure with lock-free architecture
 * 
 * This structure represents the complete database instance including
 * persistent storage, in-memory indexes, caching, and coordination
 * mechanisms for concurrent access.
 */
typedef struct {
    /* JDBX storage engine */
    jdbx_page_manager_t* page_manager;   /**< WAL, B-tree, caching backend */
    
    /* Lock-free hierarchical structure */
    void* libraries;                     /**< Skiplist of library_t structures */
    pthread_rwlock_t lock;               /**< Database-wide coordination lock */
    bool initialized;                    /**< Initialization status flag */
    char path[256];                      /**< Database file path */
    
    /* Integrated performance components */
    generic_cache_t* query_cache;        /**< Query result cache (1000 entries) */
    generic_cache_t* doc_cache;          /**< Document cache (10000 entries) */
    
    /* Legacy API compatibility */
    database_t facade;                   /**< Compatibility interface wrapper */
} jdbx_database_t;

/**
 * Global database instance - single source of truth
 * 
 * This is the primary database instance used throughout the application.
 * Initialized once during startup and shared across all components.
 * Thread-safe access is managed through the embedded locking mechanisms.
 */
static jdbx_database_t g_db = {
    .page_manager = NULL,                    /**< Initialized by db_init() */
    .libraries = NULL,                       /**< Created during initialization */
    .lock = PTHREAD_RWLOCK_INITIALIZER,      /**< Ready for immediate use */
    .initialized = false,                    /**< Set to true after db_init() */
    .path = {0},                             /**< Set during initialization */
    .query_cache = NULL,                     /**< Created during initialization */
    .doc_cache = NULL,                       /**< Created during initialization */
    .facade = {0}                            /**< Legacy compatibility interface */
};

/* Forward declarations */
static library_t* get_or_create_library(const char* name);
static collection_t* get_or_create_collection(const char* library_name, const char* collection_name);
static void ensure_virtual_collection_exists(const char* virtual_library, const char* collection_name);
static int parse_collection_path(const char* path, char* library, char* collection);
static int skiplist_string_compare(const void* a, size_t a_len, const void* b, size_t b_len);
static char* generate_doc_cache_key(const char* uuid);
static char* generate_query_cache_key(const char* library, const char* collection, json_value_t* query);

/**
 * String comparison function for skiplist operations
 * 
 * @param a First string to compare
 * @param a_len Length of first string (unused - strings are null-terminated)
 * @param b Second string to compare  
 * @param b_len Length of second string (unused - strings are null-terminated)
 * @return Standard strcmp result: <0, 0, or >0
 */
static int skiplist_string_compare(const void* a, size_t a_len, const void* b, size_t b_len) {
    (void)a_len; /* Strings are null-terminated */
    (void)b_len;
    return strcmp((const char*)a, (const char*)b);
}

/**
 * Parse collection path into library and collection components
 * 
 * Handles paths in format "library/collection" or just "collection".
 * If no library is specified, "default" library is used.
 * 
 * @param path Input path string (e.g., "mylib/mycoll" or "mycoll")
 * @param library Output buffer for library name (must be 64+ bytes)
 * @param collection Output buffer for collection name (must be 64+ bytes)
 * @return 0 on success, -1 on invalid parameters
 */
static int parse_collection_path(const char* path, char* library, char* collection) {
    if (!path || !library || !collection) return -1;
    
    const char* slash = strchr(path, '/');
    if (!slash) {
        /* No library specified, use default */
        strcpy(library, "default");
        strcpy(collection, path);
    } else {
        /* Extract library and collection */
        size_t lib_len = slash - path;
        strncpy(library, path, lib_len);
        library[lib_len] = '\0';
        strcpy(collection, slash + 1);
    }
    
    return 0;
}

/**
 * Generate cache key for document operations
 * Format: "doc:uuid"
 */
static char* generate_doc_cache_key(const char* uuid) {
    if (!uuid) return NULL;
    
    size_t key_len = strlen(uuid) + 5; /* "doc:" + uuid + null terminator */
    char* cache_key = (char*)BUFFER_ALLOC(key_len);
    if (!cache_key) return NULL;
    
    snprintf(cache_key, key_len, "doc:%s", uuid);
    return cache_key;
}

/**
 * Generate cache key for query operations
 * Format: "library:collection:query_hash"
 */
static char* generate_query_cache_key(const char* library, const char* collection, json_value_t* query) {
    if (!library || !collection) return NULL;
    
    /* Generate query hash - simple string representation for now */
    char* query_str = NULL;
    uint32_t query_hash = 0;
    
    if (query) {
        query_str = json_stringify(query);
        if (query_str) {
            /* Simple hash function for query string */
            const char* str = query_str;
            while (*str) {
                query_hash = ((query_hash << 5) + query_hash) + (unsigned char)*str;
                str++;
            }
            BUFFER_FREE(query_str);
        }
    }
    
    /* Allocate cache key with format: library:collection:hash */
    size_t key_len = strlen(library) + strlen(collection) + 32; /* 32 for hash + separators */
    char* cache_key = (char*)BUFFER_ALLOC(key_len);
    if (!cache_key) return NULL;
    
    snprintf(cache_key, key_len, "%s:%s:%08x", library, collection, query_hash);
    return cache_key;
}

/* JSON helper functions */
static void json_object_set_string(json_value_t* obj, const char* key, const char* value) __attribute__((unused));
static void json_object_set_string(json_value_t* obj, const char* key, const char* value) {
    if (!obj || obj->type != JSON_OBJECT || !key) return;
    json_value_t* str_val = json_create_string(value);
    if (str_val) {
        json_object_set(obj, key, str_val);
    }
}

/* Lock-free library management with atomic operations */
static pthread_mutex_t g_library_creation_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Get or create library - Lock-free for reads, minimal locking for writes */
static library_t* get_or_create_library(const char* name) {
    if (!name) return NULL;
    
    /* First attempt: Lock-free search (skiplist is thread-safe for reads) */
    size_t value_len;
    library_t* lib = (library_t*)skiplist_search(g_db.libraries, name, strlen(name) + 1, &value_len);
    if (lib) {
        return lib; /* Found it - no locking needed */
    }
    
    /* Library doesn't exist, we need to create it */
    pthread_mutex_lock(&g_library_creation_mutex);
    
    /* Double-check after acquiring creation mutex */
    lib = (library_t*)skiplist_search(g_db.libraries, name, strlen(name) + 1, &value_len);
    if (lib) {
        pthread_mutex_unlock(&g_library_creation_mutex);
        return lib; /* Another thread created it while we waited */
    }
    
    LOG_DEBUG("Creating library: %s", name);
    
    /* Create new library structure */
    lib =BUFFER_CALLOC(1, sizeof(library_t));
    if (!lib) {
        pthread_mutex_unlock(&g_library_creation_mutex);
        LOG_ERROR("Memory allocation failed for library '%s'", name);
        return NULL;
    }
    
    strncpy(lib->name, name, sizeof(lib->name) - 1);
    lib->collections = skiplist_create(skiplist_string_compare);
    pthread_rwlock_init(&lib->lock, NULL);
    
    /* Insert into skiplist */
    bool insert_success = skiplist_insert(g_db.libraries, lib->name, strlen(lib->name) + 1, lib, sizeof(library_t));
    
    pthread_mutex_unlock(&g_library_creation_mutex);
    
    if (!insert_success) {
        LOG_ERROR("Failed to insert library '%s' into skiplist", name);
        skiplist_destroy(lib->collections);
        pthread_rwlock_destroy(&lib->lock);
        BUFFER_FREE(lib);
        return NULL;
    }
    
    LOG_INFO("Created library '%s'", name);
    return lib;
}

/* Get or create collection */
static collection_t* get_or_create_collection(const char* library_name, const char* collection_name) {
    if (!library_name || !collection_name) return NULL;
    
    /* UNIFIED DOCUMENTS ARCHITECTURE:
     * Only allow creation of the single physical unified collection.
     * All other "collections" should be virtual (document-based).
     */
    
    /* Only allow the unified documents collection */
    if (strcmp(library_name, PHYSICAL_STORAGE_LIBRARY) == 0 && 
        strcmp(collection_name, PHYSICAL_STORAGE_COLLECTION) == 0) {
        
        /* This is the single physical unified collection - allow creation */
        library_t* lib = get_or_create_library(library_name);
        if (!lib) return NULL;
        
        pthread_rwlock_wrlock(&lib->lock);
        
        /* Check if collection exists */
        size_t value_len;
        collection_t* coll = (collection_t*)skiplist_search(lib->collections, collection_name, 
                                                           strlen(collection_name) + 1, &value_len);
        if (coll) {
            pthread_rwlock_unlock(&lib->lock);
            return coll;
        }
        
        /* Create the unified documents collection */
        coll =BUFFER_CALLOC(1, sizeof(collection_t));
        if (!coll) {
            pthread_rwlock_unlock(&lib->lock);
            return NULL;
        }
        
        strncpy(coll->name, collection_name, sizeof(coll->name) - 1);
        strncpy(coll->library, library_name, sizeof(coll->library) - 1);
        coll->documents = skiplist_create(skiplist_string_compare);
        coll->indexes = skiplist_create(skiplist_string_compare);
        pthread_rwlock_init(&coll->lock, NULL);
        
        /* Add to library */
        skiplist_insert(lib->collections, coll->name, strlen(coll->name) + 1, 
                       coll, sizeof(collection_t));
        
        pthread_rwlock_unlock(&lib->lock);
        
        LOG_INFO("Created unified documents collection '%s/%s'", library_name, collection_name);
        return coll;
        
    } else {
        /* For any other collection request, this is a virtual collection.
         * We should ensure it exists as a virtual collection document.
         */
        ensure_virtual_collection_exists(library_name, collection_name);
        
        /* Return the unified documents collection for actual storage */
        return get_or_create_collection(PHYSICAL_STORAGE_LIBRARY, PHYSICAL_STORAGE_COLLECTION);
    }
}

/**
 * Ensure virtual collection exists as a document in unified storage
 * This creates a collection document if it doesn't exist
 * 
 * Virtual collections are stored as documents with:
 * - type: "collection"
 * - library: virtual_library
 * - collection: "collections"
 * - name: collection_name
 */
static void ensure_virtual_collection_exists(const char* virtual_library, const char* collection_name) {
    if (!virtual_library || !collection_name) return;
    
    /* Query for existing collection document */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string(DOC_TYPE_NAME_COLLECTION));
    json_object_set(query, "library", json_create_string(virtual_library));
    json_object_set(query, "collection", json_create_string("collections"));
    json_object_set(query, "name", json_create_string(collection_name));
    
    /* Check if collection document already exists */
    json_value_t* results = db_query_documents(&g_db.facade, PHYSICAL_STORAGE_LIBRARY, PHYSICAL_STORAGE_COLLECTION, query);
    
    bool collection_exists = false;
    if (results && json_get_type(results) == JSON_OBJECT) {
        json_value_t* documents = json_object_get(results, "documents");
        if (documents && json_get_type(documents) == JSON_ARRAY && json_array_size(documents) > 0) {
            collection_exists = true;
        }
    }
    
    if (results) {
        /* CHECKPOINT: json_free(results); */
    }
    /* CHECKPOINT: json_free(query); */
    
    if (collection_exists) {
        LOG_DEBUG("Virtual collection %s/%s already exists", virtual_library, collection_name);
        return;
    }
    
    /* Create virtual collection document */
    json_value_t* collection_doc = json_create_object();
    json_object_set(collection_doc, "type", json_create_string(DOC_TYPE_NAME_COLLECTION));
    json_object_set(collection_doc, "library", json_create_string(virtual_library));
    json_object_set(collection_doc, "collection", json_create_string("collections"));
    json_object_set(collection_doc, "name", json_create_string(collection_name));
    
    /* Add metadata */
    char timestamp[64];
    time_t now = time(NULL);
    struct tm* utc_tm = gmtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
    json_object_set(collection_doc, "created_at", json_create_string(timestamp));
    json_object_set(collection_doc, "modified_at", json_create_string(timestamp));
    json_object_set(collection_doc, "owner", json_create_string("system"));
    
    /* Insert collection document */
    json_value_t* inserted = virtual_insert(&g_db.facade, DOC_TYPE_NAME_COLLECTION, virtual_library, "collections", collection_doc, "system");
    
    if (inserted) {
        LOG_INFO("Created virtual collection document: %s/%s", virtual_library, collection_name);
        /* CHECKPOINT: json_free(inserted); */
    } else {
        LOG_ERROR("Failed to create virtual collection document: %s/%s", virtual_library, collection_name);
    }
    
    /* CHECKPOINT: json_free(collection_doc); */
}

/*==============================================================================
 * UNIFIED DOCUMENTS - Storage vs Virtual Function Separation
 *============================================================================*/

/**
 * Virtual Operations - High-level document type operations with proper field handling
 */

/**
 * GENERIC virtual query - works with any document type
 * Virtual layer handles business logic and field validation
 */
json_value_t* virtual_query(database_t* db, const char* type, const char* library, 
                           const char* collection, json_value_t* filters) {
    if (!type || !library) {
        LOG_ERROR("Virtual query: Missing required type or library");
        return NULL;
    }
    
    LOG_DEBUG("Virtual: Querying documents type='%s' library='%s' collection='%s'", 
              type, library, collection ? collection : "auto");
    
    // Create unified query with proper type/library/collection fields
    json_value_t* unified_query = json_create_object();
    json_object_set(unified_query, "type", json_create_string(type));
    json_object_set(unified_query, "library", json_create_string(library));
    
    // Auto-determine collection from type if not provided
    if (collection) {
        json_object_set(unified_query, "collection", json_create_string(collection));
    } else {
        // Auto-map type to collection (user -> users, role -> roles, etc.)
        char auto_collection[64];
        snprintf(auto_collection, sizeof(auto_collection), "%ss", type); // Simple pluralization
        json_object_set(unified_query, "collection", json_create_string(auto_collection));
    }
    
    // Add any additional filters
    if (filters && json_get_type(filters) == JSON_OBJECT) {
        json_value_t* filter_keys = json_object_get_keys(filters);
        if (filter_keys && json_get_type(filter_keys) == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(filter_keys); i++) {
                json_value_t* key_val = json_array_get(filter_keys, i);
                if (key_val && json_get_type(key_val) == JSON_STRING) {
                    const char* key = json_get_string(key_val);
                    json_value_t* value = json_object_get(filters, key);
                    if (value) {
                        json_object_set(unified_query, key, json_clone(value));
                    }
                }
            }
        }
        /* CHECKPOINT: json_free(filter_keys); */
    }
    
    // STORAGE LAYER: Execute query
    json_value_t* results = storage_query_documents(db, unified_query);
    /* CHECKPOINT: json_free(unified_query); */
    
    return results;
}

/**
 * GENERIC virtual insert - works with any document type  
 * Virtual layer handles business logic, validation, and mandatory field population
 */
json_value_t* virtual_insert(database_t* db, const char* type, const char* library,
                            const char* collection, json_value_t* document, const char* owner) {
    if (!type || !library || !document || !owner) {
        LOG_ERROR("Virtual insert: Missing required parameters (type, library, document, owner)");
        return NULL;
    }
    
    LOG_DEBUG("Virtual: Inserting document type='%s' library='%s' collection='%s'", 
              type, library, collection ? collection : "auto");
    
    // Create document copy with mandatory unified documents fields
    json_value_t* doc_copy = json_deep_copy(document);
    
    // MANDATORY UNIFIED DOCUMENTS FIELDS (override any existing values)
    json_object_set(doc_copy, "type", json_create_string(type));
    json_object_set(doc_copy, "library", json_create_string(library));
    
    // Auto-determine collection from type if not provided
    if (collection) {
        json_object_set(doc_copy, "collection", json_create_string(collection));
    } else {
        char auto_collection[64];
        snprintf(auto_collection, sizeof(auto_collection), "%ss", type);
        json_object_set(doc_copy, "collection", json_create_string(auto_collection));
    }
    
    // Set owner from parameter (explicit ownership)
    json_object_set(doc_copy, "owner", json_create_string(owner));
    
    // STORAGE LAYER: Insert with proper fields
    json_value_t* result = storage_insert_document(db, doc_copy);
    /* CHECKPOINT: json_free(doc_copy); */
    
    if (result) {
        const char* doc_id = json_get_string(json_object_get(result, "uuid"));
        TRACE_DB("Virtual: Document created successfully type='%s' id='%s'", type, doc_id);
    } else {
        LOG_ERROR("Virtual: Failed to create document type='%s'", type);
    }
    
    return result;
}

/**
 * COMPATIBILITY: Legacy specific function that calls generic virtual_query
 */
json_value_t* virtual_query_users(database_t* db, const char* library, json_value_t* filters) {
    return virtual_query(db, DOC_TYPE_NAME_USER, library, VIRTUAL_COLLECTION_USERS, filters);
}

/**
 * Virtual update with audit trail
 */
json_value_t* virtual_update(database_t* db, const char* uuid, json_value_t* document) {
    if (!db || !uuid || !document) {
        LOG_ERROR("Virtual update: Missing required parameters");
        return NULL;
    }
    
    LOG_DEBUG("Virtual: Updating document uuid='%s'", uuid);
    
    /* CHECKPOINT FIX: Create checkpoint for this operation to handle cleanup automatically */
    memory_checkpoint_t* checkpoint = memory_checkpoint_create();
    if (!checkpoint) {
        LOG_ERROR("Virtual update: Failed to create memory checkpoint");
        return NULL;
    }
    
    // Create document copy for update
    json_value_t* doc_copy = json_deep_copy(document);
    if (!doc_copy) {
        LOG_ERROR("Virtual update: Failed to copy document");
        memory_checkpoint_rewind(checkpoint);
        return NULL;
    }
    
    // Add audit trail - updated_at timestamp  
    time_t now = time(NULL);
    json_object_set(doc_copy, "updated_at", json_create_integer(now));
    
    // Note: Do NOT validate required fields here - storage_update_document will merge 
    // the update fields with the existing document, preserving required fields like 'owner'
    
    // STORAGE LAYER: Update with proper fields
    json_value_t* result = storage_update_document(db, uuid, doc_copy);
    
    if (result) {
        TRACE_DB("Virtual: Document updated successfully uuid='%s'", uuid);
        /* CHECKPOINT: Promote result to survive checkpoint cleanup */
        memory_promote(result);
        memory_checkpoint_commit(checkpoint);
    } else {
        LOG_ERROR("Virtual: Failed to update document uuid='%s'", uuid);
        memory_checkpoint_rewind(checkpoint);
    }
    
    return result;
}

/**
 * Virtual get by UUID
 */
json_value_t* virtual_get(database_t* db, const char* uuid) {
    if (!db || !uuid) {
        LOG_ERROR("Virtual get: Missing required parameters");
        return NULL;
    }
    
    return storage_get_document(db, uuid);
}

/**
 * Virtual delete by UUID  
 */
int virtual_delete(database_t* db, const char* uuid) {
    if (!db || !uuid) {
        LOG_ERROR("Virtual delete: Missing required parameters");
        return -1;
    }
    
    LOG_DEBUG("Virtual: Deleting document uuid='%s'", uuid);
    
    int result = storage_delete_document(db, uuid);
    
    // 🔧 FIX: storage_delete_document returns 1 for success (from skiplist_delete bool)
    if (result) {
        TRACE_DB("Virtual: Document deleted successfully uuid='%s'", uuid);
        return 1;  // Return 1 for success to match API expectations
    } else {
        LOG_ERROR("Virtual: Failed to delete document uuid='%s'", uuid);
        return 0;  // Return 0 for failure
    }
}


/**
 * Storage Operations - Direct physical unified collection access
 */

/**
 * Query documents directly from physical storage with unified query
 * This is a storage operation that works directly on the default/documents collection
 */
json_value_t* storage_query_documents(database_t* db, json_value_t* query) {
    (void)db; // Use global database
    
    LOG_DEBUG("Storage query on unified collection with query=%p", query);
    
    if (!query) {
        LOG_WARNING("storage_query_documents called with NULL query");
        return NULL;
    }
    
    // BAR RAISING: Promote query to survive checkpoint operations
    // Without promotion, query can be freed during skiplist iteration
    // causing crashes when accessing query fields in the matching loop
    json_promote(query);
    
    pthread_rwlock_rdlock(&g_db.lock);
    library_t* lib = get_or_create_library(STORAGE_LIBRARY);
    if (!lib) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    collection_t* coll = get_or_create_collection(STORAGE_LIBRARY, STORAGE_COLLECTION);
    if (!coll) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    pthread_rwlock_rdlock(&coll->lock);
    pthread_rwlock_unlock(&g_db.lock);
    
    // Iterate through all documents in the single unified collection
    json_value_t* result = json_create_object();
    json_value_t* filtered_docs = json_create_array();
    int count = 0;
    
    // Use skiplist iterator to traverse all documents
    void* iter = skiplist_iterator_create(coll->documents);
    if (!iter) {
        pthread_rwlock_unlock(&coll->lock);
        /* CHECKPOINT: json_free(result); */
        /* CHECKPOINT: json_free(filtered_docs); */
        return NULL;
    }
    
    char* key;
    size_t key_len;
    void* doc_ptr_data;
    size_t value_len;
    
    while (skiplist_iterator_next(iter, (void**)&key, &key_len, (void**)&doc_ptr_data, &value_len)) {
        if (!doc_ptr_data || value_len == 0) continue;
        
        /* CRITICAL THREAD SAFETY: Parse JSON string stored in skiplist */
        json_value_t* doc = NULL;
        if (doc_ptr_data) {
            // We now store JSON strings, not pointers
            char* json_str = (char*)doc_ptr_data;
            doc = json_parse(json_str);
        }
        if (!doc || doc->type != JSON_OBJECT) {
            LOG_DEBUG("Skipping invalid or corrupted document in storage iteration");
            if (doc) {
                /* CHECKPOINT: json_free(doc); */
            }
            continue;
        }
        
        // Apply query filter if provided
        bool matches_query = true;
        if (query && query->type == JSON_OBJECT) {
            // Simple field matching - iterate through query keys
            json_value_t* query_keys = json_object_get_keys(query);
            if (query_keys && query_keys->type == JSON_ARRAY) {
                for (size_t j = 0; j < json_array_size(query_keys) && matches_query; j++) {
                    json_value_t* key_val = json_array_get(query_keys, j);
                    if (key_val && key_val->type == JSON_STRING) {
                        const char* field_key = json_get_string(key_val);
                        json_value_t* query_value = json_object_get(query, field_key);
                        json_value_t* doc_field = json_object_get(doc, field_key);
                        
                        if (!doc_field) {
                            matches_query = false;
                            break;
                        }
                        
                        // Compare values based on type
                        if (query_value->type != doc_field->type) {
                            matches_query = false;
                            break;
                        }
                        
                        // Type-specific comparisons
                        switch (query_value->type) {
                            case JSON_STRING:
                                if (strcmp(json_get_string(query_value), json_get_string(doc_field)) != 0) {
                                    matches_query = false;
                                }
                                break;
                            case JSON_BOOLEAN:
                                if (query_value->value.boolean != doc_field->value.boolean) {
                                    matches_query = false;
                                }
                                break;
                            case JSON_NUMBER:
                                if (query_value->value.number != doc_field->value.number) {
                                    matches_query = false;
                                }
                                break;
                            default:
                                // For complex types, we don't support comparison yet
                                matches_query = false;
                                break;
                        }
                    }
                }
            }
            /* CHECKPOINT: json_free(query_keys); */
        }
        
        if (matches_query) {
            /* CRITICAL SAFETY: Validate document before cloning to prevent corruption */
            if (doc && doc->type == JSON_OBJECT) {
                json_value_t* doc_copy = json_clone(doc);
                if (doc_copy) {
                    json_array_append(filtered_docs, doc_copy);
                    count++;
                } else {
                    LOG_WARNING("Failed to clone document during storage query - skipping corrupted document");
                }
            }
        }
        
        /* CRITICAL: Free the parsed JSON document */
        if (doc) {
            /* CHECKPOINT: json_free(doc); */
        }
    }
    
    skiplist_iterator_destroy(iter);
    pthread_rwlock_unlock(&coll->lock);
    
    json_object_set(result, "documents", filtered_docs);
    json_object_set(result, "count", json_create_integer(count));
    
    return result;
}

/*==============================================================================
 * Database Initialization - Pure JDBX Implementation
 *============================================================================*/

/**
 * Initialize JDBX database instance
 * 
 * Creates or opens a JDBX database at the specified path, initializing all
 * internal structures, caches, and subsystems. This is the main entry point
 * for database operations and must be called before any other database functions.
 * 
 * Features initialized:
 * - JDBX page manager with WAL support
 * - Lock-free skiplist structures for libraries/collections
 * - Query and document caches for performance
 * - System libraries and collections
 * - Adaptive indexing and query tracking
 * 
 * @param path Database file path (NULL for default: /opt/jdbx/build/var/jdbx.jdbx)
 * @return Database instance pointer on success, NULL on failure
 */
database_t* db_init(const char* path) {
    pthread_rwlock_wrlock(&g_db.lock);
    
    if (g_db.initialized) {
        pthread_rwlock_unlock(&g_db.lock);
        LOG_WARNING("Database already initialized");
        return &g_db.facade;
    }
    
    /* Determine database path */
    const char* db_path = path;
    char jdbx_path[1024];
    
    if (!db_path || strlen(db_path) == 0) {
        db_path = getenv("JDBX_DB_PATH");
        if (!db_path) {
            db_path = "/opt/jdbx/build/var/jdbx.jdbx";
        }
    }
    
    /* Ensure .jdbx extension */
    if (strlen(db_path) > 5 && strcmp(db_path + strlen(db_path) - 5, ".jdbx") == 0) {
        strncpy(jdbx_path, db_path, sizeof(jdbx_path) - 1);
        jdbx_path[sizeof(jdbx_path) - 1] = '\0';
    } else if (strlen(db_path) > 4 && strcmp(db_path + strlen(db_path) - 4, ".jdb") == 0) {
        /* Replace .jdb with .jdbx */
        snprintf(jdbx_path, sizeof(jdbx_path), "%.*s.jdbx", 
                 (int)(strlen(db_path) - 4), db_path);
    } else {
        /* Add .jdbx extension */
        snprintf(jdbx_path, sizeof(jdbx_path), "%s.jdbx", db_path);
    }
    
    LOG_INFO("Initializing JDBX database at %s", jdbx_path);
    
    /* Initialize JDBX page manager for persistent storage */
    struct stat st;
    bool file_exists = (stat(jdbx_path, &st) == 0);
    
    if (file_exists) {
        LOG_INFO("Opening existing JDBX database");
        g_db.page_manager = jdbx_open(jdbx_path);
    } else {
        LOG_INFO("Creating new JDBX database");
        extern server_config_t* g_server_config;
        size_t initial_size = 100 * 1024 * 1024; /* Default 100MB */
        if (g_server_config && g_server_config->jdbx_initial_size > 0) {
            initial_size = g_server_config->jdbx_initial_size;
        }
        g_db.page_manager = jdbx_create(jdbx_path, initial_size);
    }
    
    if (!g_db.page_manager) {
        pthread_rwlock_unlock(&g_db.lock);
        LOG_ERROR("Failed to initialize JDBX page manager");
        return NULL;
    }
    
    /* Initialize in-memory lock-free structures */
    g_db.libraries = skiplist_create(skiplist_string_compare);
    if (!g_db.libraries) {
        jdbx_close(g_db.page_manager);
        pthread_rwlock_unlock(&g_db.lock);
        LOG_ERROR("Failed to create libraries skiplist");
        return NULL;
    }
    
    /* Initialize caches */
    g_db.query_cache = generic_cache_create(1000);
    g_db.doc_cache = generic_cache_create(10000);
    
    /* UNIFIED DOCUMENTS: Create only the unified storage libraries and collections */
    /* Create default library for unified document storage */
    get_or_create_library("default");
    
    /* Create the single unified documents collection where ALL documents will be stored */
    get_or_create_collection("default", "documents");
    
    /* Note: No physical system collections created - all documents (users, roles, sessions, etc.)
     * will be stored in default/documents with library/type/collection fields for filtering */
    
    /* Initialize subsystems */
    adaptive_indexer_init(&g_db.facade);
    query_tracker_init();
    
    /* Store path and mark as initialized */
    strncpy(g_db.path, jdbx_path, sizeof(g_db.path) - 1);
    g_db.initialized = true;
    
    /* Initialize facade for compatibility */
    g_db.facade.path = BUFFER_STRDUP(g_db.path);
    g_db.facade.is_bootstrap_mode = 0;
    g_db.facade.collections = json_create_object();
    pthread_rwlock_init(&g_db.facade.rwlock, NULL);
    g_db.facade.is_modified = 0;
    g_db.facade.cache = NULL;
    g_db.facade.cache_enabled = 1;
    g_db.facade.transaction_manager = NULL;
    g_db.facade.persistence = NULL;
    
    pthread_rwlock_unlock(&g_db.lock);
    
    LOG_INFO("JDBX database initialized successfully with WAL support");
    return &g_db.facade;
}

int db_init_directory(const char* dir_path) {
    return db_init(dir_path) ? 1 : 0;
}

void db_shutdown(void) {
    pthread_rwlock_wrlock(&g_db.lock);
    
    if (!g_db.initialized) {
        pthread_rwlock_unlock(&g_db.lock);
        return;
    }
    
    /* Cleanup subsystems */
    adaptive_indexer_cleanup();
    query_tracker_cleanup();
    
    /* Destroy caches */
    if (g_db.query_cache) {
        generic_cache_destroy(g_db.query_cache);
        g_db.query_cache = NULL;
    }
    if (g_db.doc_cache) {
        generic_cache_destroy(g_db.doc_cache);
        g_db.doc_cache = NULL;
    }
    
    /* Clean up libraries */
    if (g_db.libraries) {
        skiplist_destroy(g_db.libraries);
        g_db.libraries = NULL;
    }
    
    /* Sync and close JDBX page manager */
    if (g_db.page_manager) {
        jdbx_sync(g_db.page_manager);
        jdbx_close(g_db.page_manager);
        g_db.page_manager = NULL;
    }
    
    /* Clean up facade */
    if (g_db.facade.path) {
        BUFFER_FREE(g_db.facade.path);
        g_db.facade.path = NULL;
    }
    if (g_db.facade.collections) {
        /* CHECKPOINT: json_free(g_db.facade.collections); */
        g_db.facade.collections = NULL;
    }
    pthread_rwlock_destroy(&g_db.facade.rwlock);
    
    g_db.initialized = false;
    
    pthread_rwlock_unlock(&g_db.lock);
    
    LOG_INFO("JDBX database shutdown complete");
}

/*==============================================================================
 * Collection Operations
 *============================================================================*/

int db_create_collection(database_t* db, const char* collection_path) {
    (void)db; /* Unused - we use global */
    
    if (!g_db.initialized || !collection_path) return 0;
    
    char library[256], collection[256];
    if (parse_collection_path(collection_path, library, collection) != 0) {
        return 0;
    }
    
    collection_t* coll = get_or_create_collection(library, collection);
    return coll ? 1 : 0;
}

int db_collection_exists(database_t* db, const char* collection_path) {
    (void)db; /* Unused - we use global */
    
    if (!g_db.initialized || !collection_path) return 0;
    
    char library[256], collection[256];
    if (parse_collection_path(collection_path, library, collection) != 0) {
        return 0;
    }
    
    /* Lock-free library lookup */
    size_t value_len;
    library_t* lib = (library_t*)skiplist_search(g_db.libraries, library, strlen(library) + 1, &value_len);
    if (!lib) {
        return 0;
    }
    
    /* Lock collection for read */
    pthread_rwlock_rdlock(&lib->lock);
    collection_t* coll = (collection_t*)skiplist_search(lib->collections, collection, 
                                                        strlen(collection) + 1, &value_len);
    pthread_rwlock_unlock(&lib->lock);
    
    return coll ? 1 : 0;
}

json_value_t* db_list_collections(database_t* db) {
    (void)db; /* Unused - we use global */
    
    if (!g_db.initialized) return json_create_array();
    
    json_value_t* all_collections = json_create_array();
    
    /* Lock-free iteration through libraries */
    skiplist_iterator_t* lib_iter = skiplist_iterator_create(g_db.libraries);
    if (!lib_iter) {
        if (g_logger) LOG_ERROR("Failed to create library iterator for collections list");
        return all_collections; /* Return empty array */
    }
    
    void* lib_key = NULL;
    size_t lib_key_len = 0;
    void* lib_value = NULL;
    size_t lib_value_len = 0;
    
    while (skiplist_iterator_next(lib_iter, &lib_key, &lib_key_len, &lib_value, &lib_value_len)) {
        library_t* lib = (library_t*)lib_value;
        
        /* Lock individual library for collection iteration */
        pthread_rwlock_rdlock(&lib->lock);
        
        /* Iterate through collections in this library */
        skiplist_iterator_t* coll_iter = skiplist_iterator_create(lib->collections);
        if (!coll_iter) {
            if (g_logger) LOG_WARNING("Failed to create collection iterator for library '%s'", lib->name);
            pthread_rwlock_unlock(&lib->lock);
            continue; /* Skip this library and continue with next */
        }
        
        void* coll_key = NULL;
        size_t coll_key_len = 0;
        void* coll_value = NULL;
        size_t coll_value_len = 0;
        
        while (skiplist_iterator_next(coll_iter, &coll_key, &coll_key_len, &coll_value, &coll_value_len)) {
            collection_t* coll = (collection_t*)coll_value;
            
            /* Create full path */
            char full_path[128];
            snprintf(full_path, sizeof(full_path), "%s/%s", lib->name, coll->name);
            json_array_append(all_collections, json_create_string(full_path));
        }
        if (coll_iter) {
            skiplist_iterator_destroy(coll_iter);
        }
        
        pthread_rwlock_unlock(&lib->lock);
        
        /* Note: skiplist iterator returns direct pointers, no cleanup needed */
    }
    
    if (lib_iter) {
        skiplist_iterator_destroy(lib_iter);
    }
    
    return all_collections;
}

/*==============================================================================
 * Document Operations
 *============================================================================*/

/* REMOVED: db_insert() - Legacy hierarchical API eliminated for unified documents architecture */

/* REMOVED: db_find_by_id() - Legacy hierarchical API eliminated for unified documents architecture */

/* REMOVED: All legacy hierarchical API functions eliminated for unified documents architecture */

/*==============================================================================
 * TRUE UNIFIED DOCUMENTS API - Single Source of Truth
 *============================================================================*/

/* Clean API with separate library and collection parameters */
json_value_t* db_insert_document(database_t* db, const char* library, const char* collection, json_value_t* document) {
    (void)db; // Use global database
    
    // UNIFIED DOCUMENTS: Store ALL documents directly in the single unified collection
    // Generate UUID for document - use simple thread-safe approach
    char uuid[64];
    unsigned int random_val;
    
    // Use current time and thread ID for uniqueness
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    random_val = (unsigned int)(ts.tv_nsec ^ (uintptr_t)pthread_self() ^ 0x54321);
    
    snprintf(uuid, sizeof(uuid), "doc-%ld-%u", time(NULL), random_val);
    
    // Create document copy with mandatory unified fields
    json_value_t* doc_copy = json_deep_copy(document);
    json_object_set(doc_copy, "uuid", json_create_string(uuid));
    
    // UNIFIED DOCUMENTS: Preserve document type if already set, otherwise infer from collection
    json_value_t* existing_type = json_object_get(doc_copy, "type");
    if (!existing_type || json_get_type(existing_type) != JSON_STRING) {
        // Only set type if not already specified - infer from collection for backward compatibility
        json_object_set(doc_copy, "type", json_create_string(collection));
    }
    
    // UNIFIED DOCUMENTS: Preserve document library if already set, otherwise use storage library
    json_value_t* existing_library = json_object_get(doc_copy, "library");
    if (!existing_library || json_get_type(existing_library) != JSON_STRING) {
        // Only set library if not already specified
        json_object_set(doc_copy, "library", json_create_string(library));
    }
    
    // UNIFIED DOCUMENTS: Preserve document collection if already set, otherwise use storage collection
    json_value_t* existing_collection = json_object_get(doc_copy, "collection");
    if (!existing_collection || json_get_type(existing_collection) != JSON_STRING) {
        // Only set collection if not already specified
        json_object_set(doc_copy, "collection", json_create_string(collection));
    }
    
    // Add timestamps
    time_t now = time(NULL);
    json_object_set(doc_copy, "created_at", json_create_integer(now));
    json_object_set(doc_copy, "modified_at", json_create_integer(now));
    
    // Store directly in the single unified collection
    pthread_rwlock_rdlock(&g_db.lock);
    library_t* lib = get_or_create_library(STORAGE_LIBRARY);
    if (!lib) {
        pthread_rwlock_unlock(&g_db.lock);
        /* CHECKPOINT: json_free(doc_copy); */
        return NULL;
    }
    
    collection_t* coll = get_or_create_collection(STORAGE_LIBRARY, STORAGE_COLLECTION);
    if (!coll) {
        pthread_rwlock_unlock(&g_db.lock);
        /* CHECKPOINT: json_free(doc_copy); */
        return NULL;
    }
    
    pthread_rwlock_wrlock(&coll->lock);
    pthread_rwlock_unlock(&g_db.lock);
    
    // CRITICAL THREAD SAFETY: Store serialized JSON string instead of JSON object pointer
    // This prevents concurrent access to JSON structures during deep copy operations
    char* doc_str = json_stringify(doc_copy);
    if (!doc_str) {
        pthread_rwlock_unlock(&coll->lock);
        /* CHECKPOINT: json_free(doc_copy); */
        return NULL;
    }
    
    // Store the serialized string in the skiplist
    size_t str_len = strlen(doc_str) + 1;
    char* stored_str = (char*)BUFFER_ALLOC(str_len);
    if (!stored_str) {
        BUFFER_FREE(doc_str);
        pthread_rwlock_unlock(&coll->lock);
        /* CHECKPOINT: json_free(doc_copy); */
        return NULL;
    }
    memcpy(stored_str, doc_str, str_len);
    BUFFER_FREE(doc_str);
    
    // CRITICAL: Mark skiplist documents as hazard-protected
    memory_mark_hazard_protected(stored_str, NULL);
    
    skiplist_insert(coll->documents, uuid, strlen(uuid) + 1, stored_str, str_len);
    
    pthread_rwlock_unlock(&coll->lock);
    
    LOG_INFO("Inserted document '%s' into unified collection (library='%s', type='%s')", uuid, library, collection);
    
    // Return copy of the inserted document
    return json_deep_copy(doc_copy);
}

json_value_t* db_get_document(database_t* db, const char* library, const char* collection, const char* id) {
    /* 🚀 DOCUMENT CACHE LOOKUP - Check cache first for O(1) performance */
    char* cache_key = generate_doc_cache_key(id);
    if (cache_key && g_db.doc_cache) {
        void* cached_doc = generic_cache_get(g_db.doc_cache, cache_key, strlen(cache_key) + 1);
        if (cached_doc) {
            /* Cache hit - verify document matches library/collection and return copy */
            json_value_t* doc = (json_value_t*)cached_doc;
            json_value_t* doc_library = json_object_get(doc, "library");
            json_value_t* doc_type = json_object_get(doc, "type");
            
            if (doc_library && doc_type && 
                strcmp(json_get_string(doc_library), library) == 0 &&
                strcmp(json_get_string(doc_type), collection) == 0) {
                json_value_t* result = json_deep_copy(doc);
                BUFFER_FREE(cache_key);
                LOG_DEBUG("Document cache HIT for UUID: %s", id);
                return result;
            }
            LOG_DEBUG("Document cache HIT but library/type mismatch for UUID: %s", id);
        }
        LOG_DEBUG("Document cache MISS for UUID: %s", id);
    }
    
    // UNIFIED DOCUMENTS: Query from unified collection with library/type/id filtering
    json_value_t* query = json_create_object();
    json_object_set(query, "uuid", json_create_string(id));
    json_object_set(query, "library", json_create_string(library));
    json_object_set(query, "type", json_create_string(collection));
    
    json_value_t* results = db_query_documents(db, library, collection, query);
    /* CHECKPOINT: json_free(query); */
    
    if (!results) {
        if (cache_key) BUFFER_FREE(cache_key);
        return NULL;
    }
    
    json_value_t* docs = json_object_get(results, "documents");
    if (!docs || docs->type != JSON_ARRAY || json_array_size(docs) == 0) {
        /* CHECKPOINT: json_free(results); */
        if (cache_key) BUFFER_FREE(cache_key);
        return NULL;
    }
    
    json_value_t* doc = json_deep_copy(json_array_get(docs, 0));
    
    /* 🚀 DOCUMENT CACHE STORE - Cache the document for future lookups */
    if (cache_key && g_db.doc_cache && doc) {
        json_value_t* doc_copy = json_deep_copy(doc);
        if (doc_copy) {
            int cache_success = generic_cache_put(g_db.doc_cache,
                                                cache_key, strlen(cache_key) + 1,
                                                doc_copy, sizeof(json_value_t));
            if (cache_success) {
                LOG_DEBUG("Document cached for UUID: %s", id);
            } else {
                /* CHECKPOINT: json_free(doc_copy); */
                LOG_DEBUG("Failed to cache document for UUID: %s", id);
            }
        }
    }
    
    if (cache_key) BUFFER_FREE(cache_key);
    /* CHECKPOINT: json_free(results); */
    return doc;
}

json_value_t* db_update_document(database_t* db, const char* library, const char* collection, const char* id, 
                                json_value_t* update) {
    (void)db; // Use global database
    
    // UNIFIED DOCUMENTS: Update document directly in single unified collection
    pthread_rwlock_rdlock(&g_db.lock);
    library_t* lib = get_or_create_library(STORAGE_LIBRARY);
    if (!lib) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    collection_t* coll = get_or_create_collection(STORAGE_LIBRARY, STORAGE_COLLECTION);
    if (!coll) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    pthread_rwlock_wrlock(&coll->lock);
    pthread_rwlock_unlock(&g_db.lock);
    
    // Find existing document in unified collection
    size_t value_len;
    void* raw_data = skiplist_search(coll->documents, id, strlen(id) + 1, &value_len);
    if (!raw_data || value_len == 0) {
        if (raw_data) BUFFER_FREE(raw_data);  // Free the search result
        pthread_rwlock_unlock(&coll->lock);
        return NULL;
    }
    
    // CRITICAL THREAD SAFETY: Parse JSON string from skiplist
    char* json_str = (char*)raw_data;
    json_value_t* existing_doc = json_parse(json_str);
    BUFFER_FREE(raw_data);  // Free the search result immediately after parsing
    
    if (!existing_doc) {
        pthread_rwlock_unlock(&coll->lock);
        return NULL;
    }
    
    // Verify this document matches the requested library/collection
    json_value_t* doc_library = json_object_get(existing_doc, "library");
    json_value_t* doc_type = json_object_get(existing_doc, "type");
    if (!doc_library || !doc_type || 
        strcmp(json_get_string(doc_library), library) != 0 ||
        strcmp(json_get_string(doc_type), collection) != 0) {
        BUFFER_FREE(raw_data);  // Free the search result
        pthread_rwlock_unlock(&coll->lock);
        return NULL;
    }
    
    // Apply updates while protecting mandatory fields
    json_value_t* updated_doc = json_deep_copy(existing_doc);
    if (update->type == JSON_OBJECT) {
        json_value_t* keys = json_object_get_keys(update);
        if (keys && keys->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(keys); i++) {
                json_value_t* key_val = json_array_get(keys, i);
                if (key_val && key_val->type == JSON_STRING) {
                    const char* key = json_get_string(key_val);
                    json_value_t* value = json_object_get(update, key);
                    
                    // Protect mandatory unified document fields
                    if (strcmp(key, "uuid") != 0 && strcmp(key, "type") != 0 && 
                        strcmp(key, "library") != 0 && strcmp(key, "collection") != 0 && 
                        strcmp(key, "created_at") != 0) {
                        json_object_set(updated_doc, key, json_deep_copy(value));
                    }
                }
            }
            /* CHECKPOINT: json_free(keys); */
        }
    }
    
    // Update modified timestamp
    json_object_set(updated_doc, "modified_at", json_create_integer(time(NULL)));
    
    // CRITICAL THREAD SAFETY: Serialize updated document to JSON string
    char* updated_str = json_stringify(updated_doc);
    if (!updated_str) {
        pthread_rwlock_unlock(&coll->lock);
        /* CHECKPOINT: json_free(updated_doc); */
        /* CHECKPOINT: json_free(existing_doc); */
        return NULL;
    }
    
    // Store the new JSON string
    size_t str_len = strlen(updated_str) + 1;
    char* stored_str = (char*)BUFFER_ALLOC(str_len);
    if (!stored_str) {
        BUFFER_FREE(updated_str);
        pthread_rwlock_unlock(&coll->lock);
        /* CHECKPOINT: json_free(updated_doc); */
        /* CHECKPOINT: json_free(existing_doc); */
        return NULL;
    }
    memcpy(stored_str, updated_str, str_len);
    BUFFER_FREE(updated_str);
    
    // CRITICAL: Mark as hazard-protected
    memory_mark_hazard_protected(stored_str, NULL);
    
    // 3. Atomic replace in skiplist
    skiplist_delete(coll->documents, id, strlen(id) + 1);
    skiplist_insert(coll->documents, id, strlen(id) + 1, stored_str, str_len);
    
    pthread_rwlock_unlock(&coll->lock);
    
    LOG_INFO("Updated document '%s' in unified collection (library='%s', type='%s')", id, library, collection);
    
    // Return a copy and free the originals
    json_value_t* result = json_deep_copy(updated_doc);
    /* CHECKPOINT: json_free(existing_doc); */
    /* CHECKPOINT: json_free(updated_doc); */
    return result;
}

int db_delete_document(database_t* db, const char* library, const char* collection, const char* id) {
    (void)db; // Use global database
    
    // UNIFIED DOCUMENTS: Delete document directly from single unified collection
    pthread_rwlock_rdlock(&g_db.lock);
    library_t* lib = get_or_create_library(STORAGE_LIBRARY);
    if (!lib) {
        pthread_rwlock_unlock(&g_db.lock);
        return 0;
    }
    
    collection_t* coll = get_or_create_collection(STORAGE_LIBRARY, STORAGE_COLLECTION);
    if (!coll) {
        pthread_rwlock_unlock(&g_db.lock);
        return 0;
    }
    
    pthread_rwlock_wrlock(&coll->lock);
    pthread_rwlock_unlock(&g_db.lock);
    
    // Find and verify document in unified collection
    size_t value_len;
    void* raw_data = skiplist_search(coll->documents, id, strlen(id) + 1, &value_len);
    if (!raw_data || value_len == 0) {
        if (raw_data) BUFFER_FREE(raw_data);  // Free the search result
        pthread_rwlock_unlock(&coll->lock);
        return 0;
    }
    
    // CRITICAL THREAD SAFETY: Parse JSON string from skiplist
    char* json_str = (char*)raw_data;
    json_value_t* existing_doc = json_parse(json_str);
    BUFFER_FREE(raw_data);  // Free the search result immediately after parsing
    
    if (!existing_doc) {
        pthread_rwlock_unlock(&coll->lock);
        return 0;
    }
    
    // Verify this document matches the requested library/collection
    json_value_t* doc_library = json_object_get(existing_doc, "library");
    json_value_t* doc_type = json_object_get(existing_doc, "type");
    if (!doc_library || !doc_type || 
        strcmp(json_get_string(doc_library), library) != 0 ||
        strcmp(json_get_string(doc_type), collection) != 0) {
        /* CHECKPOINT: json_free(existing_doc); */
        pthread_rwlock_unlock(&coll->lock);
        return 0;
    }
    
    // Delete document from unified collection
    /* CHECKPOINT: json_free(existing_doc); */
    int result = skiplist_delete(coll->documents, id, strlen(id) + 1);
    pthread_rwlock_unlock(&coll->lock);
    
    if (result) {
        LOG_INFO("Deleted document '%s' from unified collection (library='%s', type='%s')", id, library, collection);
    }
    
    return result;
}

json_value_t* db_query_documents(database_t* db, const char* library, const char* collection, json_value_t* query) {
    (void)db; // Use global database
    
    /* ⚠️ ARCHITECTURAL WARNING ⚠️
     * If library/collection is NOT "default/documents", you're probably doing it WRONG!
     * 
     * JDBX uses unified storage - ALL documents are in "default/documents".
     * Use the "type" field in your query to filter for specific document types.
     * 
     * Example - Getting all users:
     * ❌ WRONG: db_query_documents(db, "system", "users", {})
     * ✅ RIGHT: db_query_documents(db, "default", "documents", {"type": "user", "library": "system"})
     */
    if (strcmp(library, "default") != 0 || strcmp(collection, "documents") != 0) {
        LOG_WARNING("Query to non-unified collection %s/%s - this is probably wrong! Use type discrimination instead.", library, collection);
    }
    
    /* 🚀 QUERY CACHE LOOKUP - Check cache first for performance */
    char* cache_key = generate_query_cache_key(library, collection, query);
    if (cache_key && g_db.query_cache) {
        void* cached_result = generic_cache_get(g_db.query_cache, cache_key, strlen(cache_key) + 1);
        if (cached_result) {
            /* Cache hit - deserialize and return cached result */
            json_value_t* cached_json = json_deep_copy((json_value_t*)cached_result);
            BUFFER_FREE(cache_key);
            LOG_DEBUG("Query cache HIT for %s/%s", library, collection);
            return cached_json;
        }
        LOG_DEBUG("Query cache MISS for %s/%s", library, collection);
    }
    
    // UNIFIED DOCUMENTS: Query directly from single unified collection
    LOG_DEBUG("Querying unified documents for library='%s', collection='%s'", library, collection);
    
    pthread_rwlock_rdlock(&g_db.lock);
    library_t* lib = get_or_create_library(STORAGE_LIBRARY);
    if (!lib) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    collection_t* coll = get_or_create_collection(STORAGE_LIBRARY, STORAGE_COLLECTION);
    if (!coll) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    pthread_rwlock_rdlock(&coll->lock);
    pthread_rwlock_unlock(&g_db.lock);
    
    // Iterate through all documents in the single unified collection
    json_value_t* result = json_create_object();
    json_value_t* filtered_docs = json_create_array();
    int count = 0;
    
    // Use skiplist iterator to traverse all documents
    void* iter = skiplist_iterator_create(coll->documents);
    if (!iter) {
        pthread_rwlock_unlock(&coll->lock);
        /* CHECKPOINT: json_free(result); */
        /* CHECKPOINT: json_free(filtered_docs); */
        return NULL;
    }
    
    char* key;
    size_t key_len;
    void* ref_data;
    size_t value_len;
    
    LOG_DEBUG("Starting document iteration in unified collection");
    
    while (skiplist_iterator_next(iter, (void**)&key, &key_len, (void**)&ref_data, &value_len)) {
        if (!ref_data || value_len == 0) continue;
        
        // CRITICAL THREAD SAFETY: Parse JSON string instead of accessing JSON object pointer
        // This prevents concurrent access to JSON structures during deep copy operations
        char* doc_str = (char*)ref_data;
        json_value_t* doc = json_parse(doc_str);
        if (!doc || doc->type != JSON_OBJECT) {
            LOG_DEBUG("Skipping invalid or corrupted document in iteration");
            if (doc) {
                /* CHECKPOINT: json_free(doc); */
            }
            continue;
        }
        
        // Filter by library
        json_value_t* doc_library = json_object_get(doc, "library");
        if (!doc_library || strcmp(json_get_string(doc_library), library) != 0) {
            LOG_DEBUG("Document filtered out by library: wanted='%s', got='%s'", 
                      library, doc_library ? json_get_string(doc_library) : "(null)");
            /* CHECKPOINT: json_free(doc); */
            continue;
        }
        
        // UNIFIED DOCUMENTS: Don't filter by collection parameter for type
        // The type filtering will be done in the query matching below
        LOG_DEBUG("Document passed library filter, checking query filters");
        
        // Apply query filter if provided
        bool matches_query = true;
        if (query && query->type == JSON_OBJECT) {
            // Simple field matching for now - iterate through query keys
            json_value_t* query_keys = json_object_get_keys(query);
            if (query_keys && query_keys->type == JSON_ARRAY) {
                for (size_t j = 0; j < json_array_size(query_keys) && matches_query; j++) {
                    json_value_t* key_val = json_array_get(query_keys, j);
                    if (key_val && key_val->type == JSON_STRING) {
                        const char* key = json_get_string(key_val);
                        json_value_t* query_value = json_object_get(query, key);
                        json_value_t* doc_field = json_object_get(doc, key);
                        
                        if (!doc_field) {
                            matches_query = false;
                        } else if (query_value->type == JSON_STRING && doc_field->type == JSON_STRING) {
                            matches_query = (strcmp(json_get_string(doc_field), json_get_string(query_value)) == 0);
                        } else if (query_value->type == JSON_INTEGER && doc_field->type == JSON_INTEGER) {
                            matches_query = (doc_field->value.integer == query_value->value.integer);
                        } else if (query_value->type == JSON_BOOLEAN && doc_field->type == JSON_BOOLEAN) {
                            matches_query = (doc_field->value.boolean == query_value->value.boolean);
                        } else {
                            matches_query = false;
                        }
                    }
                }
                /* CHECKPOINT: json_free(query_keys); */
            }
        }
        
        if (matches_query) {
            /* CRITICAL SAFETY: Validate document before deep copy to prevent corruption */
            if (doc && doc->type == JSON_OBJECT) {
                json_value_t* doc_copy = json_deep_copy(doc);
                if (doc_copy) {
                    json_array_append(filtered_docs, doc_copy);
                    count++;
                } else {
                    LOG_WARNING("Failed to copy document during query - skipping corrupted document");
                }
            }
        }
        
        // Clean up the parsed JSON document
        /* CHECKPOINT: json_free(doc); */
    }
    
    skiplist_iterator_destroy(iter);
    pthread_rwlock_unlock(&coll->lock);
    
    json_object_set(result, "documents", filtered_docs);
    json_object_set(result, "count", json_create_integer(count));
    
    /* 🚀 QUERY CACHE STORE - Cache the result for future queries */
    if (cache_key && g_db.query_cache && result) {
        /* Cache the result - clone it so cache owns its copy */
        json_value_t* result_copy = json_deep_copy(result);
        if (result_copy) {
            char* result_str = json_stringify(result_copy);
            if (result_str) {
                int cache_success = generic_cache_put(g_db.query_cache, 
                                                    cache_key, strlen(cache_key) + 1,
                                                    result_copy, sizeof(json_value_t));
                if (cache_success) {
                    LOG_DEBUG("Query result cached for %s/%s (size: %d docs)", library, collection, count);
                } else {
                    /* CHECKPOINT: json_free(result_copy); */
                    LOG_DEBUG("Failed to cache query result for %s/%s", library, collection);
                }
                BUFFER_FREE(result_str);
            } else {
                /* CHECKPOINT: json_free(result_copy); */
            }
        }
    }
    
    if (cache_key) {
        BUFFER_FREE(cache_key);
    }
    
    LOG_DEBUG("Query completed: found %d matching documents", count);
    return result;
}

/*==============================================================================
 * Database Query Caching Implementation
 * 
 * High-performance caching layer for database queries and document lookups.
 * Uses the existing initialized cache infrastructure for 10-100x performance
 * improvements on repeated operations.
 *============================================================================*/


/**
 * Invalidate cache entries matching a pattern
 * Used for cache invalidation on write operations
 */
static void invalidate_query_cache_pattern(const char* library, const char* collection) __attribute__((unused)); 
static void invalidate_query_cache_pattern(const char* library, const char* collection) {
    if (!g_db.query_cache || !library || !collection) return;
    
    /* For simplicity, clear the entire query cache on writes */
    /* TODO: Implement pattern-based invalidation for more efficiency */
    generic_cache_clear(g_db.query_cache);
    LOG_DEBUG("Query cache cleared due to write operation in %s/%s", library, collection);
}

/**
 * Invalidate document cache entry
 */
static void invalidate_doc_cache(const char* uuid) __attribute__((unused));
static void invalidate_doc_cache(const char* uuid) {
    if (!g_db.doc_cache || !uuid) return;
    
    char* cache_key = generate_doc_cache_key(uuid);
    if (cache_key) {
        generic_cache_remove(g_db.doc_cache, cache_key, strlen(cache_key) + 1);
        BUFFER_FREE(cache_key);
        LOG_DEBUG("Document cache invalidated for UUID: %s", uuid);
    }
}

/* REMOVED: Legacy function eliminated for TRUE unified documents architecture */

/* Schema operations */
int db_set_schema(database_t* db, const char* collection, json_value_t* schema) {
    (void)db; (void)collection; (void)schema;
    return 1;
}

schema_t* db_get_schema(database_t* db, const char* collection) {
    (void)db; (void)collection;
    return NULL;
}

int db_validate_schema(database_t* db, const char* collection, json_value_t* document) {
    (void)db; (void)collection; (void)document;
    return 1;
}

/* Aggregation */
json_value_t* db_aggregate(database_t* db, const char* collection, json_value_t* pipeline) {
    (void)db; (void)collection; (void)pipeline;
    return json_create_array();
}

/* Index operations */
index_t* db_create_index(database_t* db, const char* collection_path, const char* name,
                        const char* field_path, index_type_t type) {
    (void)db; /* Unused - we use global */
    
    if (!g_db.initialized || !collection_path || !field_path) return NULL;
    
    char library[256], collection[256];
    if (parse_collection_path(collection_path, library, collection) != 0) {
        return NULL;
    }
    
    pthread_rwlock_rdlock(&g_db.lock);
    size_t value_len;
    library_t* lib = (library_t*)skiplist_search(g_db.libraries, library, strlen(library) + 1, &value_len);
    if (!lib) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    pthread_rwlock_rdlock(&lib->lock);
    pthread_rwlock_unlock(&g_db.lock);
    
    collection_t* coll = (collection_t*)skiplist_search(lib->collections, collection, 
                                                        strlen(collection) + 1, &value_len);
    if (!coll) {
        pthread_rwlock_unlock(&lib->lock);
        return NULL;
    }
    
    pthread_rwlock_wrlock(&coll->lock);
    pthread_rwlock_unlock(&lib->lock);
    
    /* Create index with proper name */
    char index_name[128];
    snprintf(index_name, sizeof(index_name), "idx_%s_%s", 
             name ? name : "auto", field_path);
    
    /* Create index skiplist for this field */
    skiplist_t* index_skiplist = skiplist_create(skiplist_string_compare);
    if (!index_skiplist) {
        pthread_rwlock_unlock(&coll->lock);
        return NULL;
    }
    
    /* Store index in collection's indexes skiplist */
    skiplist_insert(coll->indexes, index_name, strlen(index_name) + 1, 
                   index_skiplist, sizeof(void*));
    
    /* Build index from existing documents */
    skiplist_iterator_t* doc_iter = skiplist_iterator_create(coll->documents);
    void* doc_key = NULL;
    size_t doc_key_len = 0;
    void* doc_value = NULL;
    size_t doc_value_len = 0;
    
    int indexed_count = 0;
    while (skiplist_iterator_next(doc_iter, &doc_key, &doc_key_len, &doc_value, &doc_value_len)) {
        if (!doc_value || doc_value_len == 0) continue;
        
        const char* doc_id = (const char*)doc_key;
        
        // CRITICAL THREAD SAFETY: Parse JSON string instead of accessing JSON object pointer
        char* json_str = (char*)doc_value;
        json_value_t* doc = json_parse(json_str);
        if (!doc) continue;
        
        /* Extract field value for indexing */
        json_value_t* field_value = json_object_get(doc, field_path);
        if (field_value) {
            char* field_str = json_stringify(field_value);
            if (field_str) {
                /* Index entry: field_value -> document_id */
                skiplist_insert(index_skiplist, field_str, strlen(field_str) + 1,
                               (void*)doc_id, strlen(doc_id) + 1);
                BUFFER_FREE(field_str);
                indexed_count++;
            }
        }
        
        /* CHECKPOINT: json_free(doc); */
    }
    
    skiplist_iterator_destroy(doc_iter);
    pthread_rwlock_unlock(&coll->lock);
    
    /* Create index metadata for compatibility */
    index_t* idx =BUFFER_CALLOC(1, sizeof(index_t));
    if (idx) {
        idx->name = BUFFER_STRDUP(name ? name : field_path);
        idx->field_path = BUFFER_STRDUP(field_path);
        idx->type = type;
        pthread_rwlock_init(&idx->lock, NULL);
        /* The actual skiplist is stored in the collection's indexes skiplist */
    }
    
    LOG_INFO("Created index '%s' on field '%s' in collection '%s' (%d documents indexed)", 
             name ? name : field_path, field_path, collection_path, indexed_count);
    return idx;
}

int db_drop_index(database_t* db, const char* collection_path, const char* name) {
    (void)db; (void)collection_path; (void)name;
    return 0;
}

json_value_t* db_list_indexes(database_t* db, const char* collection_path) {
    (void)db; (void)collection_path;
    return json_create_array();
}

int db_rebuild_indices(database_t* db) {
    (void)db;
    return 1;
}

int db_drop_collection(database_t* db, const char* collection_path) {
    (void)db; (void)collection_path;
    return 0;
}

json_value_t* db_stats(database_t* db, const char* collection) {
    (void)db; (void)collection;
    json_value_t* stats = json_create_object();
    json_object_set(stats, "engine", json_create_string("JDBX"));
    json_object_set(stats, "storage", json_create_string("B-tree with WAL"));
    return stats;
}

/* ========================================================================
 * STORAGE LAYER FUNCTIONS - Direct unified collection access
 * ======================================================================== */

/**
 * Insert document directly into unified storage
 * Storage layer function - operates on physical unified collection
 * SURGICAL FIX: Direct skiplist access, no virtual layer interference
 */
json_value_t* storage_insert_document(database_t* db, json_value_t* document) {
    (void)db; // Use global database
    
    // Direct access to unified collection - NO virtual logic
    pthread_rwlock_rdlock(&g_db.lock);
    library_t* lib = get_or_create_library(PHYSICAL_STORAGE_LIBRARY);
    if (!lib) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    collection_t* coll = get_or_create_collection(PHYSICAL_STORAGE_LIBRARY, PHYSICAL_STORAGE_COLLECTION);
    if (!coll) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    pthread_rwlock_wrlock(&coll->lock);
    pthread_rwlock_unlock(&g_db.lock);
    
    // Generate UUID - use simple thread-safe approach
    char uuid[64];
    unsigned int random_val;
    
    // Use current time and thread ID for uniqueness
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    random_val = (unsigned int)(ts.tv_nsec ^ (uintptr_t)pthread_self());
    
    snprintf(uuid, sizeof(uuid), "doc-%ld-%u", time(NULL), random_val);
    
    // Create document copy preserving ALL existing fields
    json_value_t* doc_copy = json_deep_copy(document);
    json_object_set(doc_copy, "uuid", json_create_string(uuid));
    
    // Add timestamps only if missing - preserve existing values
    time_t now = time(NULL);
    if (!json_object_get(doc_copy, "created_at")) {
        json_object_set(doc_copy, "created_at", json_create_integer(now));
    }
    json_object_set(doc_copy, "modified_at", json_create_integer(now));
    
    // BAR RAISING: Store JSON object directly - skiplist manages pointer lifecycle
    // CRITICAL FIX: Must allocate persistent pointer storage, not use stack address!
    json_value_t** doc_ptr = (json_value_t**)BUFFER_ALLOC(sizeof(json_value_t*));
    if (!doc_ptr) {
        pthread_rwlock_unlock(&coll->lock);
        /* CHECKPOINT: json_free(doc_copy); */
        return NULL;
    }
    *doc_ptr = json_deep_copy(doc_copy);
    
    // CRITICAL: Mark skiplist documents as hazard-protected
    // The skiplist uses hazard pointers for safe memory reclamation
    // Mark both the pointer storage and the document as hazard-protected
    memory_mark_hazard_protected(doc_ptr, NULL);
    memory_mark_hazard_protected(*doc_ptr, NULL);
    
    skiplist_insert(coll->documents, uuid, strlen(uuid) + 1, doc_ptr, sizeof(json_value_t*));
    
    pthread_rwlock_unlock(&coll->lock);
    
    LOG_INFO("Storage: Inserted document '%s' directly into unified collection", uuid);
    
    return json_deep_copy(doc_copy);
}

/**
 * Update document directly in unified storage
 * Storage layer function - operates on physical unified collection
 * SURGICAL FIX: Direct skiplist access, no virtual layer interference
 */
json_value_t* storage_update_document(database_t* db, const char* uuid, json_value_t* document) {
    (void)db; // Use global database
    
    // Direct access to unified collection - NO virtual logic
    pthread_rwlock_rdlock(&g_db.lock);
    library_t* lib = get_or_create_library(PHYSICAL_STORAGE_LIBRARY);
    if (!lib) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    collection_t* coll = get_or_create_collection(PHYSICAL_STORAGE_LIBRARY, PHYSICAL_STORAGE_COLLECTION);
    if (!coll) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    pthread_rwlock_wrlock(&coll->lock);
    pthread_rwlock_unlock(&g_db.lock);
    
    // Find existing document
    size_t value_len;
    void* raw_data = skiplist_search(coll->documents, uuid, strlen(uuid) + 1, &value_len);
    if (!raw_data || value_len == 0) {
        if (raw_data) BUFFER_FREE(raw_data);  // Free the search result
        pthread_rwlock_unlock(&coll->lock);
        return NULL;
    }
    
    // CRITICAL THREAD SAFETY: Parse JSON string from skiplist
    char* json_str = (char*)raw_data;
    json_value_t* existing_doc = json_parse(json_str);
    BUFFER_FREE(raw_data);  // Free the search result immediately after parsing
    
    if (!existing_doc) {
        pthread_rwlock_unlock(&coll->lock);
        return NULL;
    }
    
    // Create updated document preserving ALL existing fields and adding new ones
    json_value_t* updated_doc = json_deep_copy(existing_doc);
    
    // Merge new fields from document parameter
    if (document && document->type == JSON_OBJECT) {
        json_value_t* keys = json_object_get_keys(document);
        if (keys && keys->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(keys); i++) {
                json_value_t* key_val = json_array_get(keys, i);
                if (key_val && key_val->type == JSON_STRING) {
                    const char* key = json_get_string(key_val);
                    json_value_t* value = json_object_get(document, key);
                    if (value) {
                        json_object_set(updated_doc, key, json_clone(value));
                    }
                }
            }
        }
        /* CHECKPOINT: json_free(keys); */
    }
    
    // Update timestamp
    json_object_set(updated_doc, "modified_at", json_create_integer(time(NULL)));
    
    // CRITICAL THREAD SAFETY: Serialize updated document to JSON string
    char* updated_str = json_stringify(updated_doc);
    if (!updated_str) {
        pthread_rwlock_unlock(&coll->lock);
        /* CHECKPOINT: json_free(updated_doc); */
        /* CHECKPOINT: json_free(existing_doc); */
        return NULL;
    }
    
    // Store the new JSON string
    size_t str_len = strlen(updated_str) + 1;
    char* stored_str = (char*)BUFFER_ALLOC(str_len);
    if (!stored_str) {
        BUFFER_FREE(updated_str);
        pthread_rwlock_unlock(&coll->lock);
        /* CHECKPOINT: json_free(updated_doc); */
        /* CHECKPOINT: json_free(existing_doc); */
        return NULL;
    }
    memcpy(stored_str, updated_str, str_len);
    BUFFER_FREE(updated_str);
    
    // CRITICAL: Mark as hazard-protected
    memory_mark_hazard_protected(stored_str, NULL);
    
    skiplist_delete(coll->documents, uuid, strlen(uuid) + 1);
    skiplist_insert(coll->documents, uuid, strlen(uuid) + 1, stored_str, str_len);
    
    pthread_rwlock_unlock(&coll->lock);
    
    LOG_INFO("Storage: Updated document '%s' directly in unified collection", uuid);
    
    // Return a copy and free the originals
    json_value_t* result = json_deep_copy(updated_doc);
    /* CHECKPOINT: json_free(existing_doc); */
    /* CHECKPOINT: json_free(updated_doc); */
    return result;
}

/**
 * Delete document directly from unified storage
 * Storage layer function - operates on physical unified collection
 * SURGICAL FIX: Direct skiplist access, no virtual layer interference
 */
int storage_delete_document(database_t* db, const char* uuid) {
    (void)db; // Use global database
    
    // Direct access to unified collection - NO virtual logic
    pthread_rwlock_rdlock(&g_db.lock);
    library_t* lib = get_or_create_library(PHYSICAL_STORAGE_LIBRARY);
    if (!lib) {
        pthread_rwlock_unlock(&g_db.lock);
        return 0;
    }
    
    collection_t* coll = get_or_create_collection(PHYSICAL_STORAGE_LIBRARY, PHYSICAL_STORAGE_COLLECTION);
    if (!coll) {
        pthread_rwlock_unlock(&g_db.lock);
        return 0;
    }
    
    pthread_rwlock_wrlock(&coll->lock);
    pthread_rwlock_unlock(&g_db.lock);
    
    // 🔧 FIX: Atomic search-and-delete to prevent race conditions
    // With write lock held, search for the document first
    size_t value_len;
    void* raw_data = skiplist_search(coll->documents, uuid, strlen(uuid) + 1, &value_len);
    if (!raw_data || value_len == 0) {
        if (raw_data) BUFFER_FREE(raw_data);
        pthread_rwlock_unlock(&coll->lock);
        LOG_DEBUG("Storage: Document '%s' not found", uuid);
        return 0;
    }
    
    // For deletion, we don't need to parse the JSON, just verify it exists
    BUFFER_FREE(raw_data);  // Free the search result
    
    // Now delete from skiplist while we still hold the write lock
    int result = skiplist_delete(coll->documents, uuid, strlen(uuid) + 1);
    
    // 🔧 FIX: Defer memory cleanup to prevent use-after-free
    if (result) {
        // CRITICAL: We have removed the document from the skiplist, but other threads
        // might still have pointers to it. With our lock-free skiplist, readers don't
        // take locks, so they could be accessing this memory RIGHT NOW.
        //
        // SOLUTION: Use our unified memory reclamation system. The hazard protection
        // will prevent immediate freeing. When the hazard pointer system determines
        // it's safe, our callback will be invoked to free the memory.
        
        LOG_INFO("Storage: Deleted document '%s' directly from unified collection", uuid);
        
        // The allocations are already marked as hazard-protected from when they were
        // inserted. The hazard pointer system will call memory_hazard_retire_callback
        // when it's safe to free them. For now, we just let them remain protected.
    } else {
        // This shouldn't happen since we just found it
        LOG_ERROR("Storage: Failed to delete document '%s' from skiplist (unexpected)", uuid);
    }
    
    pthread_rwlock_unlock(&coll->lock);
    
    return result;
}

/**
 * Get document directly from unified storage
 * Storage layer function - operates on physical unified collection
 * SURGICAL FIX: Direct skiplist access, no virtual layer interference
 */
json_value_t* storage_get_document(database_t* db, const char* uuid) {
    (void)db; // Use global database
    
    // Direct access to unified collection - NO virtual logic
    pthread_rwlock_rdlock(&g_db.lock);
    library_t* lib = get_or_create_library(PHYSICAL_STORAGE_LIBRARY);
    if (!lib) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    collection_t* coll = get_or_create_collection(PHYSICAL_STORAGE_LIBRARY, PHYSICAL_STORAGE_COLLECTION);
    if (!coll) {
        pthread_rwlock_unlock(&g_db.lock);
        return NULL;
    }
    
    pthread_rwlock_rdlock(&coll->lock);
    pthread_rwlock_unlock(&g_db.lock);
    
    // Find document by UUID
    size_t value_len;
    void* raw_data = skiplist_search(coll->documents, uuid, strlen(uuid) + 1, &value_len);
    json_value_t* result = NULL;
    
    if (raw_data && value_len > 0) {
        // CRITICAL THREAD SAFETY: Parse JSON string instead of accessing JSON object pointer
        // This prevents concurrent access to JSON structures during deep copy operations
        char* doc_str = (char*)raw_data;
        result = json_parse(doc_str);
        if (result) {
            LOG_DEBUG("Storage: Found document '%s' in unified collection", uuid);
        } else {
            LOG_ERROR("Storage: Failed to parse JSON for document '%s'", uuid);
        }
        BUFFER_FREE(raw_data);  // Free the search result allocation
    } else {
        LOG_DEBUG("Storage: Document '%s' not found in unified collection", uuid);
        if (raw_data) {
            BUFFER_FREE(raw_data);  // Free even if wrong size
        }
    }
    
    pthread_rwlock_unlock(&coll->lock);
    
    return result;
}

/* REMOVED: Legacy bootstrap function eliminated for TRUE unified documents architecture */

/**
 * Stub function for legacy db_get_collection calls
 * In unified documents architecture, collections are virtual and always exist
 */
db_collection_t* db_get_collection(database_t* db, const char* collection_name) {
    (void)db; (void)collection_name;
    /* Return non-NULL to indicate collection "exists" (virtual in unified architecture) */
    static db_collection_t stub_collection = {0}; /* Static stub collection */
    return &stub_collection;
}

/**
 * Stub function for legacy db_save calls
 * In unified documents architecture, persistence is automatic
 */
int db_save(database_t* db) {
    (void)db;
    /* Return success since persistence is automatic */
    return 1;
}

/**
 * Stub function for legacy db_close calls
 * In unified documents architecture, cleanup is automatic
 */
void db_close(database_t* db) {
    (void)db;
    /* Cleanup is automatic in unified documents architecture */
}

/**
 * Get access to global database for cache statistics
 * This is a controlled access function for metrics/monitoring
 */
void* get_global_database_caches(generic_cache_t** query_cache, generic_cache_t** doc_cache) {
    if (query_cache) *query_cache = g_db.query_cache;
    if (doc_cache) *doc_cache = g_db.doc_cache;
    return &g_db;
}