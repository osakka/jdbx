/*
 * JDBX Database Implementation
 * Single file database with everything inside
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "database/jdbx_database.h"
#include "utils/logger.h"
#include "utils/json_helpers.h"
#include "utils/buffer_pool.h"

/* Define json_serialize if not available */
#ifndef json_serialize
#define json_serialize json_stringify
#endif

/* JSON helper functions */
static void json_object_set_string(json_value_t* obj, const char* key, const char* value) {
    if (!obj || obj->type != JSON_OBJECT || !key) return;
    json_value_t* str_val = json_create_string(value);
    if (str_val) {
        json_object_set(obj, key, str_val);
    }
}

static void json_object_set_number(json_value_t* obj, const char* key, double value) {
    if (!obj || obj->type != JSON_OBJECT || !key) return;
    json_value_t* num_val = json_create_number(value);
    if (num_val) {
        json_object_set(obj, key, num_val);
    }
}

/* Constants */
#define COLLECTIONS_DIR_KEY "_collections_directory"
#define MAX_KEY_SIZE 1024
#define CACHE_SIZE_PER_COLLECTION (1024 * 1024 * 50)  /* 50MB cache per collection */

/* Helper: Generate document ID */
static void generate_doc_id(char* id_buf, size_t buf_size) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    
    unsigned int rand_val = 0;
    FILE* f = fopen("/dev/urandom", "r");
    if (f) {
        fread(&rand_val, sizeof(rand_val), 1, f);
        fclose(f);
    } else {
        rand_val = (unsigned int)ts.tv_nsec;
    }
    
    snprintf(id_buf, buf_size, "doc-%ld-%09ld-%u", 
             ts.tv_sec, ts.tv_nsec, rand_val);
}

/* Helper: String comparison for B-tree */
static int string_compare(const void* a, size_t a_len, const void* b, size_t b_len) {
    size_t min_len = a_len < b_len ? a_len : b_len;
    int cmp = memcmp(a, b, min_len);
    if (cmp != 0) return cmp;
    return (int)a_len - (int)b_len;
}

/* Helper: Load collection metadata */
static int load_collection_metadata(jdbx_database_t* db, const char* name,
                                   jdbx_collection_t* meta, uint64_t* meta_page) {
    void* value = NULL;
    size_t value_len = 0;
    
    /* Look up collection in directory */
    if (jdbx_btree_get(db->collections_dir, name, strlen(name), 
                       &value, &value_len) != 0) {
        return -1;  /* Collection not found */
    }
    
    if (value_len != sizeof(uint64_t)) {
        free(value);
        return -1;
    }
    
    *meta_page = *(uint64_t*)value;
    free(value);
    
    /* Read metadata page */
    page_header_t* page = jdbx_get_page(db->pm, *meta_page);
    if (!page || page->type != PAGE_TYPE_COLLECTION_META) {
        LOG_ERROR("Invalid collection metadata page %llu", 
                  (unsigned long long)*meta_page);
        return -1;
    }
    
    /* Copy metadata */
    memcpy(meta, (char*)page + sizeof(page_header_t), sizeof(jdbx_collection_t));
    return 0;
}

/* Helper: Save collection metadata */
static int save_collection_metadata(jdbx_database_t* db, const char* name,
                                   jdbx_collection_t* meta) {
    /* Allocate metadata page if needed */
    uint64_t meta_page = 0;
    void* value = NULL;
    size_t value_len = 0;
    
    /* Check if collection already exists */
    if (jdbx_btree_get(db->collections_dir, name, strlen(name),
                       &value, &value_len) == 0) {
        meta_page = *(uint64_t*)value;
        free(value);
    } else {
        /* Allocate new page */
        meta_page = jdbx_alloc_page(db->pm, PAGE_TYPE_COLLECTION_META);
        if (meta_page == 0) {
            LOG_ERROR("Failed to allocate metadata page");
            return -1;
        }
    }
    
    /* Write metadata to page */
    page_header_t* page = jdbx_get_page_for_write(db->pm, meta_page);
    if (!page) {
        LOG_ERROR("Failed to get metadata page for write");
        return -1;
    }
    
    /* Copy metadata after header */
    memcpy((char*)page + sizeof(page_header_t), meta, sizeof(jdbx_collection_t));
    
    /* Update directory */
    if (jdbx_btree_insert(db->collections_dir, name, strlen(name),
                          &meta_page, sizeof(meta_page)) != 0) {
        LOG_ERROR("Failed to update collections directory");
        return -1;
    }
    
    return 0;
}

