/* Test B+tree index creation and population */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"

int main() {
    /* Initialize logger */
    logger_init(NULL, LOG_LEVEL_INFO);
    
    /* Initialize database */
    database_t* db = db_init("/tmp/test_btree_db");
    if (!db) {
        printf("Failed to initialize database\n");
        return 1;
    }
    
    /* Create collection */
    const char* collection = "test_users";
    if (db_create_collection(db, collection) != 0) {
        printf("Failed to create collection\n");
        return 1;
    }
    
    /* Insert test documents */
    printf("Inserting 100 test documents...\n");
    for (int i = 1; i <= 100; i++) {
        json_value_t* doc = json_create_object();
        char name[32];
        snprintf(name, sizeof(name), "User %d", i);
        json_object_set(doc, "name", json_create_string(name));
        json_object_set(doc, "age", json_create_integer(20 + (i % 60)));
        json_object_set(doc, "score", json_create_integer(50 + (i % 50)));
        
        json_value_t* result = db_insert_document(db, collection, doc);
        if (result) {
            json_free(result);
        }
        json_free(doc);
    }
    printf("Documents inserted.\n\n");
    
    /* Create index on age field */
    printf("Creating B+tree index on 'age' field...\n");
    index_t* age_index = db_create_index(db, collection, "age_idx", "age", INDEX_TYPE_NON_UNIQUE);
    if (age_index) {
        printf("Index created successfully!\n");
        if (age_index->name) free(age_index->name);
        if (age_index->field_path) free(age_index->field_path);
        free(age_index);
    } else {
        printf("Failed to create index!\n");
    }
    
    /* Test range query */
    printf("\nTesting range query (age between 30 and 35)...\n");
    json_value_t* query = json_create_object();
    json_value_t* age_cond = json_create_object();
    json_object_set(age_cond, "$gte", json_create_integer(30));
    json_object_set(age_cond, "$lte", json_create_integer(35));
    json_object_set(query, "age", age_cond);
    
    clock_t start = clock();
    json_value_t* results = db_query_documents(db, collection, query);
    clock_t end = clock();
    
    double query_time = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    
    if (results) {
        json_value_t* docs = json_object_get(results, "documents");
        if (docs && docs->type == JSON_ARRAY) {
            printf("Found %zu documents in %.2f ms\n", json_array_size(docs), query_time);
            
            /* Show first few results */
            printf("\nFirst 3 results:\n");
            for (size_t i = 0; i < 3 && i < json_array_size(docs); i++) {
                json_value_t* doc = json_array_get(docs, i);
                json_value_t* name = json_object_get(doc, "name");
                json_value_t* age = json_object_get(doc, "age");
                if (name && age) {
                    printf("  - %s, age: %ld\n", 
                           name->value.string, 
                           (long)age->value.integer);
                }
            }
        }
        json_free(results);
    }
    
    json_free(query);
    
    /* List indexes */
    printf("\nListing indexes on collection:\n");
    json_value_t* index_list = db_list_indexes(db, collection);
    if (index_list) {
        char* json_str = json_stringify(index_list);
        printf("%s\n", json_str);
        free(json_str);
        json_free(index_list);
    }
    
    /* Clean up */
    db_close(db);
    logger_close();
    
    return 0;
}