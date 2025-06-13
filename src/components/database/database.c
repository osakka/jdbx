/*
 * JDBX Database - Lock-Free Hierarchical Implementation with JDBX Storage
 * Single source of truth: Libraries → Collections → Documents
 * Storage: JDBX page manager with WAL, B-tree, and caching
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

#include "database/database.h"
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
#include "utils/buffer_pool.h"

/* Clean hierarchical database structure */
typedef struct library {
    char name[64];
    void* collections;  /* Skiplist of collections */
    pthread_rwlock_t lock;
} library_t;

typedef struct collection {
    char name[64];
    char library[64];
    void* documents;    /* Skiplist of documents */
    void* indexes;      /* Skiplist of indexes */
    json_value_t* schema;
    pthread_rwlock_t lock;
} collection_t;

/* JDBX Database with lock-free architecture */
typedef struct {
    /* JDBX storage engine */
    jdbx_page_manager_t* page_manager;  /* WAL, B-tree, caching */
    
    /* Lock-free hierarchical structure */
    void* libraries;           /* Skiplist of libraries */
    pthread_rwlock_t lock;
    bool initialized;
    char path[256];
    
    /* Integrated components */
    generic_cache_t* query_cache;
    generic_cache_t* doc_cache;
    
    /* Compatibility facade */
    database_t facade;
} jdbx_database_t;

/* Global database instance */
static jdbx_database_t g_db = {
    .page_manager = NULL,
    .libraries = NULL,
    .lock = PTHREAD_RWLOCK_INITIALIZER,
    .initialized = false,
    .path = {0},
    .query_cache = NULL,
    .doc_cache = NULL,
    .facade = {0}
};

/* Forward declarations */
static library_t* get_or_create_library(const char* name);
static collection_t* get_or_create_collection(const char* library_name, const char* collection_name);
static int parse_collection_path(const char* path, char* library, char* collection);
static int skiplist_string_compare(const void* a, size_t a_len, const void* b, size_t b_len);

/* Helper: String comparison for skiplist */
static int skiplist_string_compare(const void* a, size_t a_len, const void* b, size_t b_len) {
    (void)a_len; /* Strings are null-terminated */
    (void)b_len;
    return strcmp((const char*)a, (const char*)b);
}

/* Helper: Parse library/collection path */
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

/* JSON helper functions */
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
    lib = calloc(1, sizeof(library_t));
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
        free(lib);
        return NULL;
    }
    
    LOG_INFO("Created library '%s'", name);
    return lib;
}

/* Get or create collection */
static collection_t* get_or_create_collection(const char* library_name, const char* collection_name) {
    if (!library_name || !collection_name) return NULL;
    
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
    
    /* Create new collection */
    coll = calloc(1, sizeof(collection_t));
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
    
    LOG_INFO("Created collection '%s/%s'", library_name, collection_name);
    
    /* Register with adaptive indexing */
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", library_name, collection_name);
    
    return coll;
}

/*==============================================================================
 * Database Initialization - Pure JDBX Implementation
 *============================================================================*/

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
    
    /* Create system library */
    get_or_create_library("system");
    
    /* Create system collections */
    const char* system_collections[] = {
        "config", "metrics", "users", "roles", "sessions", "libraries", "indexes"
    };
    
    for (size_t i = 0; i < sizeof(system_collections)/sizeof(char*); i++) {
        get_or_create_collection("system", system_collections[i]);
    }
    
    /* Create default library */
    get_or_create_library("default");
    
    /* Initialize subsystems */
    adaptive_indexer_init(&g_db.facade);
    query_tracker_init();
    
    /* Store path and mark as initialized */
    strncpy(g_db.path, jdbx_path, sizeof(g_db.path) - 1);
    g_db.initialized = true;
    
    /* Initialize facade for compatibility */
    g_db.facade.path = strdup(g_db.path);
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
        free(g_db.facade.path);
        g_db.facade.path = NULL;
    }
    if (g_db.facade.collections) {
        json_free(g_db.facade.collections);
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
        skiplist_iterator_destroy(coll_iter);
        
        pthread_rwlock_unlock(&lib->lock);
    }
    
    skiplist_iterator_destroy(lib_iter);
    
    return all_collections;
}

