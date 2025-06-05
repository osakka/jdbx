#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"

static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main() {
    logger_init("/tmp/quick_load_test.log", LOG_LEVEL_INFO);
    
    printf("Quick JSONdb Load Test\n");
    printf("=====================\n\n");
    
    database_t* db = db_init("/tmp/quick_test_db");
    assert(db != NULL);
    
    db_create_collection(db, "test");
    
    /* Test different document volumes */
    int test_sizes[] = {1000, 10000, 100000, 1000000};
    
    for (int t = 0; t < 4; t++) {
        int num_docs = test_sizes[t];
        printf("Testing with %d documents:\n", num_docs);
        
        double start = get_time_ms();
        int errors = 0;
        
        for (int i = 0; i < num_docs; i++) {
            json_value_t* doc = json_create_object();
            json_object_set(doc, "id", json_create_integer(i));
            json_object_set(doc, "value", json_create_integer(i * 10));
            
            char data[64];
            snprintf(data, sizeof(data), "Test document %d", i);
            json_object_set(doc, "data", json_create_string(data));
            
            json_value_t* result = db_insert_document(db, "test", doc);
            if (!result) errors++;
            else json_free(result);
            
            json_free(doc);
            
            if ((i + 1) % (num_docs / 10) == 0) {
                printf("  %d%% complete...\r", (i + 1) * 100 / num_docs);
                fflush(stdout);
            }
        }
        
        double end = get_time_ms();
        double elapsed = end - start;
        double throughput = num_docs * 1000.0 / elapsed;
        
        printf("\n  Time: %.2f ms\n", elapsed);
        printf("  Throughput: %.0f docs/sec\n", throughput);
        printf("  Errors: %d\n\n", errors);
        
        /* Test read performance */
        printf("  Testing random reads...\n");
        double read_total = 0;
        for (int i = 0; i < 100; i++) {
            char id[64];
            snprintf(id, sizeof(id), "doc-%d", rand() % num_docs);
            
            double r_start = get_time_ms();
            json_value_t* found = db_get_document(db, "test", id);
            double r_end = get_time_ms();
            
            if (found) {
                read_total += (r_end - r_start);
                json_free(found);
            }
        }
        printf("  Average read latency: %.3f ms\n\n", read_total / 100);
    }
    
    /* Check final database stats */
    json_value_t* collections = db_list_collections_with_info(db);
    if (collections) {
        printf("Final database state:\n");
        for (size_t i = 0; i < json_array_size(collections); i++) {
            json_value_t* coll = json_array_get(collections, i);
            json_value_t* name = json_object_get(coll, "name");
            json_value_t* count = json_object_get(coll, "document_count");
            json_value_t* size = json_object_get(coll, "total_size");
            
            if (name && count && size) {
                printf("  Collection: %s\n", name->value.string);
                printf("  Documents: %.0f\n", count->value.number);
                printf("  Total size: %.0f bytes\n", size->value.number);
            }
        }
        json_free(collections);
    }
    
    db_close(db);
    logger_close();
    return 0;
}