#include "api/api.h"
#include "database/database.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Parse collection name from query */
static char* parse_collection_query(http_request_t* request) {
    if (!request || !request->query) {
        return NULL;
    }
    
    char* query = strdup(request->query);
    if (!query) {
        return NULL;
    }
    
    char* collection = NULL;
    char* token = strtok(query, "&");
    
    while (token) {
        if (strncmp(token, "collection=", 11) == 0) {
            collection = strdup(token + 11);
            break;
        }
        token = strtok(NULL, "&");
    }
    
    free(query);
    return collection;
}

/* Count field types in a document */
static void count_field_types(json_value_t* document, json_value_t* type_counts, const char* prefix) {
    if (!document || !type_counts) {
        return;
    }
    
    switch (document->type) {
        case JSON_OBJECT: {
            for (size_t i = 0; i < document->value.object.size; i++) {
                const char* key = document->value.object.entries[i].key;
                json_value_t* value = document->value.object.entries[i].value;
                
                /* Create field path */
                char field_path[512] = {0};
                if (prefix && *prefix) {
                    sprintf(field_path, "%s.%s", prefix, key);
                } else {
                    sprintf(field_path, "%s", key);
                }
                
                /* Count field type */
                const char* type_str = NULL;
                switch (value->type) {
                    case JSON_NULL: type_str = "null"; break;
                    case JSON_BOOLEAN: type_str = "boolean"; break;
                    case JSON_NUMBER: type_str = "number"; break;
                    case JSON_INTEGER: type_str = "integer"; break;
                    case JSON_STRING: type_str = "string"; break;
                    case JSON_ARRAY: type_str = "array"; break;
                    case JSON_OBJECT: type_str = "object"; break;
                    default: type_str = "unknown";
                }
                
                /* Get or create type count */
                json_value_t* type_count = json_object_get(type_counts, field_path);
                if (!type_count) {
                    type_count = json_create_object();
                    json_object_set(type_counts, field_path, type_count);
                }
                
                /* Increment type count */
                json_value_t* count = json_object_get(type_count, type_str);
                if (!count) {
                    json_object_set(type_count, type_str, json_create_integer(1));
                } else {
                    count->value.integer++;
                }
                
                /* Recursively count nested fields */
                if (value->type == JSON_OBJECT) {
                    count_field_types(value, type_counts, field_path);
                }
            }
            break;
        }
        
        case JSON_ARRAY: {
            /* For arrays, we only count the types of the elements, not the paths */
            size_t array_size = json_array_size(document);
            for (size_t i = 0; i < array_size; i++) {
                json_value_t* value = json_array_get(document, i);
                if (value && value->type == JSON_OBJECT) {
                    count_field_types(value, type_counts, prefix);
                }
            }
            break;
        }
        
        default:
            /* Nothing to do for primitive types */
            break;
    }
}