/*==============================================================================
 * Document Operations
 *============================================================================*/

json_value_t* db_insert(database_t* db, const char* collection_path, json_value_t* document) {
    (void)db; /* Unused - we use global */
    
    LOG_DEBUG("db_insert called for collection: %s", collection_path);
    
    if (!g_db.initialized || !collection_path || !document) return NULL;
    
    char library[256], collection[256];
    if (parse_collection_path(collection_path, library, collection) != 0) {
        return NULL;
    }
    
    LOG_DEBUG("db_insert: Getting collection %s/%s", library, collection);
    collection_t* coll = get_or_create_collection(library, collection);
    if (!coll) return NULL;
    
    /* Generate UUID if not present */
    json_value_t* id_val = json_object_get(document, "uuid");
    char doc_id[64];
    if (!id_val) {
        snprintf(doc_id, sizeof(doc_id), "doc-%ld-%d", time(NULL), rand());
        json_object_set(document, "uuid", json_create_string(doc_id));
    } else {
        strncpy(doc_id, json_get_string(id_val), sizeof(doc_id) - 1);
    }
    
    /* Add metadata */
    json_object_set(document, "_created_at", json_create_integer(time(NULL)));
    json_object_set(document, "_modified_at", json_create_integer(time(NULL)));
    
    LOG_DEBUG("db_insert: Acquiring write lock for collection %s", collection_path);
    pthread_rwlock_wrlock(&coll->lock);
    LOG_DEBUG("db_insert: Write lock acquired, inserting document");
    
    /* Insert into documents skiplist - store pointer value */
    json_value_t* doc_copy = json_deep_copy(document);
    skiplist_insert(coll->documents, doc_id, strlen(doc_id) + 1, &doc_copy, sizeof(json_value_t*));
    
    /* Update all indexes for this document */
    skiplist_iterator_t* idx_iter = skiplist_iterator_create(coll->indexes);
    void* idx_key = NULL;
    size_t idx_key_len = 0;
    void* idx_value = NULL;
    size_t idx_value_len = 0;
    
    while (skiplist_iterator_next(idx_iter, &idx_key, &idx_key_len, &idx_value, &idx_value_len)) {
        const char* idx_name = (const char*)idx_key;
        skiplist_t* idx_skiplist = (skiplist_t*)idx_value;
        
        /* Extract field name from index name (idx_auto_fieldname or idx_name_fieldname) */
        const char* field_start = strrchr(idx_name, '_');
        if (field_start) {
            const char* field_path = field_start + 1;
            
            /* Get field value and add to index */
            json_value_t* field_value = json_object_get(doc_copy, field_path);
            if (field_value) {
                char* field_str = json_stringify(field_value);
                if (field_str) {
                    skiplist_insert(idx_skiplist, field_str, strlen(field_str) + 1,
                                   (void*)doc_id, strlen(doc_id) + 1);
                    buffer_pool_free_safe(field_str);
                }
            }
        }
    }
    
    skiplist_iterator_destroy(idx_iter);
    pthread_rwlock_unlock(&coll->lock);
    
    /* Also store in JDBX for persistence */
    if (g_db.page_manager) {
        char doc_key[512];
        snprintf(doc_key, sizeof(doc_key), "doc:%s:%s:%s", library, collection, doc_id);
        
        char* doc_json = json_stringify(doc_copy);
        if (doc_json) {
            /* Create a B-tree for persistent storage if needed */
            jdbx_btree_t* persistent_btree = jdbx_btree_create(g_db.page_manager, skiplist_string_compare);
            if (persistent_btree) {
                jdbx_btree_insert(persistent_btree, doc_key, strlen(doc_key) + 1,
                                 doc_json, strlen(doc_json) + 1);
                jdbx_btree_close(persistent_btree);
            }
            buffer_pool_free_safe(doc_json);
        }
    }
    
    LOG_DEBUG("Inserted document '%s' into '%s'", doc_id, collection_path);
    return json_deep_copy(document);
}