/* Helper: Find or load collection */
static int find_collection(jdbx_database_t* db, const char* name, int* index) {
    /* Check cache first */
    for (int i = 0; i < db->num_collections; i++) {
        if (strcmp(db->collections[i].name, name) == 0) {
            *index = i;
            return 0;
        }
    }
    
    /* Load from disk */
    if (db->num_collections >= 1024) {
        LOG_ERROR("Too many collections loaded");
        return -1;
    }
    
    jdbx_collection_t meta;
    uint64_t meta_page;
    if (load_collection_metadata(db, name, &meta, &meta_page) != 0) {
        return -1;  /* Collection not found */
    }
    
    /* Add to cache */
    int idx = db->num_collections++;
    strncpy(db->collections[idx].name, name, 255);
    db->collections[idx].name[255] = '\0';
    memcpy(&db->collections[idx].meta, &meta, sizeof(meta));
    
    /* Open B-trees */
    db->collections[idx].doc_tree = jdbx_btree_open(db->pm, meta.doc_tree_root, string_compare);
    db->collections[idx].primary_idx = jdbx_btree_open(db->pm, meta.primary_idx_root, string_compare);
    
    /* Open secondary indexes */
    for (int i = 0; i < meta.num_secondary_indexes; i++) {
        db->collections[idx].secondary_idx[i] = 
            jdbx_btree_open(db->pm, meta.secondary_idx_roots[i], string_compare);
    }
    
    /* Initialize cache */
    db->collections[idx].doc_cache = generic_cache_create(CACHE_SIZE_PER_COLLECTION);
    pthread_rwlock_init(&db->collections[idx].lock, NULL);
    
    *index = idx;
    return 0;
}

/*==============================================================================
 * Public API Implementation
 *============================================================================*/

jdbx_database_t* jdbx_database_create(const char* path) {
    jdbx_database_t* db = calloc(1, sizeof(jdbx_database_t));
    if (!db) return NULL;
    
    /* Create JDBX file */
    db->pm = jdbx_create(path, 1024 * 1024 * 100); /* 100MB initial */
    if (!db->pm) {
        LOG_ERROR("Failed to create JDBX file: %s", path);
        free(db);
        return NULL;
    }
    
    /* Create collections directory B-tree */
    db->collections_dir = jdbx_btree_create(db->pm, string_compare);
    if (!db->collections_dir) {
        LOG_ERROR("Failed to create collections directory");
        jdbx_close(db->pm);
        free(db);
        return NULL;
    }
    
    /* Store collections directory root in header */
    uint64_t* dir_root = (uint64_t*)&db->pm->header->reserved[8];
    *dir_root = db->collections_dir->root_page;
    
    /* Sync header */
    if (jdbx_sync(db->pm) != 0) {
        LOG_ERROR("Failed to sync JDBX header");
        jdbx_btree_close(db->collections_dir);
        jdbx_close(db->pm);
        free(db);
        return NULL;
    }
    
    pthread_rwlock_init(&db->global_lock, NULL);
    
    LOG_INFO("Created JDBX database: %s", path);
    return db;
}