/* Get distribution of values for a specific field */
static json_value_t* get_field_distribution(database_t* db, const char* collection, const char* field) {
    if (!db || !collection || !field) {
        return NULL;
    }
    
    /* Get collection */
    db_collection_t* coll = db_get_collection(db, collection);
    if (!coll || !coll->documents || coll->documents->type != JSON_ARRAY) {
        return NULL;
    }
    
    /* Create distribution object */
    json_value_t* distribution = json_create_object();
    
    /* Value counts */
    json_value_t* value_counts = json_create_object();
    json_object_set(distribution, "values", value_counts);
    
    /* Type counts */
    json_value_t* type_counts = json_create_object();
    json_object_set(distribution, "types", type_counts);
    
    /* Total documents */
    int total_docs = 0;
    
    /* Count values for field */
    for (size_t i = 0; i < json_array_size(coll->documents); i++) {
        json_value_t* doc = json_array_get(coll->documents, i);
        if (!doc || doc->type != JSON_OBJECT) {
            continue;
        }
        
        total_docs++;
        
        /* Split field path */
        char* field_copy = strdup(field);
        char* token = strtok(field_copy, ".");
        json_value_t* current = doc;
        
        /* Traverse field path */
        while (token && current && current->type == JSON_OBJECT) {
            current = json_object_get(current, token);
            token = strtok(NULL, ".");
        }
        
        free(field_copy);
        
        /* Count field value */
        if (current) {
            /* Type counting */
            const char* type_str = NULL;
            switch (current->type) {
                case JSON_NULL: type_str = "null"; break;
                case JSON_BOOLEAN: type_str = "boolean"; break;
                case JSON_NUMBER: type_str = "number"; break;
                case JSON_INTEGER: type_str = "integer"; break;
                case JSON_STRING: type_str = "string"; break;
                case JSON_ARRAY: type_str = "array"; break;
                case JSON_OBJECT: type_str = "object"; break;
                default: type_str = "unknown";
            }
            
            /* Increment type count */
            json_value_t* type_count = json_object_get(type_counts, type_str);
            if (!type_count) {
                json_object_set(type_counts, type_str, json_create_integer(1));
            } else {
                type_count->value.integer++;
            }
            
            /* Value counting for primitive types */
            if (current->type == JSON_NULL || 
                current->type == JSON_BOOLEAN || 
                current->type == JSON_INTEGER || 
                current->type == JSON_NUMBER || 
                current->type == JSON_STRING) {
                
                char value_str[256] = {0};
                
                switch (current->type) {
                    case JSON_NULL:
                        strcpy(value_str, "null");
                        break;
                        
                    case JSON_BOOLEAN:
                        sprintf(value_str, "%s", current->value.boolean ? "true" : "false");
                        break;
                        
                    case JSON_INTEGER:
                        sprintf(value_str, "%lld", (long long)current->value.integer);
                        break;
                        
                    case JSON_NUMBER:
                        sprintf(value_str, "%f", current->value.number);
                        break;
                        
                    case JSON_STRING:
                        if (strlen(current->value.string) < sizeof(value_str) - 1) {
                            strcpy(value_str, current->value.string);
                        } else {
                            strncpy(value_str, current->value.string, sizeof(value_str) - 4);
                            strcat(value_str, "...");
                        }
                        break;
                        
                    default:
                        break;
                }
                
                /* Increment value count */
                json_value_t* value_count = json_object_get(value_counts, value_str);
                if (!value_count) {
                    json_object_set(value_counts, value_str, json_create_integer(1));
                } else {
                    value_count->value.integer++;
                }
            }
        }
    }
    
    /* Set total count */
    json_object_set(distribution, "total", json_create_integer(total_docs));
    
    return distribution;
}

/* Handle collection statistics request */
http_response_t* api_handle_visualization_collection_stats(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Get collections list */
    json_value_t* collections = db_list_collections(ctx->db);
    if (!collections) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to list collections\"}", "application/json");
    }
    
    /* Create result array */
    json_value_t* stats = json_create_array();
    
    /* Process each collection */
    for (size_t i = 0; i < json_array_size(collections); i++) {
        json_value_t* coll_name_val = json_array_get(collections, i);
        if (!coll_name_val || coll_name_val->type != JSON_STRING) {
            continue;
        }
        
        const char* coll_name = coll_name_val->value.string;
        
        /* Get collection */
        db_collection_t* collection = db_get_collection(ctx->db, coll_name);
        if (!collection) {
            continue;
        }
        
        /* Get document count */
        size_t doc_count = 0;
        if (collection->documents && collection->documents->type == JSON_ARRAY) {
            doc_count = json_array_size(collection->documents);
        }
        
        /* Create collection stats */
        json_value_t* coll_stats = json_create_object();
        json_object_set(coll_stats, "name", json_create_string(coll_name));
        json_object_set(coll_stats, "count", json_create_integer(doc_count));
        
        /* Add to stats array */
        json_array_append(stats, coll_stats);
    }
    
    /* Create result object */
    json_value_t* result = json_create_object();
    json_object_set(result, "collections", stats);
    json_object_set(result, "timestamp", json_create_integer(time(NULL)));
    
    /* Serialize result */
    char* result_str = json_stringify(result);
    
    /* Free resources */
    json_free(result);
    json_free(collections);
    
    /* Create HTTP response */
    http_response_t* response = create_http_response(HTTP_OK, result_str, "application/json");
    
    /* Free result string */
    free(result_str);
    
    return response;
}