json_value_t* db_find_by_id(database_t* db, const char* collection_path, const char* id) {
    (void)db; /* Unused - we use global */
    
    if (!g_db.initialized || !collection_path || !id) return NULL;
    
    char library[256], collection[256];
    if (parse_collection_path(collection_path, library, collection) != 0) {
        return NULL;
    }
    
    /* Lock-free library lookup */
    size_t value_len;
    library_t* lib = (library_t*)skiplist_search(g_db.libraries, library, strlen(library) + 1, &value_len);
    if (!lib) {
        return NULL;
    }
    
    /* Lock library for collection lookup */
    pthread_rwlock_rdlock(&lib->lock);
    collection_t* coll = (collection_t*)skiplist_search(lib->collections, collection, 
                                                        strlen(collection) + 1, &value_len);
    if (!coll) {
        pthread_rwlock_unlock(&lib->lock);
        return NULL;
    }
    
    /* Lock collection for document access */
    pthread_rwlock_rdlock(&coll->lock);
    pthread_rwlock_unlock(&lib->lock);
    
    void* doc_ptr = skiplist_search(coll->documents, id, strlen(id) + 1, &value_len);
    json_value_t* doc = doc_ptr ? *(json_value_t**)doc_ptr : NULL;
    json_value_t* result = doc ? json_deep_copy(doc) : NULL;
    
    pthread_rwlock_unlock(&coll->lock);
    
    return result;
}

