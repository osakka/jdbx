#include "database/batch_operations.h"
#include "database/document_storage.h"
#include "database/database.h"
#include "utils/json_helpers.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

/* Suppress unused parameter warnings */
#define UNUSED(x) ((void)(x))

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
    uuid_bytes[8] = (uuid_bytes[8] & 0x3F) | 0x80; /* Variant 10 */
    
    /* Format as standard UUID string */
    snprintf(id_buf, buf_size, 
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid_bytes[0], uuid_bytes[1], uuid_bytes[2], uuid_bytes[3],
             uuid_bytes[4], uuid_bytes[5], uuid_bytes[6], uuid_bytes[7],
             uuid_bytes[8], uuid_bytes[9], uuid_bytes[10], uuid_bytes[11],
             uuid_bytes[12], uuid_bytes[13], uuid_bytes[14], uuid_bytes[15]);
}

/* JDBX-based batch insert implementation 
 * TODO: This is a placeholder implementation that processes documents one by one
 * Future optimization will use JDBX's native batch capabilities
 */
static int batch_insert_jdbx(
    database_t* db,
    const char* collection_name,
    json_value_t* documents,
    batch_insert_result_t* result,
    const batch_insert_options_t* options
) {
    (void)collection_name; /* Currently using unified documents storage */
    (void)options; /* TODO: Implement batch options */
    size_t count = json_array_size(documents);
    size_t errors = 0;
    
    /* Phase 1: Prepare documents */
    char** doc_ids =BUFFER_CALLOC(count, sizeof(char*));
    char** json_strings =BUFFER_CALLOC(count, sizeof(char*));
    size_t* doc_sizes =BUFFER_CALLOC(count, sizeof(size_t));
    
    if (!doc_ids || !json_strings || !doc_sizes) {
        BUFFER_FREE(doc_ids);
        BUFFER_FREE(json_strings);
        BUFFER_FREE(doc_sizes);
        return -1;
    }
    
    for (size_t i = 0; i < count; i++) {
        json_value_t* doc = json_array_get(documents, i);
        if (!doc || doc->type != JSON_OBJECT) {
            errors++;
            continue;
        }
        
        /* Generate or extract document ID */
        doc_ids[i] =BUFFER_ALLOC(256);
        json_value_t* id_field = json_object_get(doc, "id");
        
        if (!id_field || id_field->type != JSON_STRING) {
            /* Generate UUID for ID */
            generate_batch_doc_id(doc_ids[i], 256, i);
            json_object_set(doc, "id", json_create_string(doc_ids[i]));
            json_object_set(doc, "uuid", json_create_string(doc_ids[i]));
        } else {
            strncpy(doc_ids[i], id_field->value.string, 255);
            doc_ids[i][255] = '\0';
            if (!json_object_get(doc, "uuid")) {
                json_object_set(doc, "uuid", json_create_string(doc_ids[i]));
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
    
    /* Phase 2: Insert documents into JDBX
     * For now, use regular insert API in a loop
     * TODO: Implement native JDBX batch insert
     */
    for (size_t i = 0; i < count; i++) {
        if (!json_strings[i]) continue;
        
        /* Parse JSON and insert */
        json_value_t* parsed_doc = json_parse(json_strings[i]);
        if (!parsed_doc) {
            errors++;
            result->failed_inserts++;
            continue;
        }
        
        /* Determine document type from collection_name */
        const char* doc_type = collection_name;
        if (strcmp(collection_name, "users") == 0) doc_type = "user";
        else if (strcmp(collection_name, "roles") == 0) doc_type = "role";
        else if (strcmp(collection_name, "sessions") == 0) doc_type = "session";
        
        json_value_t* insert_result = virtual_insert(db, doc_type, "default", collection_name, parsed_doc, "batch-system");
        int success = (insert_result != NULL);
        
        if (success) {
            result->successful_inserts++;
            json_free(insert_result); /* Free the returned result */
        } else {
            errors++;
            result->failed_inserts++;
            
            /* Add error details if requested */
            if (result->errors) {
                json_value_t* error = json_create_object();
                json_object_set(error, "id", json_create_string(doc_ids[i]));
                json_object_set(error, "error", json_create_string("insert_failed"));
                json_array_append(result->errors, error);
            }
        }
        
        json_free(parsed_doc);
    }
    
    /* Phase 3: Cleanup */
    for (size_t i = 0; i < count; i++) {
        if (json_strings[i]) BUFFER_FREE(json_strings[i]);
        if (doc_ids[i]) BUFFER_FREE(doc_ids[i]);
    }
    
    BUFFER_FREE(json_strings);
    BUFFER_FREE(doc_ids);
    BUFFER_FREE(doc_sizes);
    
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
    batch_insert_result_t* result =BUFFER_CALLOC(1, sizeof(batch_insert_result_t));
    if (!result) return NULL;
    
    result->errors = json_create_array();
    result->total_documents = json_array_size(documents);
    
    /* Measure execution time */
    double start_time = get_time_ms();
    
    /* Execute batch insert using JDBX */
    int ret = batch_insert_jdbx(db, collection_name, documents, result, &opts);
    
    result->total_time_ms = get_time_ms() - start_time;
    
    if (ret != 0) {
        json_free(result->errors);
        BUFFER_FREE(result);
        return NULL;
    }
    
    /* No documents_per_second field in the structure, just return */
    
    return result;
}

/* Free batch insert result */
void batch_insert_result_free(batch_insert_result_t* result) {
    if (result) {
        if (result->errors) {
            json_free(result->errors);
        }
        BUFFER_FREE(result);
    }
}