/* Handle document type analysis request */
http_response_t* api_handle_visualization_document_types(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Parse collection name from query */
    char* collection = parse_collection_query(request);
    if (!collection) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Collection parameter required\"}", "application/json");
    }
    
    /* Get collection */
    db_collection_t* coll = db_get_collection(ctx->db, collection);
    if (!coll) {
        free(collection);
        return create_http_response(HTTP_NOT_FOUND, 
                                  "{\"error\":\"Collection not found\"}", "application/json");
    }
    
    /* Create result object */
    json_value_t* result = json_create_object();
    json_object_set(result, "collection", json_create_string(collection));
    
    /* Create field types object */
    json_value_t* field_types = json_create_object();
    json_object_set(result, "field_types", field_types);
    
    /* Process documents */
    if (coll->documents && coll->documents->type == JSON_ARRAY) {
        size_t doc_count = json_array_size(coll->documents);
        json_object_set(result, "document_count", json_create_integer(doc_count));
        
        /* Count field types in each document */
        for (size_t i = 0; i < doc_count; i++) {
            json_value_t* doc = json_array_get(coll->documents, i);
            if (doc && doc->type == JSON_OBJECT) {
                count_field_types(doc, field_types, "");
            }
        }
    } else {
        json_object_set(result, "document_count", json_create_integer(0));
    }
    
    /* Add timestamp */
    json_object_set(result, "timestamp", json_create_integer(time(NULL)));
    
    /* Serialize result */
    char* result_str = json_stringify(result);
    
    /* Free resources */
    json_free(result);
    free(collection);
    
    /* Create HTTP response */
    http_response_t* response = create_http_response(HTTP_OK, result_str, "application/json");
    
    /* Free result string */
    free(result_str);
    
    return response;
}

/* Handle field distribution analysis request */
http_response_t* api_handle_visualization_field_distribution(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->query) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Parse query parameters */
    char* query = strdup(request->query);
    if (!query) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Memory allocation failed\"}", "application/json");
    }
    
    char* collection = NULL;
    char* field = NULL;
    
    char* token = strtok(query, "&");
    while (token) {
        if (strncmp(token, "collection=", 11) == 0) {
            collection = strdup(token + 11);
        } else if (strncmp(token, "field=", 6) == 0) {
            field = strdup(token + 6);
        }
        token = strtok(NULL, "&");
    }
    
    free(query);
    
    /* Check required parameters */
    if (!collection || !field) {
        if (collection) free(collection);
        if (field) free(field);
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Collection and field parameters required\"}", "application/json");
    }
    
    /* Get field distribution */
    json_value_t* distribution = get_field_distribution(ctx->db, collection, field);
    if (!distribution) {
        free(collection);
        free(field);
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                                  "{\"error\":\"Failed to analyze field distribution\"}", "application/json");
    }
    
    /* Create result object */
    json_value_t* result = json_create_object();
    json_object_set(result, "collection", json_create_string(collection));
    json_object_set(result, "field", json_create_string(field));
    json_object_set(result, "distribution", distribution);
    json_object_set(result, "timestamp", json_create_integer(time(NULL)));
    
    /* Serialize result */
    char* result_str = json_stringify(result);
    
    /* Free resources */
    json_free(result);
    free(collection);
    free(field);
    
    /* Create HTTP response */
    http_response_t* response = create_http_response(HTTP_OK, result_str, "application/json");
    
    /* Free result string */
    free(result_str);
    
    return response;
}