jdbx_database_t* jdbx_database_open(const char* path) {
    jdbx_database_t* db = calloc(1, sizeof(jdbx_database_t));
    if (!db) return NULL;
    
    /* Open JDBX file */
    db->pm = jdbx_open(path);
    if (!db->pm) {
        LOG_ERROR("Failed to open JDBX file: %s", path);
        free(db);
        return NULL;
    }
    
    /* Read collections directory root from header */
    uint64_t* dir_root = (uint64_t*)&db->pm->header->reserved[8];
    if (*dir_root == 0) {
        LOG_ERROR("No collections directory found in JDBX");
        jdbx_close(db->pm);
        free(db);
        return NULL;
    }
    
    /* Open collections directory B-tree */
    db->collections_dir = jdbx_btree_open(db->pm, *dir_root, string_compare);
    if (!db->collections_dir) {
        LOG_ERROR("Failed to open collections directory");
        jdbx_close(db->pm);
        free(db);
        return NULL;
    }
    
    pthread_rwlock_init(&db->global_lock, NULL);
    
    LOG_INFO("Opened JDBX database: %s", path);
    return db;
}

void jdbx_database_close(jdbx_database_t* db) {
    if (!db) return;
    
    /* Close all loaded collections */
    for (int i = 0; i < db->num_collections; i++) {
        if (db->collections[i].doc_tree) {
            jdbx_btree_close(db->collections[i].doc_tree);
        }
        if (db->collections[i].primary_idx) {
            jdbx_btree_close(db->collections[i].primary_idx);
        }
        for (int j = 0; j < db->collections[i].meta.num_secondary_indexes; j++) {
            if (db->collections[i].secondary_idx[j]) {
                jdbx_btree_close(db->collections[i].secondary_idx[j]);
            }
        }
        if (db->collections[i].doc_cache) {
            generic_cache_destroy(db->collections[i].doc_cache);
        }
        pthread_rwlock_destroy(&db->collections[i].lock);
    }
    
    /* Close collections directory */
    if (db->collections_dir) {
        jdbx_btree_close(db->collections_dir);
    }
    
    /* Close page manager */
    if (db->pm) {
        jdbx_sync(db->pm);
        jdbx_close(db->pm);
    }
    
    pthread_rwlock_destroy(&db->global_lock);
    free(db);
}

int jdbx_create_collection(jdbx_database_t* db, const char* name) {
    if (!db || !name) return -1;
    
    pthread_rwlock_wrlock(&db->global_lock);
    
    /* Check if collection already exists */
    void* value = NULL;
    size_t value_len = 0;
    if (jdbx_btree_get(db->collections_dir, name, strlen(name),
                       &value, &value_len) == 0) {
        free(value);
        pthread_rwlock_unlock(&db->global_lock);
        LOG_ERROR("Collection '%s' already exists", name);
        return -1;
    }
    
    /* Create collection metadata */
    jdbx_collection_t meta = {0};
    strncpy(meta.name, name, 255);
    meta.created_at = meta.updated_at = time(NULL);
    
    /* Create B-trees for the collection */
    jdbx_btree_t* doc_tree = jdbx_btree_create(db->pm, string_compare);
    if (!doc_tree) {
        pthread_rwlock_unlock(&db->global_lock);
        LOG_ERROR("Failed to create document tree");
        return -1;
    }
    meta.doc_tree_root = doc_tree->root_page;
    jdbx_btree_close(doc_tree);
    
    jdbx_btree_t* idx_tree = jdbx_btree_create(db->pm, string_compare);
    if (!idx_tree) {
        pthread_rwlock_unlock(&db->global_lock);
        LOG_ERROR("Failed to create index tree");
        return -1;
    }
    meta.primary_idx_root = idx_tree->root_page;
    jdbx_btree_close(idx_tree);
    
    /* Save metadata */
    if (save_collection_metadata(db, name, &meta) != 0) {
        pthread_rwlock_unlock(&db->global_lock);
        LOG_ERROR("Failed to save collection metadata");
        return -1;
    }
    
    /* Update stats */
    db->stats.total_collections++;
    
    pthread_rwlock_unlock(&db->global_lock);
    
    LOG_INFO("Created collection '%s' in JDBX", name);
    return 0;
}

