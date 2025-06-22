#include "core/rate_limiter.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include "utils/json.h"
#include <string.h>
#include <stdio.h>

// Default configuration values
#define DEFAULT_REQUESTS_PER_MINUTE 600
#define DEFAULT_BURST_SIZE 50
#define DEFAULT_CONNECTION_RATE_LIMIT 10
#define DEFAULT_CIRCUIT_FAILURE_THRESHOLD 5
#define DEFAULT_CIRCUIT_RESET_TIMEOUT 30
#define DEFAULT_CIRCUIT_HALF_OPEN_REQUESTS 3
#define CLEANUP_INTERVAL_SECONDS 300  // 5 minutes

// Initialize rate limiter with database backend
rate_limiter_t* rate_limiter_init(database_t* db) {
    if (!db) {
        LOG_ERROR("Cannot initialize rate limiter without database");
        return NULL;
    }
    
    rate_limiter_t* limiter = BUFFER_ALLOC(sizeof(rate_limiter_t));
    if (!limiter) {
        LOG_ERROR("Failed to allocate rate limiter");
        return NULL;
    }
    
    limiter->db = db;
    limiter->last_cleanup = time(NULL);
    
    // Load configuration from database or use defaults
    rate_limiter_reload_config(limiter);
    
    LOG_INFO("Rate limiter initialized with database backend");
    return limiter;
}

// Destroy rate limiter
void rate_limiter_destroy(rate_limiter_t* limiter) {
    if (limiter) {
        BUFFER_FREE(limiter);
    }
}

// Load configuration from database
void rate_limiter_reload_config(rate_limiter_t* limiter) {
    if (!limiter || !limiter->db) return;
    
    // Query for rate limiter configuration document
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("config"));
    json_object_set(query, "name", json_create_string("rate_limiter"));
    json_object_set(query, "library", json_create_string("system"));
    
    json_value_t* result = storage_query_documents(limiter->db, query);
    // json_free(query); // CHECKPOINT: json_free(query);
    
    if (result && json_object_get(result, "documents")) {
        json_value_t* documents = json_object_get(result, "documents");
        if (json_array_size(documents) > 0) {
            json_value_t* config_doc = json_array_get(documents, 0);
            json_value_t* config = json_object_get(config_doc, "config");
            
            if (config) {
                // Load configuration values
                json_value_t* val;
                
                val = json_object_get(config, "requests_per_minute");
                limiter->config.requests_per_minute = val && val->type == JSON_NUMBER ? (int)val->value.number : DEFAULT_REQUESTS_PER_MINUTE;
                
                val = json_object_get(config, "burst_size");
                limiter->config.burst_size = val && val->type == JSON_NUMBER ? (int)val->value.number : DEFAULT_BURST_SIZE;
                
                val = json_object_get(config, "connection_rate_limit");
                limiter->config.connection_rate_limit = val && val->type == JSON_NUMBER ? (int)val->value.number : DEFAULT_CONNECTION_RATE_LIMIT;
                
                val = json_object_get(config, "circuit_failure_threshold");
                limiter->config.circuit_failure_threshold = val && val->type == JSON_NUMBER ? (int)val->value.number : DEFAULT_CIRCUIT_FAILURE_THRESHOLD;
                
                val = json_object_get(config, "circuit_reset_timeout");
                limiter->config.circuit_reset_timeout = val && val->type == JSON_NUMBER ? (int)val->value.number : DEFAULT_CIRCUIT_RESET_TIMEOUT;
                
                val = json_object_get(config, "circuit_half_open_requests");
                limiter->config.circuit_half_open_requests = val && val->type == JSON_NUMBER ? (int)val->value.number : DEFAULT_CIRCUIT_HALF_OPEN_REQUESTS;
                
                LOG_INFO("Rate limiter configuration loaded from database");
                // json_free(result); // CHECKPOINT: json_free(result);
                return;
            }
        }
    }
    
    // Use default configuration
    limiter->config.requests_per_minute = DEFAULT_REQUESTS_PER_MINUTE;
    limiter->config.burst_size = DEFAULT_BURST_SIZE;
    limiter->config.connection_rate_limit = DEFAULT_CONNECTION_RATE_LIMIT;
    limiter->config.circuit_failure_threshold = DEFAULT_CIRCUIT_FAILURE_THRESHOLD;
    limiter->config.circuit_reset_timeout = DEFAULT_CIRCUIT_RESET_TIMEOUT;
    limiter->config.circuit_half_open_requests = DEFAULT_CIRCUIT_HALF_OPEN_REQUESTS;
    
    LOG_INFO("Rate limiter using default configuration");
    // if (result) json_free(result); // CHECKPOINT: json_free(result);
}

