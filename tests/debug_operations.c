#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/**
 * Debug Operations Test
 * Focus on debugging each operation step by step
 */

/* Signal handler for timeout */
void handle_timeout(int sig) {
    printf("\nTIMEOUT: Operation took too long to complete\n");
    exit(1);
}

/* Test database init */
void test_db_init() {
    printf("Testing db_init...\n");
    
    database_t* db = db_init("debug_ops.json");
    if (!db) {
        printf("FAILED: Could not initialize database\n");
        exit(1);
    }
    
    printf("SUCCESS: Database initialized\n");
    
    db_close(db);
}

/* Test collection creation */
void test_create_collection() {
    printf("Testing db_create_collection...\n");
    
    database_t* db = db_init("debug_ops.json");
    if (!db) {
        printf("FAILED: Could not initialize database\n");
        exit(1);
    }
    
    int result = db_create_collection(db, "test_collection");
    if (!result) {
        printf("Collection might already exist, continuing\n");
    } else {
        printf("SUCCESS: Collection created\n");
    }
    
    db_close(db);
}

/* Test document insertion WITHOUT cache */
void test_insert_document_no_cache() {
    printf("Testing db_insert_document (without cache)...\n");
    
    database_t* db = db_init("debug_ops.json");
    if (!db) {
        printf("FAILED: Could not initialize database\n");
        exit(1);
    }
    
    /* Make sure collection exists */
    db_create_collection(db, "test_collection");
    
    /* Create test document */
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Test Document"));
    json_object_set(doc, "value", json_create_integer(42));
    
    /* Insert document with timeout */
    printf("Inserting document...\n");
    fflush(stdout);
    signal(SIGALRM, handle_timeout);
    alarm(5);  /* 5-second timeout */
    
    json_value_t* result = db_insert_document(db, "test_collection", doc);
    
    alarm(0);  /* Cancel timeout */
    
    if (result) {
        /* Get ID of inserted document */
        json_value_t* id = json_object_get(result, "_id");
        if (id && id->type == JSON_STRING) {
            printf("SUCCESS: Document inserted with ID: %s\n", id->value.string);
        } else {
            printf("SUCCESS: Document inserted but couldn't get ID\n");
        }
        json_free(result);
    } else {
        printf("FAILED: Could not insert document\n");
        exit(1);
    }
    
    json_free(doc);
    db_close(db);
}

/* Test document retrieval */
void test_get_document() {
    printf("Testing db_get_document...\n");
    
    database_t* db = db_init("debug_ops.json");
    if (!db) {
        printf("FAILED: Could not initialize database\n");
        exit(1);
    }
    
    /* First insert a document to retrieve */
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Document to Retrieve"));
    json_object_set(doc, "value", json_create_integer(100));
    
    json_value_t* insert_result = db_insert_document(db, "test_collection", doc);
    if (!insert_result) {
        printf("FAILED: Could not insert document for retrieval test\n");
        json_free(doc);
        db_close(db);
        exit(1);
    }
    
    /* Get ID of inserted document */
    json_value_t* id_val = json_object_get(insert_result, "_id");
    if (!id_val || id_val->type != JSON_STRING) {
        printf("FAILED: Could not get ID of inserted document\n");
        json_free(doc);
        json_free(insert_result);
        db_close(db);
        exit(1);
    }
    
    char* doc_id = strdup(id_val->value.string);
    json_free(insert_result);
    json_free(doc);
    
    /* Now retrieve the document */
    printf("Retrieving document with ID: %s...\n", doc_id);
    fflush(stdout);
    signal(SIGALRM, handle_timeout);
    alarm(5);  /* 5-second timeout */
    
    json_value_t* retrieved_doc = db_get_document(db, "test_collection", doc_id);
    
    alarm(0);  /* Cancel timeout */
    
    if (retrieved_doc) {
        printf("SUCCESS: Document retrieved\n");
        json_free(retrieved_doc);
    } else {
        printf("FAILED: Could not retrieve document\n");
        free(doc_id);
        db_close(db);
        exit(1);
    }
    
    free(doc_id);
    db_close(db);
}

/* Test document update */
void test_update_document() {
    printf("Testing db_update_document...\n");
    
    database_t* db = db_init("debug_ops.json");
    if (!db) {
        printf("FAILED: Could not initialize database\n");
        exit(1);
    }
    
    /* First insert a document to update */
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Document to Update"));
    json_object_set(doc, "value", json_create_integer(200));
    
    json_value_t* insert_result = db_insert_document(db, "test_collection", doc);
    if (!insert_result) {
        printf("FAILED: Could not insert document for update test\n");
        json_free(doc);
        db_close(db);
        exit(1);
    }
    
    /* Get ID of inserted document */
    json_value_t* id_val = json_object_get(insert_result, "_id");
    if (!id_val || id_val->type != JSON_STRING) {
        printf("FAILED: Could not get ID of inserted document\n");
        json_free(doc);
        json_free(insert_result);
        db_close(db);
        exit(1);
    }
    
    char* doc_id = strdup(id_val->value.string);
    json_free(insert_result);
    json_free(doc);
    
    /* Create updated document */
    json_value_t* updated_doc = json_create_object();
    json_object_set(updated_doc, "name", json_create_string("Updated Document"));
    json_object_set(updated_doc, "value", json_create_integer(300));
    json_object_set(updated_doc, "updated", json_create_boolean(1));
    
    /* Update the document */
    printf("Updating document with ID: %s...\n", doc_id);
    fflush(stdout);
    signal(SIGALRM, handle_timeout);
    alarm(5);  /* 5-second timeout */
    
    json_value_t* update_result = db_update_document(db, "test_collection", doc_id, updated_doc);
    
    alarm(0);  /* Cancel timeout */
    
    if (update_result) {
        printf("SUCCESS: Document updated\n");
        json_free(update_result);
    } else {
        printf("FAILED: Could not update document\n");
        json_free(updated_doc);
        free(doc_id);
        db_close(db);
        exit(1);
    }
    
    json_free(updated_doc);
    free(doc_id);
    db_close(db);
}

