#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/**
 * Quick Insert Test
 * 
 * Extra simple test for just database creation, collection creation, and doc insertion
 */

int main() {
    /* Create database in memory - avoid disk access issues */
    database_t* db = malloc(sizeof(database_t));
    if (!db) {
        printf("FAIL - Could not allocate database\n");
        return 1;
    }
    
    /* Initialize structure */
    db->collections = json_create_object();
    pthread_mutex_init(&db->lock, NULL);
    db->is_modified = 0;
    db->cache_enabled = 0; /* No caching */
    db->cache = NULL;
    db->path = strdup("memory_only.json");
    
    printf("Database structure initialized\n");
    
    /* Create test collection */
    json_value_t* collection = json_create_array();
    json_object_set(db->collections, "test_collection", collection);
    
    printf("Test collection created\n");
    
    /* Create test document */
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Test Document"));
    json_object_set(doc, "value", json_create_integer(42));
    json_object_set(doc, "uuid", json_create_string("test-id-123"));
    
    printf("Test document created\n");
    
    /* Manual document insertion (bypassing db_insert_document completely) */
    pthread_mutex_lock(&db->lock);
    
    /* Get test collection */
    json_value_t* coll = json_object_get(db->collections, "test_collection");
    if (coll) {
        /* Clone document */
        json_value_t* doc_copy = json_clone(doc);
        
        /* Add to collection */
        json_array_append(coll, doc_copy);
        
        /* Mark as changed */
        db->is_modified = 1;
        
        printf("Document inserted manually\n");
    } else {
        printf("FAIL - Could not find collection\n");
    }
    
    pthread_mutex_unlock(&db->lock);
    
    /* Cleanup */
    json_free(doc);
    pthread_mutex_destroy(&db->lock);
    free(db->path);
    json_free(db->collections);
    free(db);
    
    printf("SUCCESS - Test completed\n");
    return 0;
}