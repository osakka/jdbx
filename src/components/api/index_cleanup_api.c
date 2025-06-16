#include "api/api.h"
#include "core/server.h"
#include "database/index_cleanup.h"
#include "database/index_metrics.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Global cleanup instance */
static index_cleanup_t* g_index_cleanup = NULL;

/* Initialize cleanup system with database and metrics */
void index_cleanup_api_init(database_t* db, index_metrics_t* metrics) {
    if (!db || !metrics) {
        LOG_ERROR("Invalid parameters for index cleanup API init.");
        return;
    }
    
    if (!g_index_cleanup) {
        g_index_cleanup = index_cleanup_init(db, metrics);
        if (g_index_cleanup) {
            index_cleanup_start(g_index_cleanup);
            LOG_INFO("Index cleanup API initialized.");
        }
    }
}

/* Shutdown cleanup system */
void index_cleanup_api_shutdown(void) {
    if (g_index_cleanup) {
        index_cleanup_destroy(g_index_cleanup);
        g_index_cleanup = NULL;
        LOG_INFO("Index cleanup API shutdown.");
    }
}

/* GET /api/indexes/cleanup/status - Get cleanup system status */
http_response_t* api_handle_index_cleanup_status(api_context_t* ctx, http_request_t* request) {
    (void)ctx; (void)request; // Currently unused
    
    if (!g_index_cleanup) {
        return create_http_response(503,
                                  "{\"error\":\"Index cleanup not initialized\"}", 
                                  "application/json");
    }
    
    /* Export cleanup history */
    json_value_t* status = index_cleanup_export_history(g_index_cleanup);
    if (!status) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to get cleanup status\"}", 
                                  "application/json");
    }
    
    /* Convert to string */
    char* json_str = json_stringify(status);
    json_free(status);
    
    if (!json_str) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to serialize status\"}", 
                                  "application/json");
    }
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    BUFFER_FREE(json_str);
    
    return response;
}

/* POST /api/indexes/cleanup/configure - Configure cleanup thresholds */
http_response_t* api_handle_index_cleanup_configure(api_context_t* ctx, http_request_t* request) {
    (void)ctx; // Currently unused
    
    if (!g_index_cleanup) {
        return create_http_response(503,
                                  "{\"error\":\"Index cleanup not initialized\"}", 
                                  "application/json");
    }
    
    if (!request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Missing request body\"}", 
                                  "application/json");
    }
    
    /* Parse request body */
    json_value_t* config = json_parse(request->body);
    if (!config || config->type != JSON_OBJECT) {
        if (config) json_free(config);
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Invalid configuration object\"}", 
                                  "application/json");
    }
    
    /* Extract configuration values */
    double roi_threshold = -0.5;
    double effectiveness_threshold = 0.1;
    int min_age_hours = 24;
    int min_queries = 100;
    
    json_value_t* val = json_object_get(config, "roi_threshold");
    if (val && val->type == JSON_NUMBER) {
        roi_threshold = val->value.number;
    }
    
    val = json_object_get(config, "effectiveness_threshold");
    if (val && val->type == JSON_NUMBER) {
        effectiveness_threshold = val->value.number;
    }
    
    val = json_object_get(config, "min_age_hours");
    if (val && val->type == JSON_NUMBER) {
        min_age_hours = (int)val->value.number;
    }
    
    val = json_object_get(config, "min_queries");
    if (val && val->type == JSON_NUMBER) {
        min_queries = (int)val->value.number;
    }
    
    json_free(config);
    
    /* Apply configuration */
    index_cleanup_configure(g_index_cleanup, roi_threshold, effectiveness_threshold,
                          min_age_hours, min_queries);
    
    /* Return success */
    return create_http_response(HTTP_OK, "{\"status\":\"configured\"}", "application/json");
}

/* POST /api/indexes/cleanup/check - Force immediate cleanup check */
http_response_t* api_handle_index_cleanup_check(api_context_t* ctx, http_request_t* request) {
    (void)ctx; (void)request; // Currently unused
    
    if (!g_index_cleanup) {
        return create_http_response(503,
                                  "{\"error\":\"Index cleanup not initialized\"}", 
                                  "application/json");
    }
    
    /* Force cleanup check */
    int removed = index_cleanup_force_check(g_index_cleanup);
    
    /* Return result */
    char response[256];
    snprintf(response, sizeof(response), 
             "{\"status\":\"complete\",\"indexes_removed\":%d}", removed);
    
    return create_http_response(HTTP_OK, response, "application/json");
}