int jdbx_insert_document(jdbx_database_t* db, const char* collection,
                        const char* id, json_value_t* doc) {
    if (!db || !collection || !doc) return -1;
    
    pthread_rwlock_rdlock(&db->global_lock);
    
    /* Find collection */
    int coll_idx;
    if (find_collection(db, collection, &coll_idx) != 0) {
        pthread_rwlock_unlock(&db->global_lock);
        LOG_ERROR("Collection '%s' not found", collection);
        return -1;
    }
    
    pthread_rwlock_wrlock(&db->collections[coll_idx].lock);
    pthread_rwlock_unlock(&db->global_lock);
    
    /* Generate ID if not provided */
    char doc_id[256];
    if (!id || strlen(id) == 0) {
        generate_doc_id(doc_id, sizeof(doc_id));
        id = doc_id;
    }
    
    /* Set _id field in document */
    json_object_set_string(doc, "_id", id);
    
    /* Serialize document */
    char* json_str = json_serialize(doc);
    if (!json_str) {
        pthread_rwlock_unlock(&db->collections[coll_idx].lock);
        LOG_ERROR("Failed to serialize document");
        return -1;
    }
    
    /* Insert into document tree */
    if (jdbx_btree_insert(db->collections[coll_idx].doc_tree,
                          id, strlen(id), json_str, strlen(json_str) + 1) != 0) {
        free(json_str);
        pthread_rwlock_unlock(&db->collections[coll_idx].lock);
        LOG_ERROR("Failed to insert document into tree");
        return -1;
    }
    
    /* Insert into primary index (id -> id for fast lookups) */
    if (jdbx_btree_insert(db->collections[coll_idx].primary_idx,
                          id, strlen(id), id, strlen(id) + 1) != 0) {
        /* Rollback document insertion */
        jdbx_btree_delete(db->collections[coll_idx].doc_tree, id, strlen(id));
        free(json_str);
        pthread_rwlock_unlock(&db->collections[coll_idx].lock);
        LOG_ERROR("Failed to update primary index");
        return -1;
    }
    
    /* Update secondary indexes */
    for (int i = 0; i < db->collections[coll_idx].meta.num_secondary_indexes; i++) {
        const char* field = db->collections[coll_idx].meta.secondary_idx_fields[i];
        json_value_t* field_val = json_object_get(doc, field);
        if (field_val) {
            char* field_str = json_serialize(field_val);
            if (field_str) {
                /* For simplicity, store id as value (could store array of ids) */
                jdbx_btree_insert(db->collections[coll_idx].secondary_idx[i],
                                 field_str, strlen(field_str),
                                 id, strlen(id) + 1);
                free(field_str);
            }
        }
    }
    
    /* Update metadata */
    db->collections[coll_idx].meta.doc_count++;
    db->collections[coll_idx].meta.total_size += strlen(json_str);
    db->collections[coll_idx].meta.updated_at = time(NULL);
    
    /* Save updated metadata */
    save_collection_metadata(db, collection, &db->collections[coll_idx].meta);
    
    /* Add to cache */
    if (db->collections[coll_idx].doc_cache) {
        char* doc_copy = json_serialize(doc);
        if (doc_copy) {
            generic_cache_put(db->collections[coll_idx].doc_cache, id, strlen(id), 
                            doc_copy, strlen(doc_copy) + 1);
            free(doc_copy);
        }
    }
    
    free(json_str);
    pthread_rwlock_unlock(&db->collections[coll_idx].lock);
    
    /* Sync to disk */
    jdbx_sync(db->pm);
    
    return 0;
}

json_value_t* jdbx_get_document(jdbx_database_t* db, const char* collection,
                               const char* id) {
    if (!db || !collection || !id) return NULL;
    
    pthread_rwlock_rdlock(&db->global_lock);
    
    /* Find collection */
    int coll_idx;
    if (find_collection(db, collection, &coll_idx) != 0) {
        pthread_rwlock_unlock(&db->global_lock);
        LOG_ERROR("Collection '%s' not found", collection);
        return NULL;
    }
    
    pthread_rwlock_rdlock(&db->collections[coll_idx].lock);
    pthread_rwlock_unlock(&db->global_lock);
    
    /* Check cache first */
    if (db->collections[coll_idx].doc_cache) {
        char* cached_str = generic_cache_get(db->collections[coll_idx].doc_cache, id, strlen(id));
        if (cached_str) {
            json_value_t* cached_doc = json_parse(cached_str);
            pthread_rwlock_unlock(&db->collections[coll_idx].lock);
            return cached_doc;
        }
    }
    
    /* Retrieve from tree */
    void* value = NULL;
    size_t value_len = 0;
    
    if (jdbx_btree_get(db->collections[coll_idx].doc_tree,
                       id, strlen(id), &value, &value_len) != 0) {
        pthread_rwlock_unlock(&db->collections[coll_idx].lock);
        return NULL;
    }
    
    /* Parse JSON */
    json_value_t* doc = json_parse((char*)value);
    
    /* Add to cache */
    if (doc && db->collections[coll_idx].doc_cache) {
        generic_cache_put(db->collections[coll_idx].doc_cache, id, strlen(id),
                         value, value_len);
    }
    
    free(value);
    
    pthread_rwlock_unlock(&db->collections[coll_idx].lock);
    
    return doc;
}

