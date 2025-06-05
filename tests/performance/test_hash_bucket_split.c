#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include "database/database.h"
#include "index/hash_index.h"
#include "utils/logger.h"
#include "utils/json.h"
#include "utils/buffer_pool.h"

#define NUM_DOCUMENTS 10000
#define NUM_THREADS 4
#define DOCS_PER_THREAD (NUM_DOCUMENTS / NUM_THREADS)

typedef struct {
    database_t* db;
    int thread_id;
    int start_id;
    int end_id;
    int success_count;
    int error_count;
} thread_data_t;

void* insert_documents_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    LOG_INFO("Thread %d: Inserting documents %d to %d", 
             data->thread_id, data->start_id, data->end_id);
    
    for (int i = data->start_id; i < data->end_id; i++) {
        // Create unique document ID
        char doc_id[256];
        snprintf(doc_id, sizeof(doc_id), "doc-bucket-test-%08d", i);
        
        // Create document with various fields
        json_value_t* doc = json_create_object();
        json_object_set(doc, "_id", json_create_string(doc_id));
        json_object_set(doc, "thread_id", json_create_integer(data->thread_id));
        json_object_set(doc, "sequence", json_create_integer(i));
        json_object_set(doc, "timestamp", json_create_integer(time(NULL)));
        
        // Add some random data to make documents more realistic
        char data_field[512];
        snprintf(data_field, sizeof(data_field), 
                "This is test data for document %d created by thread %d. "
                "Hash value should distribute across buckets. Random: %d",
                i, data->thread_id, rand());
        json_object_set(doc, "data", json_create_string(data_field));
        
        // Insert document
        json_value_t* result = db_insert_document(data->db, "bucket_test", doc);
        if (result) {
            data->success_count++;
            json_free(result);
        } else {
            data->error_count++;
            LOG_ERROR("Thread %d: Failed to insert document %d", data->thread_id, i);
        }
        
        json_free(doc);
        
        // Print progress every 1000 documents
        if ((i - data->start_id) % 1000 == 0 && (i - data->start_id) > 0) {
            LOG_INFO("Thread %d: Progress %d/%d documents", 
                     data->thread_id, i - data->start_id, data->end_id - data->start_id);
        }
    }
    
    LOG_INFO("Thread %d: Completed. Success: %d, Errors: %d",
             data->thread_id, data->success_count, data->error_count);
    
    return NULL;
}

void print_hash_index_stats(const char* message) {
    // This is a placeholder - in real implementation we'd access the hash index stats
    LOG_INFO("=== %s ===", message);
    // TODO: Add actual stats retrieval once we have access to collection internals
}

int main() {
    srand(time(NULL));
    
    logger_init("/tmp/test_hash_bucket_split.log", LOG_LEVEL_DEBUG);
    LOG_INFO("Starting hash index bucket split test with %d documents", NUM_DOCUMENTS);
    
    // Initialize database
    database_t* db = db_init("/tmp/test_hash_bucket_split");
    assert(db != NULL);
    
    // Create collection
    int rc = db_create_collection(db, "bucket_test");
    assert(rc == 0);
    
    // Print initial stats
    print_hash_index_stats("Initial State");
    
    // Single-threaded warm-up: Insert a few documents to ensure everything is initialized
    LOG_INFO("Warm-up phase: Inserting 10 documents");
    for (int i = 0; i < 10; i++) {
        char doc_id[256];
        snprintf(doc_id, sizeof(doc_id), "warmup-%03d", i);
        
        json_value_t* doc = json_create_object();
        json_object_set(doc, "_id", json_create_string(doc_id));
        json_object_set(doc, "type", json_create_string("warmup"));
        
        json_value_t* result = db_insert_document(db, "bucket_test", doc);
        if (result) {
            json_free(result);
        }
        json_free(doc);
    }
    
    // Multi-threaded insertion test
    LOG_INFO("Starting multi-threaded insertion with %d threads", NUM_THREADS);
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].db = db;
        thread_data[i].thread_id = i;
        thread_data[i].start_id = i * DOCS_PER_THREAD;
        thread_data[i].end_id = (i + 1) * DOCS_PER_THREAD;
        thread_data[i].success_count = 0;
        thread_data[i].error_count = 0;
        
        if (pthread_create(&threads[i], NULL, insert_documents_thread, &thread_data[i]) != 0) {
            LOG_ERROR("Failed to create thread %d", i);
            exit(1);
        }
    }
    
    // Wait for all threads to complete
    int total_success = 0;
    int total_errors = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        total_success += thread_data[i].success_count;
        total_errors += thread_data[i].error_count;
    }
    
    LOG_INFO("All threads completed. Total success: %d, Total errors: %d", 
             total_success, total_errors);
    
    // Print stats after bulk insertion
    print_hash_index_stats("After Bulk Insertion");
    
    // Verification phase: Query some documents to ensure they can be retrieved
    LOG_INFO("Verification phase: Querying random documents");
    
    int verify_count = 100;
    int verify_success = 0;
    
    for (int i = 0; i < verify_count; i++) {
        int doc_num = rand() % NUM_DOCUMENTS;
        char doc_id[256];
        snprintf(doc_id, sizeof(doc_id), "doc-bucket-test-%08d", doc_num);
        
        json_value_t* doc = db_get_document(db, "bucket_test", doc_id);
        if (doc) {
            verify_success++;
            json_free(doc);
        } else {
            LOG_WARNING("Failed to retrieve document: %s", doc_id);
        }
    }
    
    LOG_INFO("Verification complete: %d/%d documents retrieved successfully", 
             verify_success, verify_count);
    
    // Query all documents to test scanning
    LOG_INFO("Querying all documents to verify total count");
    json_value_t* all_docs = db_query_documents(db, "bucket_test", NULL);
    if (all_docs) {
        json_value_t* docs_array = json_object_get(all_docs, "documents");
        if (docs_array) {
            size_t doc_count = json_array_size(docs_array);
            LOG_INFO("Total documents in collection: %zu (expected: ~%d)", 
                     doc_count, total_success + 10); // +10 for warmup
        }
        json_free(all_docs);
    }
    
    // Get collection info
    json_value_t* info = db_list_collections_with_info(db);
    if (info) {
        char* str = json_stringify(info);
        LOG_INFO("Collection info: %s", str);
        buffer_pool_free_safe(str);
        json_free(info);
    }
    
    // Final stats
    print_hash_index_stats("Final State");
    
    // Cleanup
    db_close(db);
    logger_close();
    
    printf("\nTest completed. Check /tmp/test_hash_bucket_split.log for details.\n");
    return (total_errors == 0) ? 0 : 1;
}