// Check if request is allowed (token bucket algorithm)
int rate_limiter_check_request(rate_limiter_t* limiter, const char* ip_address) {
    if (!limiter || !limiter->db || !ip_address) return 1; // Allow if not configured
    
    // Query for existing rate limit document for this IP
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string(DOC_TYPE_RATE_LIMIT));
    json_object_set(query, "ip_address", json_create_string(ip_address));
    json_object_set(query, "library", json_create_string("system"));
    
    json_value_t* result = storage_query_documents(limiter->db, query);
    json_value_t* doc = NULL;
    char doc_id[256] = {0};
    
    if (result && json_object_get(result, "documents")) {
        json_value_t* documents = json_object_get(result, "documents");
        if (json_array_size(documents) > 0) {
            doc = json_array_get(documents, 0);
            // Get the UUID for updates
            json_value_t* uuid_val = json_object_get(doc, "uuid");
            if (uuid_val && uuid_val->type == JSON_STRING) {
                strncpy(doc_id, uuid_val->value.string, sizeof(doc_id) - 1);
            }
        }
    }
    
    time_t now = time(NULL);
    double tokens = limiter->config.burst_size;
    time_t last_update = now;
    
    if (doc) {
        // Existing rate limit document
        json_value_t* tokens_val = json_object_get(doc, "tokens");
        json_value_t* last_update_val = json_object_get(doc, "last_update");
        
        if (tokens_val && last_update_val) {
            tokens = tokens_val->value.number;
            last_update = (time_t)last_update_val->value.number;
            
            // Calculate tokens accumulated since last update
            double elapsed = difftime(now, last_update);
            double tokens_per_second = (double)limiter->config.requests_per_minute / 60.0;
            tokens += elapsed * tokens_per_second;
            
            // Cap at burst size
            if (tokens > limiter->config.burst_size) {
                tokens = limiter->config.burst_size;
            }
        }
        // json_free(doc); // CHECKPOINT: json_free(doc);
    }
    
    // Check if we have tokens available
    if (tokens >= 1.0) {
        // Consume the token immediately
        tokens -= 1.0;
        
        // Update the document with consumed token
        json_value_t* rate_doc = json_create_object();
        if (doc_id[0]) {
            // Update existing document
            json_object_set(rate_doc, "uuid", json_create_string(doc_id));
        }
        json_object_set(rate_doc, "type", json_create_string(DOC_TYPE_RATE_LIMIT));
        json_object_set(rate_doc, "library", json_create_string("system"));
        json_object_set(rate_doc, "ip_address", json_create_string(ip_address));
        json_object_set(rate_doc, "tokens", json_create_number(tokens));
        json_object_set(rate_doc, "last_update", json_create_number((double)now));
        json_object_set(rate_doc, "expires_at", json_create_number((double)(now + 3600)));
        
        if (doc && doc_id[0]) {
            storage_update_document(limiter->db, doc_id, rate_doc);
        } else {
            storage_insert_document(limiter->db, rate_doc);
        }
        
        // Periodic cleanup
        if (now - limiter->last_cleanup > CLEANUP_INTERVAL_SECONDS) {
            rate_limiter_cleanup_expired(limiter);
            limiter->last_cleanup = now;
        }
        
        return 1; // Request allowed
    }
    
    LOG_WARNING("Rate limit exceeded for IP: %s (tokens: %.2f)", ip_address, tokens);
    return 0; // Request denied
}