json_value_t* jdbx_list_collections(jdbx_database_t* db) {
    if (!db) return NULL;
    
    pthread_rwlock_rdlock(&db->global_lock);
    
    json_value_t* result = json_create_array();
    
    /* Iterate through collections directory */
    jdbx_btree_iterator_t* iter = jdbx_btree_iterator_create(db->collections_dir);
    if (iter) {
        void* key = NULL;
        size_t key_len = 0;
        void* value = NULL;
        size_t value_len = 0;
        
        while (jdbx_btree_iterator_next(iter, &key, &key_len, &value, &value_len) == 0) {
            /* Create collection info object */
            json_value_t* coll_info = json_create_object();
            
            /* Collection name */
            char* name = malloc(key_len + 1);
            memcpy(name, key, key_len);
            name[key_len] = '\0';
            json_object_set_string(coll_info, "name", name);
            
            /* Load metadata for stats */
            jdbx_collection_t meta;
            uint64_t meta_page = *(uint64_t*)value;
            if (load_collection_metadata(db, name, &meta, &meta_page) == 0) {
                json_object_set_number(coll_info, "count", (double)meta.doc_count);
                json_object_set_number(coll_info, "size", (double)meta.total_size);
                json_object_set_number(coll_info, "indexes", (double)meta.num_secondary_indexes);
            }
            
            json_array_append(result, coll_info);
            
            free(name);
            free(key);
            free(value);
        }
        
        jdbx_btree_iterator_destroy(iter);
    }
    
    pthread_rwlock_unlock(&db->global_lock);
    
    return result;
}

/* Helper: Match document against query */
static int match_document(json_value_t* doc, json_value_t* query) {
    if (!query || query->type != JSON_OBJECT) {
        return 1; /* Empty query matches all */
    }
    
    /* Get query keys */
    json_value_t* keys = json_object_get_keys(query);
    if (!keys || keys->type != JSON_ARRAY) {
        return 1; /* No keys to match */
    }
    
    size_t num_keys = json_array_size(keys);
    
    /* Check each query field */
    for (size_t i = 0; i < num_keys; i++) {
        json_value_t* key_val = json_array_get(keys, i);
        if (!key_val || key_val->type != JSON_STRING) continue;
        
        const char* field = json_get_string(key_val);
        json_value_t* expected = json_object_get(query, field);
        json_value_t* actual = json_object_get(doc, field);
        
        /* Handle operators */
        if (field[0] == '$') {
            /* Query operator - not field match */
            continue;
        }
        
        /* Simple equality check */
        if (!actual) {
            json_free(keys);
            return 0; /* Field not found */
        }
        
        /* Compare values */
        if (expected->type != actual->type) {
            json_free(keys);
            return 0;
        }
        
        switch (expected->type) {
            case JSON_STRING:
                if (strcmp(json_get_string(expected), json_get_string(actual)) != 0) {
                    json_free(keys);
                    return 0;
                }
                break;
                
            case JSON_NUMBER:
                if (json_get_number(expected) != json_get_number(actual)) {
                    json_free(keys);
                    return 0;
                }
                break;
                
            case JSON_BOOLEAN:
                if (json_get_boolean(expected) != json_get_boolean(actual)) {
                    json_free(keys);
                    return 0;
                }
                break;
                
            case JSON_NULL:
                /* Both are null - match */
                break;
                
            default:
                /* Complex types - TODO: Deep comparison */
                json_free(keys);
                return 0;
        }
    }
    
    json_free(keys);
    return 1; /* All fields match */
}

