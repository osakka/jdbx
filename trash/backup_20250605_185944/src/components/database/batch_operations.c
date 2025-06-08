#include "database/batch_operations.h"
#include "database/database.h"
#include "storage/mmap_storage.h"
#include "index/hash_index.h"
#include "utils/json_helpers.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

/* Suppress unused parameter warnings */
#define UNUSED(x) ((void)(x))

/* Get collection definition from database.c */
typedef struct {
    char name[256];
    mmap_storage_t* storage;
    hash_index_t* primary_index;
    void* indexes[16];  /* Secondary indexes */
    int num_indexes;
    void* cache;
    pthread_rwlock_t lock;
    atomic_uint_fast64_t doc_count;
    atomic_uint_fast64_t total_size;
} hp_collection_t;

/* External function to find collection */
extern hp_collection_t* find_collection_internal(const char* name);

/* Timer helper */
static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/* Generate UUID v4 document ID for batch operations */
static void generate_batch_doc_id(char* id_buf, size_t buf_size, size_t batch_idx) {
    UNUSED(batch_idx); /* Use same UUID logic regardless of batch position */
    
    /* Generate 16 bytes of random data for UUID v4 */
    unsigned char uuid_bytes[16];
    for (int i = 0; i < 16; i++) {
        uuid_bytes[i] = rand() & 0xFF;
    }
    
    /* Set version (4) and variant bits according to RFC 4122 */
    uuid_bytes[6] = (uuid_bytes[6] & 0x0F) | 0x40; /* Version 4 */
    uuid_bytes[8] = (uuid_bytes[8] & 0x3F) | 0x80; /* Variant bits */
    
    /* Format as standard UUID string: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx */
    snprintf(id_buf, buf_size, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid_bytes[0], uuid_bytes[1], uuid_bytes[2], uuid_bytes[3],
             uuid_bytes[4], uuid_bytes[5], uuid_bytes[6], uuid_bytes[7],
             uuid_bytes[8], uuid_bytes[9], uuid_bytes[10], uuid_bytes[11],
             uuid_bytes[12], uuid_bytes[13], uuid_bytes[14], uuid_bytes[15]);
}

/* Process a single batch of documents */
static int process_document_batch(
    hp_collection_t* coll,
    json_value_t** docs,
    size_t count,
    batch_insert_result_t* result,
    const batch_insert_options_t* options
) {
    int errors = 0;
    
    /* Pre-allocate buffers for efficiency */
    char** json_strings = calloc(count, sizeof(char*));
    char** doc_ids = calloc(count, sizeof(char*));
    size_t* doc_sizes = calloc(count, sizeof(size_t));
    
    if (!json_strings || !doc_ids || !doc_sizes) {
        free(json_strings);
        free(doc_ids);
        free(doc_sizes);
        return -1;
    }
    
    /* Phase 1: Prepare all documents */
    for (size_t i = 0; i < count; i++) {
        json_value_t* doc = docs[i];
        if (!doc || doc->type != JSON_OBJECT) {
            errors++;
            continue;
        }
        
        /* Generate or extract ID */
        doc_ids[i] = malloc(256);
        json_value_t* id_field = json_object_get(doc, "_id");
        if (!id_field) {
            id_field = json_object_get(doc, "id");
        }
        
        if (!id_field || id_field->type != JSON_STRING) {
            generate_batch_doc_id(doc_ids[i], 256, result->total_documents + i);
            json_object_set(doc, "_id", json_create_string(doc_ids[i]));
        } else {
            strncpy(doc_ids[i], id_field->value.string, 255);
            doc_ids[i][255] = '\0';
            if (!json_object_get(doc, "_id")) {
                json_object_set(doc, "_id", json_create_string(doc_ids[i]));
            }
        }
        
        /* Serialize document */
        json_strings[i] = json_stringify(doc);
        if (json_strings[i]) {
            doc_sizes[i] = strlen(json_strings[i]);
        } else {
            errors++;
        }
    }
    
    /* Phase 2: Bulk storage operations */
    pthread_rwlock_wrlock(&coll->lock);
    
    for (size_t i = 0; i < count; i++) {
        if (!json_strings[i]) continue;
        
        /* Check for duplicates if requested */
        if (!options->ignore_duplicates) {
            uint64_t existing_offset;
            if (hash_index_search(coll->primary_index, doc_ids[i], 
                                 strlen(doc_ids[i]), &existing_offset) == 0) {
                /* Document already exists */
                if (result->errors) {
                    json_value_t* error = json_create_object();
                    json_object_set(error, "id", json_create_string(doc_ids[i]));
                    json_object_set(error, "error", json_create_string("duplicate_key"));
                    json_array_append(result->errors, error);
                }
                errors++;
                continue;
            }
        }
        
        /* Store in mmap */
        uint64_t offset = coll->storage->header->free_offset;
        if (mmap_storage_put(coll->storage, doc_ids[i], strlen(doc_ids[i]), 
                            json_strings[i], doc_sizes[i]) != 0) {
            errors++;
            continue;
        }
        
        /* Update primary index */
        if (hash_index_insert(coll->primary_index, doc_ids[i], 
                             strlen(doc_ids[i]), offset) != 0) {
            errors++;
            continue;
        }
        
        result->successful_inserts++;
        
        /* Update stats */
        atomic_fetch_add(&coll->doc_count, 1);
        atomic_fetch_add(&coll->total_size, doc_sizes[i]);
    }
    
    pthread_rwlock_unlock(&coll->lock);
    
    /* Phase 3: Cleanup */
    for (size_t i = 0; i < count; i++) {
        if (json_strings[i]) buffer_pool_free_safe(json_strings[i]);
        if (doc_ids[i]) free(doc_ids[i]);
    }
    
    free(json_strings);
    free(doc_ids);
    free(doc_sizes);
    
    result->failed_inserts += errors;
    return 0;
}