// Record a request (token already consumed in check)
void rate_limiter_record_request(rate_limiter_t* limiter, const char* ip_address) {
    // This function is now a no-op since token consumption happens in rate_limiter_check_request
    // Kept for API compatibility
    (void)limiter;
    (void)ip_address;
}

// Get circuit breaker state
circuit_state_t circuit_breaker_get_state(rate_limiter_t* limiter, const char* service_name) {
    if (!limiter || !limiter->db || !service_name) return CIRCUIT_CLOSED;
    
    // Create document ID for this service's circuit breaker
    char doc_id[256];
    snprintf(doc_id, sizeof(doc_id), "circuit-%s", service_name);
    
    // Get circuit breaker document
    json_value_t* doc = storage_get_document(limiter->db, doc_id);
    if (!doc) return CIRCUIT_CLOSED; // No document means circuit is closed
    
    json_value_t* state_val = json_object_get(doc, "state");
    json_value_t* last_failure_val = json_object_get(doc, "last_failure");
    json_value_t* half_open_requests_val = json_object_get(doc, "half_open_requests");
    
    if (!state_val) {
        // json_free(doc); // CHECKPOINT: json_free(doc);
        return CIRCUIT_CLOSED;
    }
    
    int state = (int)state_val->value.number;
    
    // Check if circuit should transition from OPEN to HALF_OPEN
    if (state == CIRCUIT_OPEN && last_failure_val) {
        time_t last_failure = (time_t)last_failure_val->value.number;
        time_t now = time(NULL);
        
        if (now - last_failure >= limiter->config.circuit_reset_timeout) {
            // Transition to half-open
            state = CIRCUIT_HALF_OPEN;
            
            // Update document
            json_object_set(doc, "state", json_create_number(CIRCUIT_HALF_OPEN));
            json_object_set(doc, "half_open_requests", json_create_number(0));
            storage_update_document(limiter->db, doc_id, doc);
        }
    }
    
    // Check if we've exceeded half-open requests
    if (state == CIRCUIT_HALF_OPEN && half_open_requests_val) {
        int requests = (int)half_open_requests_val->value.number;
        if (requests >= limiter->config.circuit_half_open_requests) {
            // Too many requests in half-open state, open circuit again
            state = CIRCUIT_OPEN;
            
            // Update document
            json_object_set(doc, "state", json_create_number(CIRCUIT_OPEN));
            json_object_set(doc, "last_failure", json_create_number((double)time(NULL)));
            storage_update_document(limiter->db, doc_id, doc);
        }
    }
    
    // json_free(doc); // CHECKPOINT: json_free(doc);
    return (circuit_state_t)state;
}

// Record circuit breaker success
void circuit_breaker_record_success(rate_limiter_t* limiter, const char* service_name) {
    if (!limiter || !limiter->db || !service_name) return;
    
    // Create document ID for this service's circuit breaker
    char doc_id[256];
    snprintf(doc_id, sizeof(doc_id), "circuit-%s", service_name);
    
    // Get circuit breaker document
    json_value_t* doc = storage_get_document(limiter->db, doc_id);
    if (!doc) return; // No document means circuit is closed, nothing to do
    
    json_value_t* state_val = json_object_get(doc, "state");
    if (!state_val) {
        // json_free(doc); // CHECKPOINT: json_free(doc);
        return;
    }
    
    int state = (int)state_val->value.number;
    
    if (state == CIRCUIT_HALF_OPEN) {
        // Success in half-open state, close the circuit
        json_object_set(doc, "state", json_create_number(CIRCUIT_CLOSED));
        json_object_set(doc, "failure_count", json_create_number(0));
        json_object_set(doc, "half_open_requests", json_create_number(0));
        storage_update_document(limiter->db, doc_id, doc);
        
        LOG_INFO("Circuit breaker closed for service: %s", service_name);
    }
    
    // json_free(doc); // CHECKPOINT: json_free(doc);
}

