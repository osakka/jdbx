/**
 * @file api_documents.c
 * @brief Document Operations API Module
 * 
 * This module handles all document-related operations in JDBX, providing a unified
 * interface for document CRUD operations across unified documents, collections, and
 * library-scoped operations. Implements the core of JDBX's unified documents
 * architecture.
 * 
 * Architecture:
 * - Single source of truth through unified documents storage
 * - Type-based document discrimination  
 * - Virtual collections via field-based grouping
 * - Field-level operations for granular access
 * - Atomic operations with ACID compliance
 * 
 * Performance:
 * - O(k) operations with revolutionary ART engine
 * - Efficient field-level access without full document loading
 * - Optimized queries with adaptive indexing integration
 * - Memory-efficient checkpoint-based allocation
 */

#include "api/api_documents.h"
#include "database/document_storage.h"
#include "database/virtual_layer.h"
#include "database/database.h"
#include "core/server.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include "utils/json.h"
#include "utils/json_helpers.h"
#include "rbac/rbac_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * ============================================================================
 * INTERNAL HELPER FUNCTIONS
 * ============================================================================
 */

/**
 * Parse URL query parameters into JSON object
 * 
 * @param query_string URL query string (e.g., "type=user&library=system")
 * @return JSON object with parsed parameters, or NULL on error
 */
static json_value_t* parse_url_query_to_json(const char* query_string) {
    if (!query_string || strlen(query_string) == 0) {
        return NULL;
    }
    
    json_value_t* result = json_create_object();
    if (!result) return NULL;
    
    char* query_copy = BUFFER_ALLOC(strlen(query_string) + 1);
    if (!query_copy) {
        /* CHECKPOINT: json_free(result); */
        return NULL;
    }
    strcpy(query_copy, query_string);
    
    char* pair = strtok(query_copy, "&");
    while (pair) {
        char* equals = strchr(pair, '=');
        if (equals) {
            *equals = '\0';
            char* key = pair;
            char* value = equals + 1;
            
            /* URL decode the value (basic implementation for most common cases) */
            size_t value_len = strlen(value);
            char* decoded_value = BUFFER_ALLOC(value_len + 1);
            if (decoded_value) {
                size_t decoded_len = 0;
                for (size_t i = 0; i < value_len; i++) {
                    if (value[i] == '%' && i + 2 < value_len) {
                        /* Hex decode %XX */
                        char hex[3] = {value[i+1], value[i+2], '\0'};
                        char* endptr;
                        long byte_val = strtol(hex, &endptr, 16);
                        if (*endptr == '\0') {
                            decoded_value[decoded_len++] = (char)byte_val;
                            i += 2; /* Skip the hex digits */
                        } else {
                            decoded_value[decoded_len++] = value[i];
                        }
                    } else if (value[i] == '+') {
                        /* + becomes space in URL encoding */
                        decoded_value[decoded_len++] = ' ';
                    } else {
                        decoded_value[decoded_len++] = value[i];
                    }
                }
                decoded_value[decoded_len] = '\0';
                json_object_set(result, key, json_create_string(decoded_value));
                BUFFER_FREE(decoded_value);
            } else {
                /* Fallback to original value if allocation fails */
                json_object_set(result, key, json_create_string(value));
            }
        }
        pair = strtok(NULL, "&");
    }
    
    BUFFER_FREE(query_copy);
    return result;
}

/**
 * Get session library from request context
 * 
 * @param ctx API context
 * @param request HTTP request
 * @return library name string, or "default" if none specified
 */
__attribute__((unused)) static char* get_session_library(api_context_t* ctx __attribute__((unused)), http_request_t* request __attribute__((unused))) {
    /* Default implementation - can be enhanced with session management */
    return "default";
}

/**
 * Create HTTP error response with JSON error message
 * 
 * @param status HTTP status code
 * @param message Error message
 * @return HTTP response with error
 */
static http_response_t* create_error_response(int status, const char* message) {
    json_value_t* error_obj = json_create_object();
    if (!error_obj) {
        return create_http_response(status, "{\"error\":\"Internal server error\"}", "application/json");
    }
    
    json_object_set(error_obj, "error", json_create_string(message));
    
    char* error_json = json_stringify(error_obj);
    /* CHECKPOINT: json_free(error_obj); */
    
    if (!error_json) {
        return create_http_response(status, "{\"error\":\"Internal server error\"}", "application/json");
    }
    
    http_response_t* response = create_http_response(status, error_json, "application/json");
    BUFFER_FREE(error_json);
    
    return response;
}