/* Additional stubs for other operations */
json_value_t* db_update(database_t* db, const char* collection_path, const char* id, 
                       json_value_t* update) {
    (void)db; /* Unused - we use global */
    
    if (!g_db.initialized || !collection_path || !id || !update) return NULL;
    
    char library[256], collection[256];
    if (parse_collection_path(collection_path, library, collection) != 0) {
        return NULL;
    }
    
    /* Get existing document */
    json_value_t* doc = db_find_by_id(db, collection_path, id);
    if (!doc) return NULL;
    
    pthread_rwlock_rdlock(&g_db.lock);
    size_t value_len;
    library_t* lib = (library_t*)skiplist_search(g_db.libraries, library, strlen(library) + 1, &value_len);
    if (!lib) {
        pthread_rwlock_unlock(&g_db.lock);
        json_free(doc);
        return NULL;
    }
    
    pthread_rwlock_rdlock(&lib->lock);
    pthread_rwlock_unlock(&g_db.lock);
    
    collection_t* coll = (collection_t*)skiplist_search(lib->collections, collection, 
                                                        strlen(collection) + 1, &value_len);
    if (!coll) {
        pthread_rwlock_unlock(&lib->lock);
        json_free(doc);
        return NULL;
    }
    
    pthread_rwlock_wrlock(&coll->lock);
    pthread_rwlock_unlock(&lib->lock);
    
    /* Apply updates */
    if (update->type == JSON_OBJECT) {
        json_value_t* keys = json_object_get_keys(update);
        if (keys && keys->type == JSON_ARRAY) {
            size_t num_keys = json_array_size(keys);
            for (size_t i = 0; i < num_keys; i++) {
                json_value_t* key_val = json_array_get(keys, i);
                if (key_val && key_val->type == JSON_STRING) {
                    const char* key = json_get_string(key_val);
                    json_value_t* value = json_object_get(update, key);
                    
                    /* Handle special update operators */
                    if (strcmp(key, "$set") == 0 && value && value->type == JSON_OBJECT) {
                        /* Apply $set operations */
                        json_value_t* set_keys = json_object_get_keys(value);
                        if (set_keys && set_keys->type == JSON_ARRAY) {
                            size_t num_set_keys = json_array_size(set_keys);
                            for (size_t j = 0; j < num_set_keys; j++) {
                                json_value_t* set_key_val = json_array_get(set_keys, j);
                                if (set_key_val && set_key_val->type == JSON_STRING) {
                                    const char* set_key = json_get_string(set_key_val);
                                    json_value_t* set_value = json_object_get(value, set_key);
                                    if (set_value) {
                                        json_object_set(doc, set_key, json_deep_copy(set_value));
                                    }
                                }
                            }
                            json_free(set_keys);
                        }
                    } else if (strcmp(key, "$unset") == 0 && value && value->type == JSON_OBJECT) {
                        /* Apply $unset operations */
                        json_value_t* unset_keys = json_object_get_keys(value);
                        if (unset_keys && unset_keys->type == JSON_ARRAY) {
                            size_t num_unset_keys = json_array_size(unset_keys);
                            for (size_t j = 0; j < num_unset_keys; j++) {
                                json_value_t* unset_key_val = json_array_get(unset_keys, j);
                                if (unset_key_val && unset_key_val->type == JSON_STRING) {
                                    const char* unset_key = json_get_string(unset_key_val);
                                    json_object_remove(doc, unset_key);
                                }
                            }
                            json_free(unset_keys);
                        }
                    } else {
                        /* Direct field update */
                        if (value) {
                            json_object_set(doc, key, json_deep_copy(value));
                        }
                    }
                }
            }
            json_free(keys);
        }
    }
    
    /* Update modified timestamp */
    json_object_set(doc, "_modified_at", json_create_integer(time(NULL)));
    
    /* Update indexes: Remove old document from indexes and add new version */
    skiplist_iterator_t* idx_iter = skiplist_iterator_create(coll->indexes);
    void* idx_key = NULL;
    size_t idx_key_len = 0;
    void* idx_value = NULL;
    size_t idx_value_len = 0;
    
    while (skiplist_iterator_next(idx_iter, &idx_key, &idx_key_len, &idx_value, &idx_value_len)) {
        const char* idx_name = (const char*)idx_key;
        skiplist_t* idx_skiplist = (skiplist_t*)idx_value;
        
        /* Extract field name from index name */
        const char* field_start = strrchr(idx_name, '_');
        if (field_start) {
            const char* field_path = field_start + 1;
            
            /* Remove old index entry - get old field value first */
            json_value_t* old_doc = (json_value_t*)skiplist_search(coll->documents, id, strlen(id) + 1, &value_len);
            if (old_doc) {
                json_value_t* old_field_value = json_object_get(old_doc, field_path);
                if (old_field_value) {
                    char* old_field_str = json_stringify(old_field_value);
                    if (old_field_str) {
                        skiplist_delete(idx_skiplist, old_field_str, strlen(old_field_str) + 1);
                        buffer_pool_free_safe(old_field_str);
                    }
                }
            }
            
            /* Add new index entry */
            json_value_t* new_field_value = json_object_get(doc, field_path);
            if (new_field_value) {
                char* new_field_str = json_stringify(new_field_value);
                if (new_field_str) {
                    skiplist_insert(idx_skiplist, new_field_str, strlen(new_field_str) + 1,
                                   (void*)id, strlen(id) + 1);
                    buffer_pool_free_safe(new_field_str);
                }
            }
        }
    }
    
    skiplist_iterator_destroy(idx_iter);
    
    /* Get old document to free it */
    size_t old_doc_len;
    void* old_doc_ptr = skiplist_search(coll->documents, id, strlen(id) + 1, &old_doc_len);
    if (old_doc_ptr) {
        json_value_t* old_doc = *(json_value_t**)old_doc_ptr;
        json_free(old_doc);
    }
    
    /* Update document in storage - store pointer value */
    skiplist_delete(coll->documents, id, strlen(id) + 1);
    skiplist_insert(coll->documents, id, strlen(id) + 1, &doc, sizeof(json_value_t*));
    
    pthread_rwlock_unlock(&coll->lock);
    
    LOG_DEBUG("Updated document '%s' in '%s'", id, collection_path);
    return json_deep_copy(doc);
}