// Record circuit breaker failure
void circuit_breaker_record_failure(rate_limiter_t* limiter, const char* service_name) {
    if (!limiter || !limiter->db || !service_name) return;
    
    // Create document ID for this service's circuit breaker
    char doc_id[256];
    snprintf(doc_id, sizeof(doc_id), "circuit-%s", service_name);
    
    // Get or create circuit breaker document
    json_value_t* doc = storage_get_document(limiter->db, doc_id);
    
    int failure_count = 1;
    int state = CIRCUIT_CLOSED;
    time_t now = time(NULL);
    
    if (doc) {
        // Update existing document
        json_value_t* failure_count_val = json_object_get(doc, "failure_count");
        json_value_t* state_val = json_object_get(doc, "state");
        
        if (failure_count_val) {
            failure_count = (int)failure_count_val->value.number + 1;
        }
        
        if (state_val) {
            state = (int)state_val->value.number;
        }
        
        // Increment half-open requests if in half-open state
        if (state == CIRCUIT_HALF_OPEN) {
            json_value_t* half_open_val = json_object_get(doc, "half_open_requests");
            int half_open_requests = half_open_val ? (int)half_open_val->value.number : 0;
            json_object_set(doc, "half_open_requests", json_create_number(half_open_requests + 1));
        }
    } else {
        // Create new document
        doc = json_create_object();
        json_object_set(doc, "uuid", json_create_string(doc_id));
        json_object_set(doc, "type", json_create_string(DOC_TYPE_CIRCUIT_BREAKER));
        json_object_set(doc, "library", json_create_string("system"));
        json_object_set(doc, "service_name", json_create_string(service_name));
    }
    
    // Check if we should open the circuit
    if (failure_count >= limiter->config.circuit_failure_threshold && state == CIRCUIT_CLOSED) {
        state = CIRCUIT_OPEN;
        LOG_WARNING("Circuit breaker opened for service: %s (failures: %d)", service_name, failure_count);
    }
    
    // Update document
    json_object_set(doc, "state", json_create_number(state));
    json_object_set(doc, "failure_count", json_create_number(failure_count));
    json_object_set(doc, "last_failure", json_create_number((double)now));
    json_object_set(doc, "expires_at", json_create_number((double)(now + 3600))); // 1 hour expiry
    
    if (storage_get_document(limiter->db, doc_id)) {
        storage_update_document(limiter->db, doc_id, doc);
    } else {
        storage_insert_document(limiter->db, doc);
    }
    
    // json_free(doc); // CHECKPOINT: json_free(doc);
}

// Check connection rate limit
int connection_rate_check(rate_limiter_t* limiter, const char* ip_address) {
    if (!limiter || !limiter->db || !ip_address) return 1; // Allow if not configured
    
    // Create document ID for this IP's connection rate
    char doc_id[256];
    snprintf(doc_id, sizeof(doc_id), "connrate-%s", ip_address);
    
    // Get connection rate document
    json_value_t* doc = storage_get_document(limiter->db, doc_id);
    
    time_t now = time(NULL);
    int connections_this_second = 0;
    
    if (doc) {
        json_value_t* timestamp_val = json_object_get(doc, "timestamp");
        json_value_t* count_val = json_object_get(doc, "count");
        
        if (timestamp_val && count_val) {
            time_t timestamp = (time_t)timestamp_val->value.number;
            
            // If within the same second, check count
            if (timestamp == now) {
                connections_this_second = (int)count_val->value.number;
                
                if (connections_this_second >= limiter->config.connection_rate_limit) {
                    LOG_WARNING("Connection rate limit exceeded for IP: %s (%d connections/sec)", 
                               ip_address, connections_this_second);
                    // json_free(doc); // CHECKPOINT: json_free(doc);
                    return 0; // Deny connection
                }
            }
        }
        // json_free(doc); // CHECKPOINT: json_free(doc);
    }
    
    return 1; // Allow connection
}

