#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "database/database.h"
#include "rbac/rbac_database.h"
#include "utils/json.h"
#include "utils/logger.h"

int main() {
    // Initialize logger
    logger_init("stdout", LOG_LEVEL_DEBUG);
    
    // Open database
    database_t* db = db_open("/opt/jsondb/build/var/database.jdb");
    if (!db) {
        printf("Failed to open database\n");
        return 1;
    }
    
    // Query sessions before
    json_value_t* empty_query = json_create_object();
    json_value_t* before = db_query_documents(db, "_sessions", empty_query);
    json_free(empty_query);
    
    if (before) {
        json_value_t* docs = json_object_get(before, "documents");
        printf("Sessions before: %zu\n", json_array_size(docs));
        json_free(before);
    }
    
    // Create a test session directly
    json_value_t* session = json_create_object();
    json_object_set(session, "user_id", json_create_string("test-user"));
    json_object_set(session, "token", json_create_string("test-token"));
    json_object_set(session, "username", json_create_string("testuser"));
    json_object_set(session, "created_at", json_create_string("2025-01-26T19:30:00Z"));
    json_object_set(session, "expires_at", json_create_string("2025-01-26T20:00:00Z"));
    json_object_set(session, "active", json_create_boolean(1));
    
    // Insert session
    json_value_t* result = db_insert_document(db, "_sessions", session);
    json_free(session);
    
    if (result) {
        const char* id = json_get_string(json_object_get(result, "_id"));
        printf("Created session with ID: %s\n", id);
        json_free(result);
    } else {
        printf("Failed to create session\n");
    }
    
    // Query sessions after
    empty_query = json_create_object();
    json_value_t* after = db_query_documents(db, "_sessions", empty_query);
    json_free(empty_query);
    
    if (after) {
        json_value_t* docs = json_object_get(after, "documents");
        printf("Sessions after: %zu\n", json_array_size(docs));
        
        // Print all sessions
        for (size_t i = 0; i < json_array_size(docs); i++) {
            json_value_t* doc = json_array_get(docs, i);
            json_value_t* id = json_object_get(doc, "_id");
            json_value_t* user = json_object_get(doc, "username");
            printf("  Session %zu: ID=%s, User=%s\n", 
                   i, 
                   id ? json_get_string(id) : "?",
                   user ? json_get_string(user) : "?");
        }
        
        json_free(after);
    }
    
    // Save and close
    db_save(db);
    db_close(db);
    
    return 0;
}