#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "database/database.h"
#include "utils/logger.h"
#include "utils/json.h"
#include "utils/buffer_pool.h"

int main() {
    logger_init("/tmp/test_simple_db.log", LOG_LEVEL_TRACE);
    
    printf("Simple database test...\n");
    
    // Initialize database
    database_t* db = db_init("/tmp/test_simple_db");
    assert(db != NULL);
    
    // Create collection
    int rc = db_create_collection(db, "test");
    assert(rc == 0);
    
    // Insert 5 documents
    printf("\nInserting 5 documents...\n");
    for (int i = 0; i < 5; i++) {
        json_value_t* doc = json_create_object();
        json_object_set(doc, "id", json_create_integer(i));
        json_object_set(doc, "name", json_create_string("test"));
        
        json_value_t* result = db_insert_document(db, "test", doc);
        if (result) {
            char* str = json_stringify(result);
            printf("Inserted: %s\n", str);
            buffer_pool_free_safe(str);
            json_free(result);
        } else {
            printf("Failed to insert document %d\n", i);
        }
        
        json_free(doc);
    }
    
    // Query all
    printf("\nQuerying all documents...\n");
    json_value_t* all = db_query_documents(db, "test", NULL);
    if (all) {
        char* str = json_stringify(all);
        printf("Query result: %s\n", str);
        
        // Count documents
        json_value_t* docs = json_object_get(all, "documents");
        if (docs) {
            printf("Number of documents returned: %zu\n", json_array_size(docs));
        }
        
        buffer_pool_free_safe(str);
        json_free(all);
    }
    
    // Get collection info
    printf("\nGetting collection info...\n");
    json_value_t* info = db_list_collections_with_info(db);
    if (info) {
        char* str = json_stringify(info);
        printf("Collections: %s\n", str);
        buffer_pool_free_safe(str);
        json_free(info);
    }
    
    db_close(db);
    logger_close();
    return 0;
}