json_value_t* jdbx_find_documents(jdbx_database_t* db, const char* collection,
                                 json_value_t* query, int limit, int skip) {
    if (!db || !collection) return NULL;
    
    pthread_rwlock_rdlock(&db->global_lock);
    
    /* Find collection */
    int coll_idx;
    if (find_collection(db, collection, &coll_idx) != 0) {
        pthread_rwlock_unlock(&db->global_lock);
        LOG_ERROR("Collection '%s' not found", collection);
        return NULL;
    }
    
    pthread_rwlock_rdlock(&db->collections[coll_idx].lock);
    pthread_rwlock_unlock(&db->global_lock);
    
    json_value_t* results = json_create_array();
    int skipped = 0;
    int found = 0;
    
    /* Iterate through all documents */
    jdbx_btree_iterator_t* iter = jdbx_btree_iterator_create(db->collections[coll_idx].doc_tree);
    if (iter) {
        void* key = NULL;
        size_t key_len = 0;
        void* value = NULL;
        size_t value_len = 0;
        
        while (jdbx_btree_iterator_next(iter, &key, &key_len, &value, &value_len) == 0) {
            /* Parse document */
            json_value_t* doc = json_parse((char*)value);
            if (doc) {
                /* Check if document matches query */
                if (match_document(doc, query)) {
                    /* Handle skip */
                    if (skipped < skip) {
                        skipped++;
                        json_free(doc);
                    } else {
                        /* Add to results */
                        json_array_append(results, doc);
                        found++;
                        
                        /* Check limit */
                        if (limit > 0 && found >= limit) {
                            free(key);
                            free(value);
                            break;
                        }
                    }
                } else {
                    json_free(doc);
                }
            }
            
            free(key);
            free(value);
        }
        
        jdbx_btree_iterator_destroy(iter);
    }
    
    pthread_rwlock_unlock(&db->collections[coll_idx].lock);
    
    return results;
}

int jdbx_update_document(jdbx_database_t* db, const char* collection,
                        const char* id, json_value_t* doc) {
    if (!db || !collection || !id || !doc) return -1;
    
    pthread_rwlock_rdlock(&db->global_lock);
    
    /* Find collection */
    int coll_idx;
    if (find_collection(db, collection, &coll_idx) != 0) {
        pthread_rwlock_unlock(&db->global_lock);
        LOG_ERROR("Collection '%s' not found", collection);
        return -1;
    }
    
    pthread_rwlock_wrlock(&db->collections[coll_idx].lock);
    pthread_rwlock_unlock(&db->global_lock);
    
    /* Ensure _id is set */
    json_object_set_string(doc, "_id", id);
    
    /* Serialize document */
    char* json_str = json_serialize(doc);
    if (!json_str) {
        pthread_rwlock_unlock(&db->collections[coll_idx].lock);
        LOG_ERROR("Failed to serialize document");
        return -1;
    }
    
    /* Update in tree (insert overwrites) */
    if (jdbx_btree_insert(db->collections[coll_idx].doc_tree,
                          id, strlen(id), json_str, strlen(json_str) + 1) != 0) {
        free(json_str);
        pthread_rwlock_unlock(&db->collections[coll_idx].lock);
        LOG_ERROR("Failed to update document in tree");
        return -1;
    }
    
    /* Update cache */
    if (db->collections[coll_idx].doc_cache) {
        generic_cache_put(db->collections[coll_idx].doc_cache, id, strlen(id),
                         json_str, strlen(json_str) + 1);
    }
    
    /* Update metadata */
    db->collections[coll_idx].meta.updated_at = time(NULL);
    save_collection_metadata(db, collection, &db->collections[coll_idx].meta);
    
    free(json_str);
    pthread_rwlock_unlock(&db->collections[coll_idx].lock);
    
    /* Sync to disk */
    jdbx_sync(db->pm);
    
    return 0;
}