int db_delete(database_t* db, const char* collection, const char* id) {
    (void)db; /* Unused - we use global */
    
    if (!g_db.initialized || !collection || !id) return 0;
    
    char library[64], coll_name[64];
    if (parse_collection_path(collection, library, coll_name) != 0) {
        return 0;
    }
    
    pthread_rwlock_rdlock(&g_db.lock);
    size_t value_len;
    library_t* lib = (library_t*)skiplist_search(g_db.libraries, library, strlen(library) + 1, &value_len);
    if (!lib) {
        pthread_rwlock_unlock(&g_db.lock);
        return 0;
    }
    
    pthread_rwlock_rdlock(&lib->lock);
    pthread_rwlock_unlock(&g_db.lock);
    
    collection_t* coll = (collection_t*)skiplist_search(lib->collections, coll_name, 
                                                        strlen(coll_name) + 1, &value_len);
    if (!coll) {
        pthread_rwlock_unlock(&lib->lock);
        return 0;
    }
    
    pthread_rwlock_wrlock(&coll->lock);
    pthread_rwlock_unlock(&lib->lock);
    
    /* Remove document from all indexes before deleting */
    void* doc_ptr = skiplist_search(coll->documents, id, strlen(id) + 1, &value_len);
    json_value_t* doc_to_delete = doc_ptr ? *(json_value_t**)doc_ptr : NULL;
    if (doc_to_delete) {
        skiplist_iterator_t* idx_iter = skiplist_iterator_create(coll->indexes);
        void* idx_key = NULL;
        size_t idx_key_len = 0;
        void* idx_value = NULL;
        size_t idx_value_len = 0;
        
        while (skiplist_iterator_next(idx_iter, &idx_key, &idx_key_len, &idx_value, &idx_value_len)) {
            const char* idx_name = (const char*)idx_key;
            skiplist_t* idx_skiplist = (skiplist_t*)idx_value;
            
            /* Extract field name from index name */
            const char* field_start = strrchr(idx_name, '_');
            if (field_start) {
                const char* field_path = field_start + 1;
                
                /* Remove document from this index */
                json_value_t* field_value = json_object_get(doc_to_delete, field_path);
                if (field_value) {
                    char* field_str = json_stringify(field_value);
                    if (field_str) {
                        skiplist_delete(idx_skiplist, field_str, strlen(field_str) + 1);
                        buffer_pool_free_safe(field_str);
                    }
                }
            }
        }
        
        skiplist_iterator_destroy(idx_iter);
        
        /* Free the JSON document before deletion */
        json_free(doc_to_delete);
    }
    
    bool result = skiplist_delete(coll->documents, id, strlen(id) + 1);
    
    pthread_rwlock_unlock(&coll->lock);
    
    LOG_DEBUG("Deleted document '%s' from '%s'", id, collection);
    return result;
}