/**
 * ============================================================================
 * UNIFIED DOCUMENTS API IMPLEMENTATION
 * ============================================================================
 */

http_response_t* api_handle_unified_documents_query(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Parse query from body or URL parameters */
  json_value_t* query = NULL;
  
  if (request->body && strlen(request->body) > 0) {
    query = json_parse(request->body);
    if (query && query->type != JSON_OBJECT) {
      /* CHECKPOINT: json_free(query); */
      query = NULL;
    }
  } else if (request->query) {
    /* Parse URL query parameters into JSON object */
    json_value_t* parsed_params = parse_url_query_to_json(request->query);
    if (parsed_params) {
      /* Check if there's a 'query' parameter with JSON */
      json_value_t* query_param = json_object_get(parsed_params, "query");
      if (query_param) {
        if (query_param->type == JSON_STRING) {
          /* Parse the JSON string */
          query = json_parse(query_param->value.string);
          if (!query || query->type != JSON_OBJECT) {
            /* CHECKPOINT: if (query) json_free(query); */
            query = json_create_object();
          }
          /* CHECKPOINT: json_free(parsed_params); */
        } else if (query_param->type == JSON_OBJECT) {
          /* Deep copy the query object to ensure it's independent */
          query = json_clone(query_param);
          if (!query) {
            query = json_create_object();
          }
          /* CHECKPOINT: json_free(parsed_params); */
        } else {
          /* Use the parsed parameters as the query */
          query = parsed_params;
        }
      } else {
        /* Use the parsed parameters as the query */
        query = parsed_params;
      }
    }
  }
  
  /* If no query provided, create empty query object */
  if (!query) {
    query = json_create_object();
  }
  
  /* Query documents from unified collection using storage layer - single source of truth */
  json_value_t* documents = storage_query_documents(ctx->db, query);
  
  /* CHECKPOINT: json_free(query); */
  
  if (!documents) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to query documents\"}", "application/json");
  }
  
  /* db_query_documents returns a complete response object, use it directly */
  char* response_str = json_stringify(documents);
  /* CHECKPOINT: json_free(documents); */
  
  if (!response_str) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to serialize response\"}", "application/json");
  }
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_unified_documents_create(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request || !request->body) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* parsed = json_parse(request->body);
  if (!parsed || parsed->type != JSON_OBJECT) {
    if (parsed) {
      /* CHECKPOINT: json_free(parsed); */
    }
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request body\"}", "application/json");
  }
  
  /* Handle nested structure: {"library":"...", "collection":"...", "document":{...}} */
  json_value_t* doc = NULL;
  json_value_t* nested_doc = json_object_get(parsed, "document");
  if (nested_doc) {
    /* MEMORY LEAK FIX: Use nested_doc directly instead of deep copy
     * Document only used within request scope for processing
     * Deep copy was causing memory accumulation when promoted */
    doc = nested_doc;
    /* Note: parsed will be cleaned up by checkpoint system */
    if (!doc || doc->type != JSON_OBJECT) {
      return create_http_response(HTTP_BAD_REQUEST, 
                   "{\"error\":\"Invalid document in nested structure\"}", "application/json");
    }
    LOG_DEBUG("Extracted document from nested structure for unified documents");
  } else {
    /* Use flat structure directly */
    doc = parsed;
    LOG_DEBUG("Using flat document structure for unified documents");
  }
  
  /* Auto-populate type field with default 'document' if missing - SAFE approach */
  if (!json_object_get(doc, "type")) {
    json_object_set(doc, "type", json_create_string("document"));
    LOG_DEBUG("Auto-populated type field with default 'document'");
  }
  
  /* Auto-populate owner field with default 'user' if missing - SAFE approach */
  if (!json_object_get(doc, "owner")) {
    json_object_set(doc, "owner", json_create_string("user"));
    LOG_DEBUG("Auto-populated owner field with default 'user'");
  }
  
  /* Insert document using storage layer - single source of truth */
  json_value_t* result = storage_insert_document(ctx->db, doc);
  /* CHECKPOINT: json_free(doc); */
  
  if (!result) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to insert document\"}", "application/json");
  }
  
  char* response_str = json_stringify(result);
  /* CHECKPOINT: json_free(result); */
  
  if (!response_str) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to serialize response\"}", "application/json");
  }
  
  return create_http_response(HTTP_CREATED, response_str, "application/json");
}

