#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "database/database.h"
#include "utils/logger.h"
#include "utils/json.h"

double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main() {
    logger_init("/tmp/test_integration.log", LOG_LEVEL_INFO);
    
    printf("Testing database integration...\n");
    
    // Initialize database
    database_t* db = db_init("/tmp/test_integration_db");
    assert(db != NULL);
    
    // Create collection
    int rc = db_create_collection(db, "users");
    assert(rc == 0);
    
    // Test 1: Insert 100 documents
    printf("\n1. Inserting 100 documents...\n");
    double start = get_time_ms();
    
    for (int i = 0; i < 100; i++) {
        json_value_t* doc = json_create_object();
        
        char name[64];
        snprintf(name, sizeof(name), "User %d", i);
        json_object_set(doc, "name", json_create_string(name));
        json_object_set(doc, "age", json_create_integer(20 + (i % 60)));
        json_object_set(doc, "active", json_create_boolean(i % 2 == 0));
        
        json_value_t* result = db_insert_document(db, "users", doc);
        if (!result) {
            printf("ERROR: Failed to insert document %d\n", i);
            json_free(doc);
            break;
        }
        
        // Save the ID for later
        if (i == 50) {
            json_value_t* id_val = json_object_get(result, "_id");
            if (id_val) {
                const char* id_str = json_get_string(id_val);
                if (id_str) {
                    printf("Document 50 ID: %s\n", id_str);
                }
            }
        }
        
        json_free(result);
        json_free(doc);
    }
    
    double insert_time = get_time_ms() - start;
    printf("Insert time: %.2f ms (%.0f docs/sec)\n", 
           insert_time, (100 * 1000.0) / insert_time);
    
    // Test 2: Query all documents
    printf("\n2. Querying all documents...\n");
    start = get_time_ms();
    
    json_value_t* all_results = db_query_documents(db, "users", NULL);
    if (all_results) {
        json_value_t* docs = json_object_get(all_results, "documents");
        int count = docs ? json_array_size(docs) : 0;
        printf("Found %d documents\n", count);
        
        // Print first few documents for debugging
        if (count > 0) {
            for (int i = 0; i < count && i < 3; i++) {
                json_value_t* doc = json_array_get(docs, i);
                char* doc_str = json_stringify(doc, 0);
                printf("  Doc %d: %s\n", i, doc_str);
                free(doc_str);
            }
        }
        
        json_free(all_results);
    } else {
        printf("ERROR: Query failed\n");
    }
    
    double query_time = get_time_ms() - start;
    printf("Query time: %.2f ms\n", query_time);
    
    // Test 3: Query with filter
    printf("\n3. Querying with filter (age=25)...\n");
    start = get_time_ms();
    
    json_value_t* query = json_create_object();
    json_object_set(query, "age", json_create_integer(25));
    
    json_value_t* filtered_results = db_query_documents(db, "users", query);
    if (filtered_results) {
        json_value_t* docs = json_object_get(filtered_results, "documents");
        int count = docs ? json_array_size(docs) : 0;
        printf("Found %d documents with age=25\n", count);
        json_free(filtered_results);
    }
    
    json_free(query);
    
    double filter_time = get_time_ms() - start;
    printf("Filter query time: %.2f ms\n", filter_time);
    
    // Test 4: Create index
    printf("\n4. Creating index on 'age' field...\n");
    start = get_time_ms();
    
    index_t* age_index = db_create_index(db, "users", "age_idx", "age", INDEX_TYPE_NON_UNIQUE);
    if (age_index) {
        printf("Index created successfully\n");
    } else {
        printf("ERROR: Failed to create index\n");
    }
    
    double index_time = get_time_ms() - start;
    printf("Index creation time: %.2f ms\n", index_time);
    
    // Test 5: Query with index
    printf("\n5. Querying with index (age=25)...\n");
    start = get_time_ms();
    
    query = json_create_object();
    json_object_set(query, "age", json_create_integer(25));
    
    json_value_t* indexed_results = db_query_documents(db, "users", query);
    if (indexed_results) {
        json_value_t* docs = json_object_get(indexed_results, "documents");
        int count = docs ? json_array_size(docs) : 0;
        printf("Found %d documents with age=25 (using index)\n", count);
        json_free(indexed_results);
    }
    
    json_free(query);
    
    double indexed_query_time = get_time_ms() - start;
    printf("Indexed query time: %.2f ms (%.1fx speedup)\n", 
           indexed_query_time, filter_time / indexed_query_time);
    
    // Cleanup
    db_close(db);
    
    printf("\nIntegration test completed!\n");
    logger_close();
    return 0;
}