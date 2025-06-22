#include "core/rate_limiter.h"
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"

// Create default rate limiter configuration
void rate_limiter_create_default_config(database_t* db) {
    if (!db) return;
    
    // Check if configuration already exists
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("config"));
    json_object_set(query, "name", json_create_string("rate_limiter"));
    json_object_set(query, "library", json_create_string("system"));
    
    json_value_t* result = storage_query_documents(db, query);
    // json_free(query); // CHECKPOINT: json_free(query);
    
    if (result && json_object_get(result, "documents")) {
        json_value_t* documents = json_object_get(result, "documents");
        if (json_array_size(documents) > 0) {
            // Configuration already exists
            // json_free(result); // CHECKPOINT: json_free(result);
            return;
        }
    }
    // if (result) json_free(result); // CHECKPOINT: json_free(result);
    
    // Create default configuration
    json_value_t* config_doc = json_create_object();
    json_object_set(config_doc, "uuid", json_create_string("config-rate-limiter"));
    json_object_set(config_doc, "type", json_create_string("config"));
    json_object_set(config_doc, "name", json_create_string("rate_limiter"));
    json_object_set(config_doc, "library", json_create_string("system"));
    
    // Configuration values
    json_value_t* config = json_create_object();
    json_object_set(config, "requests_per_minute", json_create_number(600));
    json_object_set(config, "burst_size", json_create_number(50));
    json_object_set(config, "connection_rate_limit", json_create_number(10));
    json_object_set(config, "circuit_failure_threshold", json_create_number(5));
    json_object_set(config, "circuit_reset_timeout", json_create_number(30));
    json_object_set(config, "circuit_half_open_requests", json_create_number(3));
    
    json_object_set(config_doc, "config", config);
    json_object_set(config_doc, "description", json_create_string("Rate limiter configuration"));
    
    // Insert the configuration
    storage_insert_document(db, config_doc);
    // json_free(config_doc); // CHECKPOINT: json_free(config_doc);
    
    LOG_INFO("Created default rate limiter configuration");
}