int jdbx_delete_document(jdbx_database_t* db, const char* collection,
                        const char* id) {
    if (!db || !collection || !id) return -1;
    
    pthread_rwlock_rdlock(&db->global_lock);
    
    /* Find collection */
    int coll_idx;
    if (find_collection(db, collection, &coll_idx) != 0) {
        pthread_rwlock_unlock(&db->global_lock);
        LOG_ERROR("Collection '%s' not found", collection);
        return -1;
    }
    
    pthread_rwlock_wrlock(&db->collections[coll_idx].lock);
    pthread_rwlock_unlock(&db->global_lock);
    
    /* Delete from tree */
    if (jdbx_btree_delete(db->collections[coll_idx].doc_tree, id, strlen(id)) != 0) {
        pthread_rwlock_unlock(&db->collections[coll_idx].lock);
        LOG_ERROR("Failed to delete document from tree");
        return -1;
    }
    
    /* Delete from primary index */
    jdbx_btree_delete(db->collections[coll_idx].primary_idx, id, strlen(id));
    
    /* Remove from cache */
    if (db->collections[coll_idx].doc_cache) {
        generic_cache_remove(db->collections[coll_idx].doc_cache, id, strlen(id));
    }
    
    /* Update metadata */
    db->collections[coll_idx].meta.doc_count--;
    db->collections[coll_idx].meta.updated_at = time(NULL);
    save_collection_metadata(db, collection, &db->collections[coll_idx].meta);
    
    pthread_rwlock_unlock(&db->collections[coll_idx].lock);
    
    /* Sync to disk */
    jdbx_sync(db->pm);
    
    return 0;
}

int jdbx_collection_exists(jdbx_database_t* db, const char* name) {
    if (!db || !name) return 0;
    
    pthread_rwlock_rdlock(&db->global_lock);
    
    void* value = NULL;
    size_t value_len = 0;
    int exists = (jdbx_btree_get(db->collections_dir, name, strlen(name),
                                 &value, &value_len) == 0);
    if (value) free(value);
    
    pthread_rwlock_unlock(&db->global_lock);
    
    return exists;
}

int jdbx_drop_collection(jdbx_database_t* db, const char* name) {
    if (!db || !name) return -1;
    
    pthread_rwlock_wrlock(&db->global_lock);
    
    /* Load metadata to get tree roots */
    jdbx_collection_t meta;
    uint64_t meta_page;
    if (load_collection_metadata(db, name, &meta, &meta_page) != 0) {
        pthread_rwlock_unlock(&db->global_lock);
        LOG_ERROR("Collection '%s' not found", name);
        return -1;
    }
    
    /* TODO: Free all pages used by the collection's B-trees */
    /* For now, just remove from directory */
    
    /* Remove from directory */
    if (jdbx_btree_delete(db->collections_dir, name, strlen(name)) != 0) {
        pthread_rwlock_unlock(&db->global_lock);
        LOG_ERROR("Failed to remove collection from directory");
        return -1;
    }
    
    /* Free metadata page */
    jdbx_free_page(db->pm, meta_page);
    
    /* Update stats */
    db->stats.total_collections--;
    
    pthread_rwlock_unlock(&db->global_lock);
    
    LOG_INFO("Dropped collection '%s'", name);
    return 0;
}