/* POST /api/indexes/{collection}/{index}/evaluate - Evaluate specific index */
http_response_t* api_handle_index_cleanup_evaluate(api_context_t* ctx, http_request_t* request) {
    (void)ctx; // Currently unused
    
    if (!g_index_cleanup) {
        return create_http_response(503,
                                  "{\"error\":\"Index cleanup not initialized\"}", 
                                  "application/json");
    }
    
    /* Extract collection and index from path */
    char* collection_name = NULL;
    char* index_name = NULL;
    
    /* Parse path to extract collection and index names */
    /* Expected format: /api/indexes/{collection}/{index}/evaluate */
    const char* path = request->path;
    const char* prefix = "/api/indexes/";
    if (strncmp(path, prefix, strlen(prefix)) != 0) {
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Invalid path format\"}", 
                                  "application/json");
    }
    
    const char* rest = path + strlen(prefix);
    const char* slash = strchr(rest, '/');
    if (!slash) {
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Missing index name\"}", 
                                  "application/json");
    }
    
    collection_name = strndup(rest, slash - rest);
    rest = slash + 1;
    
    slash = strchr(rest, '/');
    if (slash) {
        index_name = strndup(rest, slash - rest);
    } else {
        index_name = BUFFER_STRDUP(rest);
    }
    
    /* Evaluate the index */
    cleanup_decision_t* decision = index_cleanup_evaluate(g_index_cleanup,
                                                        collection_name, index_name);
    
    BUFFER_FREE(collection_name);
    BUFFER_FREE(index_name);
    
    if (!decision) {
        return create_http_response(HTTP_NOT_FOUND,
                                  "{\"error\":\"Index not found or no metrics\"}", 
                                  "application/json");
    }
    
    /* Build response */
    json_value_t* result = json_create_object();
    json_object_set(result, "collection", json_create_string(decision->collection_name));
    json_object_set(result, "index", json_create_string(decision->index_name));
    json_object_set(result, "should_remove", json_create_boolean(decision->should_remove));
    json_object_set(result, "roi", json_create_number(decision->roi));
    json_object_set(result, "effectiveness", json_create_number(decision->effectiveness));
    json_object_set(result, "storage_bytes", json_create_number(decision->storage_bytes));
    json_object_set(result, "query_count", json_create_number(decision->query_count));
    
    if (decision->should_remove) {
        const char* reason_str = "unknown";
        switch (decision->reason) {
            case CLEANUP_REASON_LOW_ROI: reason_str = "low_roi"; break;
            case CLEANUP_REASON_LOW_EFFECTIVENESS: reason_str = "low_effectiveness"; break;
            case CLEANUP_REASON_DUPLICATE: reason_str = "duplicate"; break;
            case CLEANUP_REASON_UNUSED: reason_str = "unused"; break;
            case CLEANUP_REASON_MANUAL: reason_str = "manual"; break;
        }
        json_object_set(result, "reason", json_create_string(reason_str));
    }
    
    BUFFER_FREE(decision);
    
    /* Convert to string */
    char* json_str = json_stringify(result);
    json_free(result);
    
    if (!json_str) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to serialize result\"}", 
                                  "application/json");
    }
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    BUFFER_FREE(json_str);
    
    return response;
}

/* DELETE /api/indexes/{collection}/{index} - Manually remove an index */
http_response_t* api_handle_index_cleanup_remove(api_context_t* ctx, http_request_t* request) {
    (void)ctx; // Currently unused
    
    if (!g_index_cleanup) {
        return create_http_response(503,
                                  "{\"error\":\"Index cleanup not initialized\"}", 
                                  "application/json");
    }
    
    /* Extract collection and index from path */
    char* collection_name = NULL;
    char* index_name = NULL;
    
    /* Parse path similar to evaluate */
    const char* path = request->path;
    const char* prefix = "/api/indexes/";
    if (strncmp(path, prefix, strlen(prefix)) != 0) {
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Invalid path format\"}", 
                                  "application/json");
    }
    
    const char* rest = path + strlen(prefix);
    const char* slash = strchr(rest, '/');
    if (!slash) {
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Missing index name\"}", 
                                  "application/json");
    }
    
    collection_name = strndup(rest, slash - rest);
    index_name = BUFFER_STRDUP(slash + 1);
    
    /* Remove the index */
    int result = index_cleanup_remove_index(g_index_cleanup, collection_name,
                                          index_name, CLEANUP_REASON_MANUAL);
    
    BUFFER_FREE(collection_name);
    BUFFER_FREE(index_name);
    
    if (result == 0) {
        return create_http_response(HTTP_OK, "{\"status\":\"removed\"}", "application/json");
    } else {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to remove index\"}", 
                                  "application/json");
    }
}