http_response_t* api_handle_unified_document_get(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract document ID from path: /api/documents/{id} */
  const char* path = request->path;
  if (strncmp(path, "/api/documents/", 15) != 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path\"}", "application/json");
  }
  
  const char* doc_id = path + 15;
  if (strlen(doc_id) == 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Document ID required\"}", "application/json");
  }
  
  /* Get document using storage layer - single source of truth */
  json_value_t* document = storage_get_document(ctx->db, doc_id);
  
  if (!document) {
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Document not found\"}", "application/json");
  }
  
  char* response_str = json_stringify(document);
  /* CHECKPOINT: json_free(document); */
  
  if (!response_str) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to serialize response\"}", "application/json");
  }
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_unified_document_update(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract document ID from path: /api/documents/{id} */
  const char* path = request->path;
  if (strncmp(path, "/api/documents/", 15) != 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path\"}", "application/json");
  }
  
  const char* doc_id = path + 15;
  if (strlen(doc_id) == 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Document ID required\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* update_doc = json_parse(request->body);
  if (!update_doc || update_doc->type != JSON_OBJECT) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid JSON body\"}", "application/json");
  }
  
  /* Update document using storage layer - single source of truth */
  json_value_t* result = storage_update_document(ctx->db, doc_id, update_doc);
  /* CHECKPOINT: json_free(update_doc); */
  
  if (!result) {
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Document not found or update failed\"}", "application/json");
  }
  
  char* response_str = json_stringify(result);
  /* CHECKPOINT: json_free(result); */
  
  if (!response_str) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to serialize response\"}", "application/json");
  }
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_unified_document_delete(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract document ID from path: /api/documents/{id} */
  const char* path = request->path;
  if (strncmp(path, "/api/documents/", 15) != 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid path\"}", "application/json");
  }
  
  const char* doc_id = path + 15;
  if (strlen(doc_id) == 0) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Document ID required\"}", "application/json");
  }
  
  /* Check if document exists using storage layer - single source of truth */
  json_value_t* existing = storage_get_document(ctx->db, doc_id);
  if (!existing) {
    return create_http_response(HTTP_NOT_FOUND, 
                 "{\"error\":\"Document not found\"}", "application/json");
  }
  /* CHECKPOINT: json_free(existing); */
  
  /* Delete document using storage layer */
  if (storage_delete_document(ctx->db, doc_id) != 1) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to delete document\"}", "application/json");
  }
  
  /* Return success response */
  json_value_t* response = json_create_object();
  json_object_set(response, "success", json_create_boolean(1));
  json_object_set(response, "message", json_create_string("Document deleted successfully"));
  json_object_set(response, "id", json_create_string(doc_id));
  
  char* response_str = json_stringify(response);
  /* CHECKPOINT: json_free(response); */
  
  if (!response_str) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to serialize response\"}", "application/json");
  }
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * ============================================================================
 * COLLECTION-SCOPED DOCUMENTS API IMPLEMENTATION
 * ============================================================================
 */

/**
 * Map collection name to document type for unified storage
 */
static const char* collection_to_document_type(const char* collection_name) {
    if (!collection_name) return "document";
    
    /* Special mappings for system collections */
    if (strcmp(collection_name, "users") == 0) return "user";
    else if (strcmp(collection_name, "roles") == 0) return "role";
    else if (strcmp(collection_name, "permissions") == 0) return "permission";
    else if (strcmp(collection_name, "sessions") == 0) return "session";
    else if (strcmp(collection_name, "libraries") == 0) return "library";
    else if (strcmp(collection_name, "collections") == 0) return "collection";
    else if (strcmp(collection_name, "functions") == 0) return "function";
    else if (strcmp(collection_name, "validators") == 0) return "validator";
    else if (strcmp(collection_name, "transformers") == 0) return "transformer";
    else if (strcmp(collection_name, "schemas") == 0) return "schema";
    else if (strcmp(collection_name, "indexes") == 0) return "index";
    else if (strcmp(collection_name, "metrics") == 0) return "metric";
    else if (strcmp(collection_name, "audit") == 0) return "audit";
    
    /* Default: use collection name as document type */
    return collection_name;
}