int jdbx_create_index(jdbx_database_t* db, const char* collection,
                     const char* field, int unique) {
    (void)unique; /* TODO: Implement unique constraint checking */
    if (!db || !collection || !field) return -1;
    
    pthread_rwlock_rdlock(&db->global_lock);
    
    /* Find collection */
    int coll_idx;
    if (find_collection(db, collection, &coll_idx) != 0) {
        pthread_rwlock_unlock(&db->global_lock);
        LOG_ERROR("Collection '%s' not found", collection);
        return -1;
    }
    
    pthread_rwlock_wrlock(&db->collections[coll_idx].lock);
    pthread_rwlock_unlock(&db->global_lock);
    
    /* Check if we have room for another index */
    if (db->collections[coll_idx].meta.num_secondary_indexes >= 16) {
        pthread_rwlock_unlock(&db->collections[coll_idx].lock);
        LOG_ERROR("Maximum number of indexes reached");
        return -1;
    }
    
    /* Create new B-tree for index */
    jdbx_btree_t* idx_tree = jdbx_btree_create(db->pm, string_compare);
    if (!idx_tree) {
        pthread_rwlock_unlock(&db->collections[coll_idx].lock);
        LOG_ERROR("Failed to create index tree");
        return -1;
    }
    
    /* Add to metadata */
    int idx_num = db->collections[coll_idx].meta.num_secondary_indexes++;
    db->collections[coll_idx].meta.secondary_idx_roots[idx_num] = idx_tree->root_page;
    strncpy(db->collections[coll_idx].meta.secondary_idx_fields[idx_num], 
            field, 127);
    
    /* Save to runtime */
    db->collections[coll_idx].secondary_idx[idx_num] = idx_tree;
    
    /* TODO: Populate index with existing documents */
    
    /* Save metadata */
    save_collection_metadata(db, collection, &db->collections[coll_idx].meta);
    
    pthread_rwlock_unlock(&db->collections[coll_idx].lock);
    
    LOG_INFO("Created index on '%s' for collection '%s'", field, collection);
    return 0;
}

json_value_t* jdbx_get_stats(jdbx_database_t* db, const char* collection) {
    if (!db) return NULL;
    
    json_value_t* stats = json_create_object();
    
    if (collection) {
        /* Collection-specific stats */
        pthread_rwlock_rdlock(&db->global_lock);
        
        jdbx_collection_t meta;
        uint64_t meta_page;
        if (load_collection_metadata(db, collection, &meta, &meta_page) == 0) {
            json_object_set_string(stats, "name", collection);
            json_object_set_number(stats, "count", (double)meta.doc_count);
            json_object_set_number(stats, "size", (double)meta.total_size);
            json_object_set_number(stats, "indexes", (double)meta.num_secondary_indexes);
            
            /* Index details */
            if (meta.num_secondary_indexes > 0) {
                json_value_t* indexes = json_create_array();
                for (int i = 0; i < meta.num_secondary_indexes; i++) {
                    json_value_t* idx = json_create_object();
                    json_object_set_string(idx, "field", meta.secondary_idx_fields[i]);
                    json_array_append(indexes, idx);
                }
                json_object_set(stats, "indexDetails", indexes);
            }
        }
        
        pthread_rwlock_unlock(&db->global_lock);
    } else {
        /* Database-wide stats */
        pthread_rwlock_rdlock(&db->global_lock);
        
        json_object_set_number(stats, "collections", 
                             (double)db->stats.total_collections);
        json_object_set_number(stats, "documents", 
                             (double)db->stats.total_documents);
        json_object_set_number(stats, "size", 
                             (double)db->stats.total_size);
        
        /* Page manager stats */
        if (db->pm) {
            json_value_t* pm_stats = json_create_object();
            json_object_set_number(pm_stats, "totalPages", 
                                 (double)db->pm->header->total_pages);
            json_object_set_number(pm_stats, "freePages", 
                                 (double)db->pm->header->free_pages);
            json_object_set_number(pm_stats, "pageReads", 
                                 (double)db->pm->stats.page_reads);
            json_object_set_number(pm_stats, "pageWrites", 
                                 (double)db->pm->stats.page_writes);
            json_object_set_number(pm_stats, "cacheHits", 
                                 (double)db->pm->stats.cache_hits);
            json_object_set_number(pm_stats, "cacheMisses", 
                                 (double)db->pm->stats.cache_misses);
            json_object_set(stats, "pageManager", pm_stats);
        }
        
        pthread_rwlock_unlock(&db->global_lock);
    }
    
    return stats;
}

int jdbx_drop_index(jdbx_database_t* db, const char* collection,
                   const char* field) {
    /* TODO: Implement index dropping */
    (void)db;
    (void)collection;
    (void)field;
    LOG_WARNING("Index dropping not yet implemented");
    return -1;
}