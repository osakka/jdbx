#include "jsondb/transaction/transaction.h"
#include "jsondb/api/api.h"
#include "jsondb/utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* API handler for getting transaction logs status and statistics */
http_response_t* api_handle_transaction_logs(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract database from context */
    database_t* db = ctx->db;
    if (!db || !db->transaction_manager || !db->transaction_manager->log) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Transaction log not available\"}", "application/json");
    }
    
    /* Get transaction log statistics */
    transaction_log_t* log = db->transaction_manager->log;
    json_value_t* stats = transaction_log_get_stats(log);
    
    if (!stats) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Failed to get transaction log statistics\"}", "application/json");
    }
    
    /* Convert to response */
    char* json_str = json_stringify(stats);
    json_free(stats);
    
    if (!json_str) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Failed to serialize statistics\"}", "application/json");
    }
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    free(json_str);
    
    return response;
}

/* API handler for configuring transaction logs */
http_response_t* api_handle_transaction_logs_configure(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract database from context */
    database_t* db = ctx->db;
    if (!db || !db->transaction_manager || !db->transaction_manager->log) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Transaction log not available\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Invalid JSON in request body\"}", "application/json");
    }
    
    /* Extract configuration parameters */
    transaction_log_t* log = db->transaction_manager->log;
    int log_level = log->log_level;
    int audit_enabled = log->audit_enabled;
    int auto_archive = log->auto_archive;
    int retention_days = log->retention_days;
    
    /* Update parameters from request if present */
    json_value_t* log_level_val = json_object_get(body, "log_level");
    if (log_level_val && log_level_val->type == JSON_INTEGER) {
        log_level = (int)log_level_val->value.integer;
        /* Validate log level (0-2) */
        if (log_level < 0 || log_level > 2) {
            log_level = log->log_level; /* Reset to current value */
        }
    }
    
    json_value_t* audit_enabled_val = json_object_get(body, "audit_enabled");
    if (audit_enabled_val) {
        if (audit_enabled_val->type == JSON_BOOLEAN) {
            audit_enabled = audit_enabled_val->value.boolean;
        } else if (audit_enabled_val->type == JSON_INTEGER) {
            audit_enabled = (audit_enabled_val->value.integer != 0);
        }
    }
    
    json_value_t* auto_archive_val = json_object_get(body, "auto_archive");
    if (auto_archive_val) {
        if (auto_archive_val->type == JSON_BOOLEAN) {
            auto_archive = auto_archive_val->value.boolean;
        } else if (auto_archive_val->type == JSON_INTEGER) {
            auto_archive = (auto_archive_val->value.integer != 0);
        }
    }
    
    json_value_t* retention_days_val = json_object_get(body, "retention_days");
    if (retention_days_val && retention_days_val->type == JSON_INTEGER) {
        retention_days = (int)retention_days_val->value.integer;
        /* Validate retention days (1-365) */
        if (retention_days < 1 || retention_days > 365) {
            retention_days = log->retention_days; /* Reset to current value */
        }
    }
    
    /* Apply the configuration */
    int result = transaction_log_configure(log, log_level, audit_enabled, auto_archive, retention_days);
    json_free(body);
    
    if (!result) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Failed to configure transaction log\"}", "application/json");
    }
    
    /* Get updated statistics */
    json_value_t* stats = transaction_log_get_stats(log);
    if (!stats) {
        return create_http_response(HTTP_OK, 
                                 "{\"status\":\"success\",\"message\":\"Configuration updated\"}", "application/json");
    }
    
    /* Add configuration status */
    json_object_set(stats, "status", json_create_string("success"));
    json_object_set(stats, "message", json_create_string("Configuration updated"));
    
    /* Convert to response */
    char* json_str = json_stringify(stats);
    json_free(stats);
    
    if (!json_str) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Failed to serialize response\"}", "application/json");
    }
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    free(json_str);
    
    return response;
}

