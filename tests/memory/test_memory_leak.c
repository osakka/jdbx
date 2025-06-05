#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"

/* Get current memory usage in KB */
static long get_memory_usage() {
    FILE* file = fopen("/proc/self/status", "r");
    if (!file) return -1;
    
    char line[256];
    long vmrss = -1;
    
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "VmRSS: %ld kB", &vmrss) == 1) {
            break;
        }
    }
    
    fclose(file);
    return vmrss;
}

int main() {
    logger_init("/tmp/test_memory_leak.log", LOG_LEVEL_INFO);
    
    printf("Memory Leak Test\n");
    printf("================\n\n");
    
    database_t* db = db_init("/tmp/memory_test_db");
    assert(db != NULL);
    
    db_create_collection(db, "leak_test");
    
    long initial_memory = get_memory_usage();
    printf("Initial memory: %ld KB\n", initial_memory);
    
    /* Test 1: Repeated insert/delete cycles */
    printf("\nTest 1: Insert/Delete cycles (10000 iterations)\n");
    
    for (int cycle = 0; cycle < 10000; cycle++) {
        /* Insert a document */
        json_value_t* doc = json_create_object();
        json_object_set(doc, "cycle", json_create_integer(cycle));
        json_object_set(doc, "data", json_create_string("Test data for memory leak detection"));
        
        json_value_t* result = db_insert_document(db, "leak_test", doc);
        char* doc_id = NULL;
        
        if (result) {
            json_value_t* id_val = json_object_get(result, "_id");
            if (id_val && id_val->type == JSON_STRING) {
                doc_id = strdup(id_val->value.string);
            }
            json_free(result);
        }
        
        json_free(doc);
        
        /* Delete the document */
        if (doc_id) {
            db_delete_document(db, "leak_test", doc_id);
            free(doc_id);
        }
        
        if ((cycle + 1) % 1000 == 0) {
            long current_memory = get_memory_usage();
            printf("  After %d cycles: %ld KB (delta: %+ld KB)\n", 
                   cycle + 1, current_memory, current_memory - initial_memory);
        }
    }
    
    /* Test 2: Query operations */
    printf("\nTest 2: Query operations (1000 queries)\n");
    
    /* Insert some test data */
    for (int i = 0; i < 100; i++) {
        json_value_t* doc = json_create_object();
        json_object_set(doc, "id", json_create_integer(i));
        json_object_set(doc, "value", json_create_integer(i * 10));
        
        json_value_t* result = db_insert_document(db, "leak_test", doc);
        if (result) json_free(result);
        json_free(doc);
    }
    
    long before_queries = get_memory_usage();
    
    /* Run many queries */
    for (int i = 0; i < 1000; i++) {
        json_value_t* query = json_create_object();
        json_value_t* value_range = json_create_object();
        json_object_set(value_range, "$gte", json_create_integer(0));
        json_object_set(value_range, "$lt", json_create_integer(50));
        json_object_set(query, "value", value_range);
        
        json_value_t* results = db_query_documents(db, "leak_test", query);
        if (results) json_free(results);
        json_free(query);
        
        if ((i + 1) % 100 == 0) {
            long current_memory = get_memory_usage();
            printf("  After %d queries: %ld KB (delta: %+ld KB)\n", 
                   i + 1, current_memory, current_memory - before_queries);
        }
    }
    
    /* Final memory check */
    long final_memory = get_memory_usage();
    printf("\nFinal memory: %ld KB\n", final_memory);
    printf("Total memory growth: %+ld KB\n", final_memory - initial_memory);
    
    if (final_memory - initial_memory > 1000) {
        printf("⚠️  WARNING: Significant memory growth detected (>1MB)\n");
    } else {
        printf("✓ Memory usage appears stable\n");
    }
    
    db_close(db);
    logger_close();
    
    return 0;
}