/* Test document deletion */
void test_delete_document() {
    printf("Testing db_delete_document...\n");
    
    database_t* db = db_init("debug_ops.json");
    if (!db) {
        printf("FAILED: Could not initialize database\n");
        exit(1);
    }
    
    /* First insert a document to delete */
    json_value_t* doc = json_create_object();
    json_object_set(doc, "name", json_create_string("Document to Delete"));
    json_object_set(doc, "value", json_create_integer(400));
    
    json_value_t* insert_result = db_insert_document(db, "test_collection", doc);
    if (!insert_result) {
        printf("FAILED: Could not insert document for delete test\n");
        json_free(doc);
        db_close(db);
        exit(1);
    }
    
    /* Get ID of inserted document */
    json_value_t* id_val = json_object_get(insert_result, "_id");
    if (!id_val || id_val->type != JSON_STRING) {
        printf("FAILED: Could not get ID of inserted document\n");
        json_free(doc);
        json_free(insert_result);
        db_close(db);
        exit(1);
    }
    
    char* doc_id = strdup(id_val->value.string);
    json_free(insert_result);
    json_free(doc);
    
    /* Delete the document */
    printf("Deleting document with ID: %s...\n", doc_id);
    fflush(stdout);
    signal(SIGALRM, handle_timeout);
    alarm(5);  /* 5-second timeout */
    
    int delete_result = db_delete_document(db, "test_collection", doc_id);
    
    alarm(0);  /* Cancel timeout */
    
    if (delete_result) {
        printf("SUCCESS: Document deleted\n");
    } else {
        printf("FAILED: Could not delete document\n");
        free(doc_id);
        db_close(db);
        exit(1);
    }
    
    free(doc_id);
    db_close(db);
}

/* Test document query */
void test_query_documents() {
    printf("Testing db_query_documents...\n");
    
    database_t* db = db_init("debug_ops.json");
    if (!db) {
        printf("FAILED: Could not initialize database\n");
        exit(1);
    }
    
    /* Query with empty query (get all documents) */
    printf("Querying all documents...\n");
    fflush(stdout);
    signal(SIGALRM, handle_timeout);
    alarm(5);  /* 5-second timeout */
    
    json_value_t* result = db_query_documents(db, "test_collection", NULL);
    
    alarm(0);  /* Cancel timeout */
    
    if (result) {
        /* Get documents array */
        json_value_t* docs = json_object_get(result, "documents");
        if (docs && docs->type == JSON_ARRAY) {
            printf("SUCCESS: Query returned %zu documents\n", docs->value.array.size);
        } else {
            printf("SUCCESS: Query completed but did not return documents array\n");
        }
        json_free(result);
    } else {
        printf("FAILED: Could not execute query\n");
        db_close(db);
        exit(1);
    }
    
    db_close(db);
}

/* Main function */
int main(int argc, char *argv[]) {
    /* Initialize logger */
    logger_init("debug_operations.log", LOG_LEVEL_DEBUG);
    
    /* Run tests based on argument */
    if (argc < 2) {
        printf("Usage: %s <test_number>\n", argv[0]);
        printf("  1: Test db_init\n");
        printf("  2: Test db_create_collection\n");
        printf("  3: Test db_insert_document (no cache)\n");
        printf("  4: Test db_get_document\n");
        printf("  5: Test db_update_document\n");
        printf("  6: Test db_delete_document\n");
        printf("  7: Test db_query_documents\n");
        printf("  8: Run all tests\n");
        return 1;
    }
    
    int test_num = atoi(argv[1]);
    
    printf("=== JSONdb Debug Operations Test ===\n\n");
    
    switch (test_num) {
        case 1:
            test_db_init();
            break;
        case 2:
            test_create_collection();
            break;
        case 3:
            test_insert_document_no_cache();
            break;
        case 4:
            test_get_document();
            break;
        case 5:
            test_update_document();
            break;
        case 6:
            test_delete_document();
            break;
        case 7:
            test_query_documents();
            break;
        case 8:
            printf("Running all tests...\n\n");
            test_db_init();
            printf("\n");
            test_create_collection();
            printf("\n");
            test_insert_document_no_cache();
            printf("\n");
            test_get_document();
            printf("\n");
            test_update_document();
            printf("\n");
            test_delete_document();
            printf("\n");
            test_query_documents();
            break;
        default:
            printf("Invalid test number: %d\n", test_num);
            return 1;
    }
    
    printf("\n=== Test completed successfully! ===\n");
    logger_close();
    
    return 0;
}