/* API handler for archiving transaction logs */
http_response_t* api_handle_transaction_logs_archive(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract database from context */
    database_t* db = ctx->db;
    if (!db || !db->transaction_manager || !db->transaction_manager->log) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Transaction log not available\"}", "application/json");
    }
    
    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Invalid JSON in request body\"}", "application/json");
    }
    
    /* Extract archive directory */
    json_value_t* archive_dir_val = json_object_get(body, "archive_dir");
    if (!archive_dir_val || archive_dir_val->type != JSON_STRING) {
        json_free(body);
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Missing or invalid archive_dir parameter\"}", "application/json");
    }
    
    const char* archive_dir = archive_dir_val->value.string;
    
    /* Perform the archive operation */
    transaction_log_t* log = db->transaction_manager->log;
    int result = transaction_log_archive(log, archive_dir);
    json_free(body);
    
    if (!result) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Failed to archive transaction logs\"}", "application/json");
    }
    
    /* Return success */
    return create_http_response(HTTP_OK, 
                             "{\"status\":\"success\",\"message\":\"Transaction logs archived successfully\"}", 
                             "application/json");
}

/* API handler for generating transaction log reports */
http_response_t* api_handle_transaction_logs_report(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract database from context */
    database_t* db = ctx->db;
    if (!db || !db->transaction_manager || !db->transaction_manager->log) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Transaction log not available\"}", "application/json");
    }
    
    /* Parse query parameters */
    char* query_string = request->query;
    time_t start_time = 0;
    time_t end_time = time(NULL); /* Default to current time */
    
    if (query_string) {
        /* Simple query string parsing - in a real app, use a more robust parser */
        char* start_param = strstr(query_string, "start_time=");
        if (start_param) {
            start_time = (time_t)atol(start_param + 11);
        }
        
        char* end_param = strstr(query_string, "end_time=");
        if (end_param) {
            end_time = (time_t)atol(end_param + 9);
        }
    }
    
    /* Generate the report */
    transaction_log_t* log = db->transaction_manager->log;
    json_value_t* report = transaction_log_get_report(log, start_time, end_time);
    
    if (!report) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Failed to generate transaction log report\"}", "application/json");
    }
    
    /* Convert to response */
    char* json_str = json_stringify(report);
    json_free(report);
    
    if (!json_str) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Failed to serialize report\"}", "application/json");
    }
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    free(json_str);
    
    return response;
}

/* API handler for getting document history from transaction logs */
http_response_t* api_handle_transaction_logs_document_history(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract database from context */
    database_t* db = ctx->db;
    if (!db || !db->transaction_manager || !db->transaction_manager->log) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Transaction log not available\"}", "application/json");
    }
    
    /* Parse query parameters */
    char* query_string = request->query;
    if (!query_string) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Missing required query parameters\"}", "application/json");
    }
    
    /* Extract collection and document_id parameters */
    char* collection_param = strstr(query_string, "collection=");
    char* document_id_param = strstr(query_string, "document_id=");
    
    if (!collection_param || !document_id_param) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                 "{\"error\":\"Missing required parameters: collection and document_id\"}", 
                                 "application/json");
    }
    
    /* Extract parameter values (simplified parsing) */
    char collection[256] = {0};
    char document_id[256] = {0};
    
    /* Parse collection parameter */
    char* collection_value = collection_param + 11; /* Skip "collection=" */
    char* collection_end = strchr(collection_value, '&');
    size_t collection_len = collection_end ? (size_t)(collection_end - collection_value) : strlen(collection_value);
    if (collection_len >= sizeof(collection)) {
        collection_len = sizeof(collection) - 1;
    }
    strncpy(collection, collection_value, collection_len);
    collection[collection_len] = '\0';
    
    /* Parse document_id parameter */
    char* document_id_value = document_id_param + 12; /* Skip "document_id=" */
    char* document_id_end = strchr(document_id_value, '&');
    size_t document_id_len = document_id_end ? (size_t)(document_id_end - document_id_value) : strlen(document_id_value);
    if (document_id_len >= sizeof(document_id)) {
        document_id_len = sizeof(document_id) - 1;
    }
    strncpy(document_id, document_id_value, document_id_len);
    document_id[document_id_len] = '\0';
    
    /* Get document history */
    transaction_log_t* log = db->transaction_manager->log;
    json_value_t* history = transaction_log_get_document_history(log, collection, document_id);
    
    if (!history) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Failed to retrieve document history\"}", "application/json");
    }
    
    /* Convert to response */
    char* json_str = json_stringify(history);
    json_free(history);
    
    if (!json_str) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                 "{\"error\":\"Failed to serialize document history\"}", "application/json");
    }
    
    http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
    free(json_str);
    
    return response;
}