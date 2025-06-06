#include "jsondb/utils/json.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

/**
 * Minimal JSON Operations Test
 * Tests JSON operations similar to what db_insert_document does
 */

/* Signal handler for timeout */
void handle_timeout(int sig) {
    (void)sig;
    printf("\nTIMEOUT: Operation took too long to complete\n");
    exit(1);
}

/* Simple database-like structure with a mutex */
typedef struct {
    json_value_t* collections;
    pthread_mutex_t lock;
} test_db_t;

/* Initialize test database */
test_db_t* test_db_init() {
    test_db_t* db = (test_db_t*)malloc(sizeof(test_db_t));
    if (!db) {
        return NULL;
    }

    /* Initialize collections */
    db->collections = json_create_object();
    if (!db->collections) {
        free(db);
        return NULL;
    }

    /* Initialize mutex */
    pthread_mutex_init(&db->lock, NULL);

    return db;
}

/* Close test database */
void test_db_close(test_db_t* db) {
    if (!db) {
        return;
    }

    /* Free resources */
    json_free(db->collections);
    pthread_mutex_destroy(&db->lock);
    free(db);
}

/* Create test collection */
int test_create_collection(test_db_t* db, const char* name) {
    if (!db || !name) {
        return 0;
    }

    pthread_mutex_lock(&db->lock);

    /* Check if collection already exists */
    if (json_object_has(db->collections, name)) {
        pthread_mutex_unlock(&db->lock);
        return 0;
    }

    /* Create new collection (array of documents) */
    json_value_t* collection = json_create_array();
    if (!collection) {
        pthread_mutex_unlock(&db->lock);
        return 0;
    }

    /* Add collection to database */
    json_object_set(db->collections, name, collection);

    pthread_mutex_unlock(&db->lock);

    return 1;
}

/* Simple UUID generation function */
char* test_generate_uuid() {
    static int counter = 0;
    char* uuid = (char*)malloc(37);
    sprintf(uuid, "test-uuid-%d", counter++);
    return uuid;
}

/* Test insert document function */
json_value_t* test_insert_document(test_db_t* db, const char* collection_name, json_value_t* document) {
    printf("  - Starting insert document test function\n");

    if (!db || !collection_name || !document || document->type != JSON_OBJECT) {
        printf("  - Invalid parameters\n");
        return NULL;
    }

    printf("  - Making document copy\n");
    /* Make a copy of document */
    json_value_t* doc_copy = json_clone(document);
    if (!doc_copy) {
        printf("  - Failed to clone document\n");
        return NULL;
    }

    printf("  - Adding ID to document\n");
    /* Generate ID if not provided */
    if (!json_object_has(doc_copy, "uuid")) {
        char* id = test_generate_uuid();
        if (id) {
            json_object_set(doc_copy, "uuid", json_create_string(id));
            free(id);
        } else {
            printf("  - Failed to generate UUID\n");
            json_free(doc_copy);
            return NULL;
        }
    }

    printf("  - Checking document has ID\n");
    /* Ensure the document has an ID */
    json_value_t* id = json_object_get(doc_copy, "uuid");
    if (!id || id->type != JSON_STRING) {
        printf("  - Document has no valid ID\n");
        json_free(doc_copy);
        return NULL;
    }

    printf("  - Creating result object\n");
    /* Create result object with ID */
    json_value_t* result = json_create_object();
    if (!result) {
        printf("  - Failed to create result object\n");
        json_free(doc_copy);
        return NULL;
    }
    json_object_set(result, "uuid", json_create_string(id->value.string));

    printf("  - Locking database\n");
    /* Lock database */
    pthread_mutex_lock(&db->lock);

    printf("  - Getting collection\n");
    /* Get collection */
    json_value_t* collection = json_object_get(db->collections, collection_name);
    if (!collection || collection->type != JSON_ARRAY) {
        printf("  - Collection not found or not an array\n");
        pthread_mutex_unlock(&db->lock);
        json_free(result);
        json_free(doc_copy);
        return NULL;
    }

    printf("  - Adding document to collection\n");
    /* Add document to collection */
    json_array_append(collection, doc_copy);

    printf("  - Unlocking database\n");
    /* Unlock database */
    pthread_mutex_unlock(&db->lock);

    printf("  - Returning result\n");
    return result;
}

/* Main function */
int main() {
    /* Initialize logger */
    logger_init("json_clone_test.log", LOG_LEVEL_DEBUG);
    printf("Logger initialized\n");

    /* Step 1: Test basic JSON cloning */
    printf("\nStep 1: Testing basic JSON cloning\n");

    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Test Document"));
    json_object_set(doc, "value", json_create_integer(42));

    json_value_t* doc_copy = json_clone(doc);
    if (doc_copy) {
        printf("SUCCESS: Document cloned successfully\n");
        json_free(doc_copy);
    } else {
        printf("FAILED: Could not clone document\n");
        json_free(doc);
        logger_close();
        return 1;
    }

    /* Step 2: Test database initialization */
    printf("\nStep 2: Testing database initialization\n");

    test_db_t* db = test_db_init();
    if (!db) {
        printf("FAILED: Could not initialize test database\n");
        json_free(doc);
        logger_close();
        return 1;
    }
    printf("SUCCESS: Test database initialized\n");

    /* Step 3: Test collection creation */
    printf("\nStep 3: Testing collection creation\n");

    int result = test_create_collection(db, "test_collection");
    if (!result) {
        printf("FAILED: Could not create test collection\n");
        test_db_close(db);
        json_free(doc);
        logger_close();
        return 1;
    }
    printf("SUCCESS: Test collection created\n");

    /* Step 4: Test document insertion */
    printf("\nStep 4: Testing document insertion\n");

    signal(SIGALRM, handle_timeout);
    alarm(10);  /* 10-second timeout */

    json_value_t* insert_result = test_insert_document(db, "test_collection", doc);

    alarm(0);  /* Cancel timeout */

    if (insert_result) {
        json_value_t* id_val = json_object_get(insert_result, "uuid");
        printf("SUCCESS: Document inserted with ID: %s\n", id_val->value.string);
        json_free(insert_result);
    } else {
        printf("FAILED: Could not insert document\n");
        test_db_close(db);
        json_free(doc);
        logger_close();
        return 1;
    }

    /* Clean up */
    json_free(doc);
    test_db_close(db);
    logger_close();

    printf("\nAll tests completed successfully\n");
    return 0;
}