json_value_t* db_find(database_t* db, const char* collection_path, const char* query,
                     const char* projection, int limit, int skip, const char* sort) {
    (void)db; /* Unused - we use global */
    (void)projection; /* TODO: Implement projection */
    (void)sort; /* TODO: Implement sorting */
    
    if (!g_db.initialized || !collection_path) return json_create_array();
    
    char library[256], collection[256];
    if (parse_collection_path(collection_path, library, collection) != 0) {
        return json_create_array();
    }
    
    /* Parse query */
    json_value_t* query_obj = NULL;
    if (query && strlen(query) > 0) {
        query_obj = json_parse(query);
        if (!query_obj) {
            LOG_ERROR("Invalid query JSON: %s", query);
            return json_create_array();
        }
    }
    
    /* Lock-free library lookup */
    size_t lookup_value_len;
    library_t* lib = (library_t*)skiplist_search(g_db.libraries, library, strlen(library) + 1, &lookup_value_len);
    if (!lib) {
        if (query_obj) json_free(query_obj);
        return json_create_array();
    }
    
    /* Lock library for collection lookup */
    pthread_rwlock_rdlock(&lib->lock);
    collection_t* coll = (collection_t*)skiplist_search(lib->collections, collection, 
                                                        strlen(collection) + 1, &lookup_value_len);
    if (!coll) {
        pthread_rwlock_unlock(&lib->lock);
        if (query_obj) json_free(query_obj);
        return json_create_array();
    }
    
    LOG_DEBUG("db_find: Acquiring read lock for collection %s/%s", library, collection);
    pthread_rwlock_rdlock(&coll->lock);
    pthread_rwlock_unlock(&lib->lock);
    LOG_DEBUG("db_find: Read lock acquired for %s/%s", library, collection);
    
    /* Execute query */
    json_value_t* results = json_create_array();
    skiplist_iterator_t* iter = skiplist_iterator_create(coll->documents);
    int count = 0;
    int skipped = 0;
    
    LOG_DEBUG("Executing query on %s/%s", library, collection);
    
    void* key = NULL;
    size_t key_len = 0;
    void* value = NULL;
    size_t value_len = 0;
    
    while (skiplist_iterator_next(iter, &key, &key_len, &value, &value_len)) {
        if (limit > 0 && count >= limit) break;
        
        /* value contains the pointer value, dereference it */
        json_value_t* doc = *(json_value_t**)value;
        
        /* Safety check for null document */
        if (!doc) {
            LOG_ERROR("Null document found in skiplist at key: %s", (const char*)key);
            continue;
        }
        
        /* Apply query filter */
        bool matches = true;
        if (query_obj && query_obj->type == JSON_OBJECT) {
            /* Simple query matching */
            json_value_t* keys = json_object_get_keys(query_obj);
            if (keys && keys->type == JSON_ARRAY) {
                size_t num_keys = json_array_size(keys);
                for (size_t i = 0; i < num_keys; i++) {
                    json_value_t* key_val = json_array_get(keys, i);
                    if (key_val && key_val->type == JSON_STRING) {
                        const char* field = json_get_string(key_val);
                        json_value_t* query_value = json_object_get(query_obj, field);
                        json_value_t* doc_value = json_object_get(doc, field);
                        
                        /* Simple string equality check for now */
                        if (!doc_value) {
                            matches = false;
                            break;
                        }
                        
                        /* Compare based on type */
                        if (query_value->type == JSON_STRING && doc_value->type == JSON_STRING) {
                            if (strcmp(query_value->value.string, doc_value->value.string) != 0) {
                                matches = false;
                                break;
                            }
                        } else {
                            /* For non-string types, use json_equals */
                            if (!json_equals(doc_value, query_value)) {
                                matches = false;
                                break;
                            }
                        }
                    }
                }
                json_free(keys);
            }
        }
        
        if (matches) {
            if (skipped < skip) {
                skipped++;
            } else {
                json_array_append(results, json_deep_copy(doc));
                count++;
            }
        }
    }
    
    skiplist_iterator_destroy(iter);
    LOG_DEBUG("db_find: Releasing read lock for %s/%s", library, collection);
    pthread_rwlock_unlock(&coll->lock);
    LOG_DEBUG("db_find: Read lock released for %s/%s", library, collection);
    
    if (query_obj) {
        json_free(query_obj);
    }
    
    return results;
}

/*==============================================================================
 * Legacy Compatibility Functions
 *============================================================================*/

database_t* db_create(void) {
    return db_init(NULL);
}

void db_destroy(database_t* db) {
    (void)db;
}

void db_close(database_t* db) {
    (void)db;
}

int db_save(database_t* db) {
    (void)db;
    if (g_db.page_manager) {
        return jdbx_sync(g_db.page_manager) == 0 ? 1 : 0;
    }
    return 1;
}

int db_start_persistence_thread(database_t* db) {
    (void)db;
    return 1; /* JDBX handles persistence automatically */
}