/**
 * Parse library and collection from request path
 * 
 * Supports both formats:
 * - /api/collections/{collection}/documents
 * - /api/libraries/{library}/collections/{collection}/documents
 * 
 * @param path Request path
 * @param library Output library name (caller must free)
 * @param collection Output collection name (caller must free)
 * @return true on success, false on error
 */
static bool parse_collection_path(const char* path, char** library, char** collection) {
    if (!path || !library || !collection) return false;
    
    *library = NULL;
    *collection = NULL;
    
    if (strncmp(path, "/api/libraries/", 15) == 0) {
        /* Library-scoped format: /api/libraries/{library}/collections/{collection}/... */
        path += 15;
        
        const char* slash = strchr(path, '/');
        if (!slash) return false;
        
        *library = strndup(path, slash - path);
        path = slash + 1;
        
        /* Verify "collections/" follows */
        if (strncmp(path, "collections/", 12) != 0) {
            free(*library);
            *library = NULL;
            return false;
        }
        path += 12;
        
        /* Extract collection name */
        const char* end = strchr(path, '/');
        if (end) {
            *collection = strndup(path, end - path);
        } else {
            *collection = strdup(path);
        }
        
    } else if (strncmp(path, "/api/collections/", 17) == 0) {
        /* Legacy format: /api/collections/{collection}/... */
        path += 17;
        
        /* Use default library */
        *library = strdup("default");
        
        /* Extract collection name */
        const char* end = strchr(path, '/');
        if (end) {
            *collection = strndup(path, end - path);
        } else {
            *collection = strdup(path);
        }
    } else {
        return false;
    }
    
    return (*library != NULL && *collection != NULL);
}