// Record a connection
void connection_rate_record(rate_limiter_t* limiter, const char* ip_address) {
    if (!limiter || !limiter->db || !ip_address) return;
    
    // Create document ID for this IP's connection rate
    char doc_id[256];
    snprintf(doc_id, sizeof(doc_id), "connrate-%s", ip_address);
    
    // Get connection rate document
    json_value_t* doc = storage_get_document(limiter->db, doc_id);
    
    time_t now = time(NULL);
    int connections_this_second = 1;
    
    if (doc) {
        json_value_t* timestamp_val = json_object_get(doc, "timestamp");
        json_value_t* count_val = json_object_get(doc, "count");
        
        if (timestamp_val && count_val) {
            time_t timestamp = (time_t)timestamp_val->value.number;
            
            // If within the same second, increment count
            if (timestamp == now) {
                connections_this_second = (int)count_val->value.number + 1;
            }
        }
    } else {
        // Create new document
        doc = json_create_object();
        json_object_set(doc, "uuid", json_create_string(doc_id));
        json_object_set(doc, "type", json_create_string(DOC_TYPE_CONNECTION_RATE));
        json_object_set(doc, "library", json_create_string("system"));
        json_object_set(doc, "ip_address", json_create_string(ip_address));
    }
    
    // Update document
    json_object_set(doc, "timestamp", json_create_number((double)now));
    json_object_set(doc, "count", json_create_number(connections_this_second));
    json_object_set(doc, "expires_at", json_create_number((double)(now + 60))); // 1 minute expiry
    
    if (storage_get_document(limiter->db, doc_id)) {
        storage_update_document(limiter->db, doc_id, doc);
    } else {
        storage_insert_document(limiter->db, doc);
    }
    
    // json_free(doc); // CHECKPOINT: json_free(doc);
}

// Cleanup expired documents
void rate_limiter_cleanup_expired(rate_limiter_t* limiter) {
    if (!limiter || !limiter->db) return;
    
    time_t now = time(NULL);
    
    // Query for expired rate limit documents
    json_value_t* query = json_create_object();
    json_value_t* type_array = json_create_array();
    json_array_append(type_array, json_create_string(DOC_TYPE_RATE_LIMIT));
    json_array_append(type_array, json_create_string(DOC_TYPE_CIRCUIT_BREAKER));
    json_array_append(type_array, json_create_string(DOC_TYPE_CONNECTION_RATE));
    json_object_set(query, "type", type_array);
    json_object_set(query, "library", json_create_string("system"));
    
    json_value_t* result = storage_query_documents(limiter->db, query);
    // json_free(query); // CHECKPOINT: json_free(query);
    
    if (result && json_object_get(result, "documents")) {
        json_value_t* documents = json_object_get(result, "documents");
        int doc_count = json_array_size(documents);
        int cleaned = 0;
        
        for (int i = 0; i < doc_count; i++) {
            json_value_t* doc = json_array_get(documents, i);
            json_value_t* expires_val = json_object_get(doc, "expires_at");
            json_value_t* uuid_val = json_object_get(doc, "uuid");
            
            if (expires_val && uuid_val) {
                time_t expires_at = (time_t)expires_val->value.number;
                
                if (expires_at < now) {
                    // Document expired, delete it
                    const char* uuid = uuid_val->value.string;
                    storage_delete_document(limiter->db, uuid);
                    cleaned++;
                }
            }
        }
        
        if (cleaned > 0) {
            LOG_INFO("Rate limiter cleanup: removed %d expired documents", cleaned);
        }
    }
    
    // if (result) json_free(result); // CHECKPOINT: json_free(result);
}