/* Main batch insert implementation */
batch_insert_result_t* db_batch_insert_documents(
    database_t* db,
    const char* collection_name,
    json_value_t* documents,
    const batch_insert_options_t* options
) {
    if (!db || !collection_name || !documents || documents->type != JSON_ARRAY) {
        return NULL;
    }
    
    /* Use default options if not provided */
    batch_insert_options_t opts = options ? *options : BATCH_INSERT_OPTIONS_DEFAULT;
    
    /* Allocate result structure */
    batch_insert_result_t* result = calloc(1, sizeof(batch_insert_result_t));
    if (!result) return NULL;
    
    result->errors = json_create_array();
    result->total_documents = json_array_size(documents);
    
    double start_time = get_time_ms();
    
    /* Get collection - cast from internal type */
    hp_collection_t* coll = find_collection_internal(collection_name);
    if (!coll) {
        /* Try to create collection */
        if (db_create_collection(db, collection_name) != 0) {
            json_free(result->errors);
            free(result);
            return NULL;
        }
        coll = find_collection_internal(collection_name);
        if (!coll) {
            json_free(result->errors);
            free(result);
            return NULL;
        }
    }
    
    /* Process documents in batches */
    size_t batch_size = opts.batch_size;
    size_t num_docs = json_array_size(documents);
    
    LOG_INFO("Batch insert: %zu documents in batches of %zu", num_docs, batch_size);
    
    for (size_t i = 0; i < num_docs; i += batch_size) {
        size_t batch_count = (i + batch_size > num_docs) ? (num_docs - i) : batch_size;
        
        /* Collect batch of documents */
        json_value_t** batch_docs = malloc(batch_count * sizeof(json_value_t*));
        if (!batch_docs) {
            result->failed_inserts += (num_docs - i);
            break;
        }
        
        for (size_t j = 0; j < batch_count; j++) {
            batch_docs[j] = json_array_get(documents, i + j);
        }
        
        /* Process this batch */
        if (process_document_batch(coll, batch_docs, batch_count, result, &opts) != 0) {
            result->failed_inserts += batch_count;
        }
        
        free(batch_docs);
        
        /* Progress logging */
        if ((i + batch_count) % 10000 == 0) {
            LOG_DEBUG("Batch insert progress: %zu/%zu documents", i + batch_count, num_docs);
        }
    }
    
    /* Update indexes if deferred */
    if (opts.defer_indexing && coll->num_indexes > 0) {
        LOG_INFO("Updating deferred indexes...");
        /* TODO: Implement deferred index updates */
    }
    
    result->total_time_ms = get_time_ms() - start_time;
    
    LOG_INFO("Batch insert complete: %zu successful, %zu failed in %.2f ms (%.0f docs/sec)",
             result->successful_inserts, result->failed_inserts, result->total_time_ms,
             result->successful_inserts * 1000.0 / result->total_time_ms);
    
    return result;
}

/* Free batch result */
void batch_insert_result_free(batch_insert_result_t* result) {
    if (result) {
        if (result->errors) {
            json_free(result->errors);
        }
        free(result);
    }
}