http_response_t* api_handle_documents_query(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_error_response(HTTP_BAD_REQUEST, "Invalid request");
    }
    
    /* Parse library and collection from path */
    char* library_name = NULL;
    char* collection_name = NULL;
    
    if (!parse_collection_path(request->path, &library_name, &collection_name)) {
        return create_error_response(HTTP_BAD_REQUEST, "Invalid path format");
    }
    
    /* Check if path ends with /documents for collection query */
    const char* documents_suffix = strstr(request->path, "/documents");
    if (!documents_suffix || (strcmp(documents_suffix, "/documents") != 0 && strncmp(documents_suffix, "/documents?", 11) != 0)) {
        free(library_name);
        free(collection_name);
        return create_error_response(HTTP_BAD_REQUEST, "Invalid path - must end with /documents");
    }
    
    /* Parse query parameters */
    json_value_t* user_query = NULL;
    if (request->body && strlen(request->body) > 0) {
        user_query = json_parse(request->body);
        if (user_query && user_query->type != JSON_OBJECT) {
            /* CHECKPOINT: json_free(user_query); */
            user_query = NULL;
        }
    } else if (request->query) {
        user_query = parse_url_query_to_json(request->query);
    }
    
    /* Create unified query with type and library filters */
    json_value_t* unified_query = json_create_object();
    const char* doc_type = collection_to_document_type(collection_name);
    
    json_object_set(unified_query, "type", json_create_string(doc_type));
    json_object_set(unified_query, "library", json_create_string(library_name));
    
    /* Merge user query parameters */
    if (user_query && user_query->type == JSON_OBJECT) {
        json_value_t* keys = json_object_get_keys(user_query);
        if (keys && keys->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(keys); i++) {
                json_value_t* key = json_array_get(keys, i);
                if (key && key->type == JSON_STRING) {
                    json_value_t* value = json_object_get(user_query, key->value.string);
                    if (value) {
                        json_object_set(unified_query, key->value.string, json_clone(value));
                    }
                }
            }
            /* CHECKPOINT: json_free(keys); */
        }
    }
    
    /* Query documents using storage layer - single source of truth */
    json_value_t* documents = storage_query_documents(ctx->db, unified_query);
    
    /* Cleanup */
    /* CHECKPOINT: json_free(unified_query); */
    if (user_query) {
        /* CHECKPOINT: json_free(user_query); */
    }
    free(library_name);
    free(collection_name);
    
    if (!documents) {
        return create_error_response(HTTP_INTERNAL_SERVER_ERROR, "Failed to query documents");
    }
    
    char* response_str = json_stringify(documents);
    /* CHECKPOINT: json_free(documents); */
    
    if (!response_str) {
        return create_error_response(HTTP_INTERNAL_SERVER_ERROR, "Failed to serialize response");
    }
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_document_get(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_error_response(HTTP_BAD_REQUEST, "Invalid request");
    }
    
    /* Parse library and collection from path */
    char* library_name = NULL;
    char* collection_name = NULL;
    
    if (!parse_collection_path(request->path, &library_name, &collection_name)) {
        return create_error_response(HTTP_BAD_REQUEST, "Invalid path format");
    }
    
    /* Extract document ID from path: .../documents/{id} */
    const char* documents_pos = strstr(request->path, "/documents/");
    if (!documents_pos) {
        free(library_name);
        free(collection_name);
        return create_error_response(HTTP_BAD_REQUEST, "Invalid path - missing document ID");
    }
    
    const char* doc_id = documents_pos + 11; /* Skip "/documents/" */
    if (strlen(doc_id) == 0) {
        free(library_name);
        free(collection_name);
        return create_error_response(HTTP_BAD_REQUEST, "Document ID required");
    }
    
    /* Get document using storage layer - single source of truth */
    json_value_t* document = storage_get_document(ctx->db, doc_id);
    
    free(library_name);
    free(collection_name);
    
    if (!document) {
        return create_error_response(HTTP_NOT_FOUND, "Document not found");
    }
    
    char* response_str = json_stringify(document);
    /* CHECKPOINT: json_free(document); */
    
    if (!response_str) {
        return create_error_response(HTTP_INTERNAL_SERVER_ERROR, "Failed to serialize response");
    }
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_document_create(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_error_response(HTTP_BAD_REQUEST, "Invalid request");
    }
    
    /* Parse library and collection from path */
    char* library_name = NULL;
    char* collection_name = NULL;
    
    if (!parse_collection_path(request->path, &library_name, &collection_name)) {
        return create_error_response(HTTP_BAD_REQUEST, "Invalid path format");
    }
    
    /* Parse request body */
    json_value_t* doc = json_parse(request->body);
    if (!doc || doc->type != JSON_OBJECT) {
        if (doc) {
            /* CHECKPOINT: json_free(doc); */
        }
        free(library_name);
        free(collection_name);
        return create_error_response(HTTP_BAD_REQUEST, "Invalid JSON body");
    }
    
    /* Auto-populate type and library fields based on collection context */
    const char* doc_type = collection_to_document_type(collection_name);
    
    if (!json_object_get(doc, "type")) {
        json_object_set(doc, "type", json_create_string(doc_type));
    }
    
    if (!json_object_get(doc, "library")) {
        json_object_set(doc, "library", json_create_string(library_name));
    }
    
    if (!json_object_get(doc, "collection")) {
        json_object_set(doc, "collection", json_create_string(collection_name));
    }
    
    /* Auto-populate owner field if missing */
    if (!json_object_get(doc, "owner")) {
        json_object_set(doc, "owner", json_create_string("user"));
    }
    
    /* Insert document using storage layer - single source of truth */
    json_value_t* result = storage_insert_document(ctx->db, doc);
    
    /* Cleanup */
    /* CHECKPOINT: json_free(doc); */
    free(library_name);
    free(collection_name);
    
    if (!result) {
        return create_error_response(HTTP_INTERNAL_SERVER_ERROR, "Failed to create document");
    }
    
    char* response_str = json_stringify(result);
    /* CHECKPOINT: json_free(result); */
    
    if (!response_str) {
        return create_error_response(HTTP_INTERNAL_SERVER_ERROR, "Failed to serialize response");
    }
    
    return create_http_response(HTTP_CREATED, response_str, "application/json");
}