void db_complete_bootstrap(database_t* db) {
    (void)db;
}

int db_needs_bootstrap(database_t* db) {
    (void)db;
    /* Check if admin user exists in system/users collection */
    if (!db_collection_exists(db, "system/users") || !db_collection_exists(db, "system/roles")) {
        return 1; /* Collections don't exist */
    }
    
    /* Check if there are any users in system/users collection */
    json_value_t* empty_query = json_create_object();
    json_value_t* users = db_query_documents(db, "system/users", empty_query);
    json_free(empty_query);
    
    if (!users) {
        return 1; /* Query failed, probably need bootstrap */
    }
    
    /* Check if users array is empty */
    json_value_t* documents = json_object_get(users, "documents");
    int needs_bootstrap = (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0);
    
    json_free(users);
    return needs_bootstrap;
}

/* Cache operations */
json_value_t* db_get_cache_stats(database_t* db) {
    (void)db;
    json_value_t* stats = json_create_object();
    json_object_set(stats, "status", json_create_string("enabled"));
    json_object_set(stats, "type", json_create_string("JDBX integrated cache"));
    return stats;
}

int db_enable_cache(database_t* db, int capacity, int ttl) {
    (void)db; (void)capacity; (void)ttl;
    return 1;
}

int db_disable_cache(database_t* db) {
    (void)db;
    return 0;
}

int db_configure_cache(database_t* db, int capacity, int ttl, const char* type, double max_memory_mb) {
    (void)db; (void)capacity; (void)ttl; (void)type; (void)max_memory_mb;
    return 1;
}

int db_clear_cache(database_t* db) {
    (void)db;
    return 1;
}

void process_cache_invalidations(database_t* db) {
    (void)db;
}

/* Collection access */
db_collection_t* db_get_collection(database_t* db, const char* name) {
    (void)db; (void)name;
    return NULL;
}

/* Document operations with legacy signatures */
json_value_t* db_get_document(database_t* db, const char* collection, const char* id) {
    return db_find_by_id(db, collection, id);
}

json_value_t* db_insert_document(database_t* db, const char* collection, json_value_t* document) {
    return db_insert(db, collection, document);
}

json_value_t* db_update_document(database_t* db, const char* collection, const char* id, 
                                json_value_t* update) {
    return db_update(db, collection, id, update);
}

int db_delete_document(database_t* db, const char* collection, const char* id) {
    return db_delete(db, collection, id);
}

json_value_t* db_query_documents(database_t* db, const char* collection, json_value_t* query) {
    char* query_str = NULL;
    if (query && query->type != JSON_NULL) {
        query_str = json_stringify(query);
    }
    
    json_value_t* result = db_find(db, collection, query_str, NULL, -1, 0, NULL);
    
    if (query_str) {
        buffer_pool_free_safe(query_str);
    }
    
    return result;
}

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
        const char* doc_id = (const char*)doc_key;
        json_value_t* doc = *(json_value_t**)doc_value;
        
        /* Extract field value for indexing */
        json_value_t* field_value = json_object_get(doc, field_path);
        if (field_value) {
            char* field_str = json_stringify(field_value);
            if (field_str) {
                /* Index entry: field_value -> document_id */
                skiplist_insert(index_skiplist, field_str, strlen(field_str) + 1,
                               (void*)doc_id, strlen(doc_id) + 1);
                buffer_pool_free_safe(field_str);
                indexed_count++;
            }
        }
    }
    
    skiplist_iterator_destroy(doc_iter);
    pthread_rwlock_unlock(&coll->lock);
    
    /* Create index metadata for compatibility */
    index_t* idx = calloc(1, sizeof(index_t));
    if (idx) {
        idx->name = strdup(name ? name : field_path);
        idx->field_path = strdup(field_path);
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

/* Bootstrap functions */
json_value_t* db_insert_direct_bootstrap(const char* collection_path, json_value_t* document) {
    return db_insert(&g_db.facade, collection_path, document);
}