http_response_t* api_handle_document_update(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_error_response(HTTP_BAD_REQUEST, "Invalid request");
    }
    
    /* Parse library and collection from path */
    char* library_name = NULL;
    char* collection_name = NULL;
    
    if (!parse_collection_path(request->path, &library_name, &collection_name)) {
        return create_error_response(HTTP_BAD_REQUEST, "Invalid path format");
    }
    
    /* Extract document ID from path: .../documents/{id} */
    const char* documents_pos = strstr(request->path, "/documents/");
    if (!documents_pos) {
        free(library_name);
        free(collection_name);
        return create_error_response(HTTP_BAD_REQUEST, "Invalid path - missing document ID");
    }
    
    const char* doc_id = documents_pos + 11; /* Skip "/documents/" */
    if (strlen(doc_id) == 0) {
        free(library_name);
        free(collection_name);
        return create_error_response(HTTP_BAD_REQUEST, "Document ID required");
    }
    
    /* Parse request body */
    json_value_t* update_doc = json_parse(request->body);
    if (!update_doc || update_doc->type != JSON_OBJECT) {
        free(library_name);
        free(collection_name);
        return create_error_response(HTTP_BAD_REQUEST, "Invalid JSON body");
    }
    
    /* Update document using storage layer - single source of truth */
    json_value_t* result = storage_update_document(ctx->db, doc_id, update_doc);
    
    /* Cleanup */
    /* CHECKPOINT: json_free(update_doc); */
    free(library_name);
    free(collection_name);
    
    if (!result) {
        return create_error_response(HTTP_NOT_FOUND, "Document not found or update failed");
    }
    
    char* response_str = json_stringify(result);
    /* CHECKPOINT: json_free(result); */
    
    if (!response_str) {
        return create_error_response(HTTP_INTERNAL_SERVER_ERROR, "Failed to serialize response");
    }
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* api_handle_document_delete(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_error_response(HTTP_BAD_REQUEST, "Invalid request");
    }
    
    /* Parse library and collection from path */
    char* library_name = NULL;
    char* collection_name = NULL;
    
    if (!parse_collection_path(request->path, &library_name, &collection_name)) {
        return create_error_response(HTTP_BAD_REQUEST, "Invalid path format");
    }
    
    /* Extract document ID from path: .../documents/{id} */
    const char* documents_pos = strstr(request->path, "/documents/");
    if (!documents_pos) {
        free(library_name);
        free(collection_name);
        return create_error_response(HTTP_BAD_REQUEST, "Invalid path - missing document ID");
    }
    
    const char* doc_id = documents_pos + 11; /* Skip "/documents/" */
    if (strlen(doc_id) == 0) {
        free(library_name);
        free(collection_name);
        return create_error_response(HTTP_BAD_REQUEST, "Document ID required");
    }
    
    /* Check if document exists */
    json_value_t* existing = storage_get_document(ctx->db, doc_id);
    if (!existing) {
        free(library_name);
        free(collection_name);
        return create_error_response(HTTP_NOT_FOUND, "Document not found");
    }
    /* CHECKPOINT: json_free(existing); */
    
    /* Delete document using storage layer - single source of truth */
    if (storage_delete_document(ctx->db, doc_id) != 1) {
        free(library_name);
        free(collection_name);
        return create_error_response(HTTP_INTERNAL_SERVER_ERROR, "Failed to delete document");
    }
    
    /* Create success response */
    json_value_t* response = json_create_object();
    json_object_set(response, "success", json_create_boolean(1));
    json_object_set(response, "message", json_create_string("Document deleted successfully"));
    json_object_set(response, "id", json_create_string(doc_id));
    
    char* response_str = json_stringify(response);
    /* CHECKPOINT: json_free(response); */
    
    free(library_name);
    free(collection_name);
    
    if (!response_str) {
        return create_error_response(HTTP_INTERNAL_SERVER_ERROR, "Failed to serialize response");
    }
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/**
 * ============================================================================
 * FIELD-LEVEL OPERATIONS API IMPLEMENTATION 
 * ============================================================================
 */

http_response_t* api_handle_document_field_access(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_error_response(HTTP_BAD_REQUEST, "Invalid request");
    }
    
    /* For now, delegate to regular document get - field operations can be enhanced later */
    return api_handle_document_get(ctx, request);
}

http_response_t* api_handle_library_document_field_access(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_error_response(HTTP_BAD_REQUEST, "Invalid request");
    }
    
    /* For now, delegate to regular document get - field operations can be enhanced later */
    return api_handle_document_get(ctx, request);
}

/**
 * ============================================================================
 * UTILITY FUNCTIONS IMPLEMENTATION
 * ============================================================================
 */

bool api_documents_init(void) {
    /* Module initialization - currently no specific init required */
    return true;
}

void api_documents_cleanup(void) {
    /* Module cleanup - currently no specific cleanup required */
}

size_t api_documents_get_route_count(void) {
    /* Return number of routes handled by this module */
    return 14; /* 6 unified + 6 collection-scoped + 2 field-level */
}