#include "api/api.h"
#include "database/database.h"
#include "database/unified_documents.h"
#include "database/collection_defaults.h"
#include "rbac/rbac_database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

/* Forward declarations */
http_response_t* api_handle_get_library_stats(api_context_t* ctx, http_request_t* request);

/* Get list of all libraries */
http_response_t* api_handle_get_libraries(api_context_t* ctx, http_request_t* request) {
  (void)request;
  if (!ctx || !ctx->db) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Database not initialized\"}", "application/json");
  }
  
  /* Query library documents from unified documents */
  json_value_t* query = json_create_object();
  json_object_set(query, "type", json_create_string("library"));
  
  json_value_t* results = db_query_documents(ctx->db, "documents", query);
  json_free(query);
  
  if (!results) {
    /* If no documents collection, return empty list */
    json_value_t* response = json_create_object();
    json_object_set(response, "libraries", json_create_array());
    
    char* response_str = json_stringify(response);
    json_free(response);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
  }
  
  /* Extract documents array */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY) {
    json_free(results);
    
    /* Return empty array */
    json_value_t* response = json_create_object();
    json_object_set(response, "libraries", json_create_array());
    
    char* response_str = json_stringify(response);
    json_free(response);
    
    return create_http_response(HTTP_OK, response_str, "application/json");
  }
  
  /* Create response */
  json_value_t* response = json_create_object();
  json_object_set(response, "libraries", json_clone(documents));
  
  json_free(results);
  
  char* response_str = json_stringify(response);
  json_free(response);
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Create a new library */
http_response_t* api_handle_create_library(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid JSON body\"}", "application/json");
  }
  
  /* Get library name */
  json_value_t* name_val = json_object_get(body, "name");
  if (!name_val || name_val->type != JSON_STRING) {
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Library name required\"}", "application/json");
  }
  
  const char* library_name = name_val->value.string;
  
  /* Validate library name */
  if (strlen(library_name) == 0 || strchr(library_name, '/') != NULL ||
      strchr(library_name, '\\') != NULL || strchr(library_name, '.') != NULL) {
    json_free(body);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid library name\"}", "application/json");
  }
  
  /* Check if library already exists */
  char test_path[1024];
  snprintf(test_path, sizeof(test_path), "%s/users", library_name);
  if (db_collection_exists(ctx->db, test_path)) {
    json_free(body);
    return create_http_response(HTTP_CONFLICT,
                 "{\"error\":\"Library already exists\"}", "application/json");
  }
  
  /* Get template name (optional) */
  json_value_t* template_val = json_object_get(body, "template");
  const char* template_name = template_val && template_val->type == JSON_STRING ? 
                              template_val->value.string : "standard";
  
  /* Library directory will be created automatically when first collection is created */
  
  /* Look up template from library_templates collection */
  json_value_t* template_collections = NULL;
  json_value_t* template_settings = NULL;
  
  if (db_collection_exists(ctx->db, "documents")) {
    /* Query for the template */
    json_value_t* template_query = json_create_object();
    json_object_set(template_query, "type", json_create_string("library_template"));
    json_object_set(template_query, "name", json_create_string(template_name));
    
    json_value_t* template_results = db_query_documents(ctx->db, "documents", template_query);
    json_free(template_query);
    
    if (template_results) {
      json_value_t* templates = json_object_get(template_results, "documents");
      if (templates && templates->type == JSON_ARRAY && json_array_size(templates) > 0) {
        json_value_t* template_doc = json_array_get(templates, 0);
        
        /* Get collections from template */
        json_value_t* cols = json_object_get(template_doc, "collections");
        if (cols && cols->type == JSON_ARRAY) {
          template_collections = json_clone(cols);
        }
        
        /* Get settings from template */
        json_value_t* settings = json_object_get(template_doc, "settings");
        if (settings && settings->type == JSON_OBJECT) {
          template_settings = json_clone(settings);
        }
      }
      json_free(template_results);
    }
  }
  
  /* If no template found, use default collections */
  if (!template_collections) {
    /* Define standard collections based on template name */
    const char* standard_collections[] = {
      "users", "roles", "permissions", "sessions", "metrics", NULL
    };
    
    const char* full_collections[] = {
      "users", "roles", "permissions", "sessions", "metrics",
      "audit", "schemas", "validators", "transformers", "functions", NULL
    };
    
    const char* minimal_collections[] = {
      "users", "roles", NULL
    };
    
    const char** collections = standard_collections;
    if (strcmp(template_name, "full") == 0) {
      collections = full_collections;
    } else if (strcmp(template_name, "minimal") == 0) {
      collections = minimal_collections;
    }
    
    /* Create default collections */
    for (int i = 0; collections[i] != NULL; i++) {
      char collection_path[512];
      snprintf(collection_path, sizeof(collection_path), "%s/%s", library_name, collections[i]);
      
      if (db_create_collection(ctx->db, collection_path) != 0) {
        LOG_WARNING("Failed to create collection %s in library %s", collections[i], library_name);
      }
    }
  } else {
    /* Create collections from template */
    size_t num_collections = json_array_size(template_collections);
    for (size_t i = 0; i < num_collections; i++) {
      json_value_t* col_def = json_array_get(template_collections, i);
      json_value_t* col_name_val = json_object_get(col_def, "name");
      
      if (col_name_val && col_name_val->type == JSON_STRING) {
        const char* col_name = col_name_val->value.string;
        char collection_path[512];
        snprintf(collection_path, sizeof(collection_path), "%s/%s", library_name, col_name);
        
        if (db_create_collection(ctx->db, collection_path) != 0) {
          LOG_WARNING("Failed to create collection %s in library %s", col_name, library_name);
        }
        
        /* TODO: Apply schema if provided */
        json_value_t* schema = json_object_get(col_def, "schema");
        if (schema) {
          /* Future: Apply schema to collection */
        }
      }
    }
    json_free(template_collections);
  }
  
  /* Create library metadata document if documents collection exists */
  if (db_collection_exists(ctx->db, "documents")) {
    json_value_t* lib_doc = json_create_object();
    json_object_set(lib_doc, "type", json_create_string("library"));
    json_object_set(lib_doc, "name", json_create_string(library_name));
    json_object_set(lib_doc, "template", json_create_string(template_name));
    
    /* Add timestamps */
    time_t now = time(NULL);
    char timestamp[64];
    struct tm* utc_tm = gmtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
    json_object_set(lib_doc, "created_at", json_create_string(timestamp));
    json_object_set(lib_doc, "updated_at", json_create_string(timestamp));
    
    /* Add settings from template if available */
    if (template_settings) {
      json_object_set(lib_doc, "settings", template_settings);
    }
    
    /* TODO: Add owner from JWT token when available */
    /* For now, set owner as admin */
    json_object_set(lib_doc, "owner", json_create_string("admin"));
    
    /* Save to documents collection */
    json_value_t* result = db_insert_document(ctx->db, "documents", lib_doc);
    if (result) {
      json_free(result);
    }
    json_free(lib_doc);
  }
  
  json_free(body);
  
  /* Return success response */
  json_value_t* response = json_create_object();
  json_object_set(response, "success", json_create_boolean(1));
  json_object_set(response, "name", json_create_string(library_name));
  json_object_set(response, "message", json_create_string("Library created successfully"));
  
  char* response_str = json_stringify(response);
  json_free(response);
  
  return create_http_response(HTTP_CREATED, response_str, "application/json");
}

/* Delete a library */
http_response_t* api_handle_delete_library(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract library name from URL */
  const char* library_name = NULL;
  if (strncmp(request->path, "/api/libraries/", 15) == 0) {
    library_name = request->path + 15;
  }
  
  if (!library_name || strlen(library_name) == 0) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Library name required\"}", "application/json");
  }
  
  /* Prevent deletion of system library */
  if (strcmp(library_name, "system") == 0) {
    return create_http_response(HTTP_FORBIDDEN,
                 "{\"error\":\"Cannot delete system library\"}", "application/json");
  }
  
  /* Check if library exists */
  char test_path[1024];
  snprintf(test_path, sizeof(test_path), "%s/users", library_name);
  if (!db_collection_exists(ctx->db, test_path)) {
    return create_http_response(HTTP_NOT_FOUND,
                 "{\"error\":\"Library not found\"}", "application/json");
  }
  
  /* List all collections in the library */
  json_value_t* collections = json_create_array();
  char lib_path[1024];
  snprintf(lib_path, sizeof(lib_path), "%s/%s", ctx->db->path, library_name);
  
  DIR* dir = opendir(lib_path);
  if (dir) {
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
      if (strstr(entry->d_name, ".mmap") != NULL) {
        /* Extract collection name without extension */
        char coll_name[256];
        strncpy(coll_name, entry->d_name, sizeof(coll_name) - 1);
        char* dot = strrchr(coll_name, '.');
        if (dot) *dot = '\0';
        
        /* Drop collection */
        char collection_path[512];
        snprintf(collection_path, sizeof(collection_path), "%s/%s", library_name, coll_name);
        db_drop_collection(ctx->db, collection_path);
        
        json_array_append(collections, json_create_string(coll_name));
      }
    }
    closedir(dir);
  }
  
  /* Remove library directory */
  if (rmdir(lib_path) != 0) {
    LOG_WARNING("Failed to remove library directory: %s", lib_path);
  }
  
  /* Remove library metadata from documents collection if exists */
  if (db_collection_exists(ctx->db, "documents")) {
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("library"));
    json_object_set(query, "name", json_create_string(library_name));
    
    json_value_t* results = db_query_documents(ctx->db, "documents", query);
    if (results) {
      json_value_t* documents = json_object_get(results, "documents");
      if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
        json_value_t* lib_doc = json_array_get(documents, 0);
        json_value_t* id_val = json_object_get(lib_doc, "uuid");
        if (id_val && id_val->type == JSON_STRING) {
          db_delete_document(ctx->db, "documents", id_val->value.string);
        }
      }
      json_free(results);
    }
    json_free(query);
  }
  
  /* Return success response */
  json_value_t* response = json_create_object();
  json_object_set(response, "success", json_create_boolean(1));
  json_object_set(response, "name", json_create_string(library_name));
  json_object_set(response, "collections_dropped", collections);
  json_object_set(response, "message", json_create_string("Library deleted successfully"));
  
  char* response_str = json_stringify(response);
  json_free(response);
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Get library information */
http_response_t* api_handle_get_library(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract library name from URL */
  const char* library_name = NULL;
  if (strncmp(request->path, "/api/libraries/", 15) == 0) {
    library_name = request->path + 15;
  }
  
  if (!library_name || strlen(library_name) == 0) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Library name required\"}", "application/json");
  }
  
  /* Check if this is a stats request */
  if (strstr(library_name, "/stats") != NULL) {
    return api_handle_get_library_stats(ctx, request);
  }
  
  /* Check if library exists */
  char test_path[1024];
  snprintf(test_path, sizeof(test_path), "%s/users", library_name);
  if (!db_collection_exists(ctx->db, test_path)) {
    return create_http_response(HTTP_NOT_FOUND,
                 "{\"error\":\"Library not found\"}", "application/json");
  }
  
  /* Create library info */
  json_value_t* lib_info = json_create_object();
  json_object_set(lib_info, "name", json_create_string(library_name));
  json_object_set(lib_info, "type", json_create_string("library"));
  
  /* Query collection metadata from unified documents to get collection list */
  json_value_t* collections = json_create_array();
  int total_documents = 0;
  
  if (db_collection_exists(ctx->db, "documents")) {
    /* Query for collections in this library */
    json_value_t* coll_query = json_create_object();
    json_object_set(coll_query, "type", json_create_string("collection"));
    json_object_set(coll_query, "library", json_create_string(library_name));
    
    json_value_t* coll_results = db_query_documents(ctx->db, "documents", coll_query);
    if (coll_results) {
      json_value_t* coll_docs = json_object_get(coll_results, "documents");
      if (coll_docs && coll_docs->type == JSON_ARRAY) {
        size_t num_collections = json_array_size(coll_docs);
        for (size_t i = 0; i < num_collections; i++) {
          json_value_t* coll_doc = json_array_get(coll_docs, i);
          json_value_t* coll_name_val = json_object_get(coll_doc, "name");
          if (coll_name_val && coll_name_val->type == JSON_STRING) {
            const char* coll_name = coll_name_val->value.string;
            
            json_value_t* coll_info = json_create_object();
            json_object_set(coll_info, "name", json_create_string(coll_name));
            
            /* Get document count */
            char collection_path[512];
            snprintf(collection_path, sizeof(collection_path), "%s/%s", library_name, coll_name);
            
            json_value_t* empty_query = json_create_object();
            json_value_t* results = db_query_documents(ctx->db, collection_path, empty_query);
            if (results) {
              json_value_t* documents = json_object_get(results, "documents");
              if (documents && documents->type == JSON_ARRAY) {
                int count = json_array_size(documents);
                json_object_set(coll_info, "document_count", json_create_number(count));
                total_documents += count;
              }
              json_free(results);
            }
            json_free(empty_query);
            
            /* Copy other metadata from collection document */
            json_value_t* is_system = json_object_get(coll_doc, "is_system");
            if (is_system) {
              json_object_set(coll_info, "is_system", json_clone(is_system));
            }
            
            json_array_append(collections, coll_info);
          }
        }
      }
      json_free(coll_results);
    }
    json_free(coll_query);
  }
  
  json_object_set(lib_info, "collections", collections);
  json_object_set(lib_info, "collection_count", json_create_number(json_array_size(collections)));
  json_object_set(lib_info, "document_count", json_create_number(total_documents));
  
  /* Get metadata from documents collection if exists */
  if (db_collection_exists(ctx->db, "documents")) {
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("library"));
    json_object_set(query, "name", json_create_string(library_name));
    
    json_value_t* results = db_query_documents(ctx->db, "documents", query);
    if (results) {
      json_value_t* documents = json_object_get(results, "documents");
      if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
        json_value_t* lib_doc = json_array_get(documents, 0);
        
        /* Copy metadata fields */
        json_value_t* created_at = json_object_get(lib_doc, "created_at");
        if (created_at) {
          json_object_set(lib_info, "created_at", json_clone(created_at));
        }
        json_value_t* updated_at = json_object_get(lib_doc, "updated_at");
        if (updated_at) {
          json_object_set(lib_info, "updated_at", json_clone(updated_at));
        }
        json_value_t* owner = json_object_get(lib_doc, "owner");
        if (owner) {
          json_object_set(lib_info, "owner", json_clone(owner));
        }
        json_value_t* display_name = json_object_get(lib_doc, "display_name");
        if (display_name) {
          json_object_set(lib_info, "display_name", json_clone(display_name));
        }
        json_value_t* description = json_object_get(lib_doc, "description");
        if (description) {
          json_object_set(lib_info, "description", json_clone(description));
        }
        json_value_t* settings = json_object_get(lib_doc, "settings");
        if (settings) {
          json_object_set(lib_info, "settings", json_clone(settings));
        }
      }
      json_free(results);
    }
    json_free(query);
  }
  
  char* response_str = json_stringify(lib_info);
  json_free(lib_info);
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Update library metadata */
http_response_t* api_handle_update_library(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract library name from URL */
  const char* library_name = NULL;
  if (strncmp(request->path, "/api/libraries/", 15) == 0) {
    library_name = request->path + 15;
  }
  
  if (!library_name || strlen(library_name) == 0) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Library name required\"}", "application/json");
  }
  
  /* Prevent updating system library */
  if (strcmp(library_name, "system") == 0) {
    return create_http_response(HTTP_FORBIDDEN,
                 "{\"error\":\"Cannot modify system library\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid JSON body\"}", "application/json");
  }
  
  /* Find library document */
  json_value_t* query = json_create_object();
  json_object_set(query, "type", json_create_string("library"));
  json_object_set(query, "name", json_create_string(library_name));
  
  json_value_t* results = db_query_documents(ctx->db, "documents", query);
  json_free(query);
  
  if (!results) {
    json_free(body);
    return create_http_response(HTTP_NOT_FOUND,
                 "{\"error\":\"Library not found\"}", "application/json");
  }
  
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0) {
    json_free(results);
    json_free(body);
    return create_http_response(HTTP_NOT_FOUND,
                 "{\"error\":\"Library not found\"}", "application/json");
  }
  
  /* Get library document */
  json_value_t* lib_doc = json_array_get(documents, 0);
  json_value_t* lib_id = json_object_get(lib_doc, "uuid");
  if (!lib_id || lib_id->type != JSON_STRING) {
    json_free(results);
    json_free(body);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Invalid library document\"}", "application/json");
  }
  
  /* Update allowed fields */
  json_value_t* update_doc = json_clone(lib_doc);
  
  /* Update display_name if provided */
  json_value_t* display_name = json_object_get(body, "display_name");
  if (display_name && display_name->type == JSON_STRING) {
    json_object_set(update_doc, "display_name", json_clone(display_name));
  }
  
  /* Update description if provided */
  json_value_t* description = json_object_get(body, "description");
  if (description && description->type == JSON_STRING) {
    json_object_set(update_doc, "description", json_clone(description));
  }
  
  /* Update settings if provided */
  json_value_t* settings = json_object_get(body, "settings");
  if (settings && settings->type == JSON_OBJECT) {
    json_object_set(update_doc, "settings", json_clone(settings));
  }
  
  /* Update timestamp */
  time_t now = time(NULL);
  char timestamp[64];
  struct tm* utc_tm = gmtime(&now);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
  json_object_set(update_doc, "updated_at", json_create_string(timestamp));
  
  /* Update in database */
  json_value_t* update_result = db_update_document(ctx->db, "documents", 
                                                  lib_id->value.string, update_doc);
  
  json_free(update_doc);
  json_free(results);
  json_free(body);
  
  if (!update_result) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to update library\"}", "application/json");
  }
  
  json_free(update_result);
  
  return create_http_response(HTTP_OK,
               "{\"success\":true,\"message\":\"Library updated successfully\"}", 
               "application/json");
}

/* Get library statistics */
http_response_t* api_handle_get_library_stats(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract library name from URL */
  const char* library_name = NULL;
  if (strncmp(request->path, "/api/libraries/", 15) == 0) {
    const char* path_part = request->path + 15;
    /* Find /stats suffix */
    char* stats_pos = strstr(path_part, "/stats");
    if (stats_pos) {
      size_t name_len = stats_pos - path_part;
      char* name_buf = malloc(name_len + 1);
      if (!name_buf) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Memory allocation failed\"}", "application/json");
      }
      strncpy(name_buf, path_part, name_len);
      name_buf[name_len] = '\0';
      library_name = name_buf;
    }
  }
  
  if (!library_name || strlen(library_name) == 0) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Library name required\"}", "application/json");
  }
  
  /* Create stats object */
  json_value_t* stats = json_create_object();
  json_object_set(stats, "library", json_create_string(library_name));
  
  /* Collection stats */
  json_value_t* collection_stats = json_create_object();
  int total_collections = 0;
  int total_documents = 0;
  int total_indexes = 0;
  
  /* Query collections in this library */
  if (db_collection_exists(ctx->db, "documents")) {
    json_value_t* coll_query = json_create_object();
    json_object_set(coll_query, "type", json_create_string("collection"));
    json_object_set(coll_query, "library", json_create_string(library_name));
    
    json_value_t* coll_results = db_query_documents(ctx->db, "documents", coll_query);
    if (coll_results) {
      json_value_t* coll_docs = json_object_get(coll_results, "documents");
      if (coll_docs && coll_docs->type == JSON_ARRAY) {
        total_collections = json_array_size(coll_docs);
        
        /* Count documents in each collection */
        for (size_t i = 0; i < (size_t)total_collections; i++) {
          json_value_t* coll_doc = json_array_get(coll_docs, i);
          json_value_t* coll_name_val = json_object_get(coll_doc, "name");
          if (coll_name_val && coll_name_val->type == JSON_STRING) {
            char collection_path[512];
            snprintf(collection_path, sizeof(collection_path), "%s/%s", 
                    library_name, coll_name_val->value.string);
            
            json_value_t* empty_query = json_create_object();
            json_value_t* results = db_query_documents(ctx->db, collection_path, empty_query);
            if (results) {
              json_value_t* documents = json_object_get(results, "documents");
              if (documents && documents->type == JSON_ARRAY) {
                total_documents += json_array_size(documents);
              }
              json_free(results);
            }
            json_free(empty_query);
          }
        }
      }
      json_free(coll_results);
    }
    json_free(coll_query);
  }
  
  json_object_set(collection_stats, "total", json_create_number(total_collections));
  json_object_set(collection_stats, "system", json_create_number(0)); /* TODO: count system collections */
  json_object_set(collection_stats, "user", json_create_number(total_collections));
  json_object_set(stats, "collections", collection_stats);
  
  /* Document stats */
  json_value_t* document_stats = json_create_object();
  json_object_set(document_stats, "total", json_create_number(total_documents));
  json_object_set(stats, "documents", document_stats);
  
  /* Index stats */
  json_value_t* index_stats = json_create_object();
  json_object_set(index_stats, "total", json_create_number(total_indexes));
  json_object_set(stats, "indexes", index_stats);
  
  /* Usage stats */
  json_value_t* usage_stats = json_create_object();
  json_object_set(usage_stats, "storage_bytes", json_create_number(0)); /* TODO: calculate actual storage */
  json_object_set(usage_stats, "api_calls_today", json_create_number(0)); /* TODO: track API calls */
  json_object_set(stats, "usage", usage_stats);
  
  /* Add timestamp */
  time_t now = time(NULL);
  char timestamp[64];
  struct tm* utc_tm = gmtime(&now);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
  json_object_set(stats, "generated_at", json_create_string(timestamp));
  
  char* response_str = json_stringify(stats);
  json_free(stats);
  
  if (library_name && strncmp(request->path, "/api/libraries/", 15) == 0) {
    free((void*)library_name);
  }
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Copy library structure */
http_response_t* api_handle_copy_library(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* This handler only processes /copy endpoints */
  if (!strstr(request->path, "/copy")) {
    return create_http_response(HTTP_NOT_FOUND,
                 "{\"error\":\"Not found\"}", "application/json");
  }
  
  /* Extract library name from URL */
  const char* source_library = NULL;
  if (strncmp(request->path, "/api/libraries/", 15) == 0) {
    const char* path_part = request->path + 15;
    /* Find /copy suffix */
    char* copy_pos = strstr(path_part, "/copy");
    if (copy_pos) {
      size_t name_len = copy_pos - path_part;
      char* name_buf = malloc(name_len + 1);
      if (!name_buf) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Memory allocation failed\"}", "application/json");
      }
      strncpy(name_buf, path_part, name_len);
      name_buf[name_len] = '\0';
      source_library = name_buf;
    }
  }
  
  if (!source_library || strlen(source_library) == 0) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Source library name required\"}", "application/json");
  }
  
  /* Parse request body */
  json_value_t* body = json_parse(request->body);
  if (!body || body->type != JSON_OBJECT) {
    if (source_library) free((void*)source_library);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid JSON body\"}", "application/json");
  }
  
  /* Get target library name */
  json_value_t* target_name_val = json_object_get(body, "name");
  if (!target_name_val || target_name_val->type != JSON_STRING) {
    json_free(body);
    if (source_library) free((void*)source_library);
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Target library name required\"}", "application/json");
  }
  
  const char* target_library = target_name_val->value.string;
  
  /* Check if target already exists */
  char test_path[1024];
  snprintf(test_path, sizeof(test_path), "%s/users", target_library);
  if (db_collection_exists(ctx->db, test_path)) {
    json_free(body);
    if (source_library) free((void*)source_library);
    return create_http_response(HTTP_CONFLICT,
                 "{\"error\":\"Target library already exists\"}", "application/json");
  }
  
  /* Get source library metadata */
  json_value_t* source_metadata = NULL;
  if (db_collection_exists(ctx->db, "documents")) {
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("library"));
    json_object_set(query, "name", json_create_string(source_library));
    
    json_value_t* results = db_query_documents(ctx->db, "documents", query);
    if (results) {
      json_value_t* documents = json_object_get(results, "documents");
      if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
        source_metadata = json_clone(json_array_get(documents, 0));
      }
      json_free(results);
    }
    json_free(query);
  }
  
  /* Create new library metadata */
  json_value_t* lib_doc = source_metadata ? source_metadata : json_create_object();
  json_object_set(lib_doc, "type", json_create_string("library"));
  json_object_set(lib_doc, "name", json_create_string(target_library));
  
  /* Update timestamps */
  time_t now = time(NULL);
  char timestamp[64];
  struct tm* utc_tm = gmtime(&now);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
  json_object_set(lib_doc, "created_at", json_create_string(timestamp));
  json_object_set(lib_doc, "updated_at", json_create_string(timestamp));
  json_object_set(lib_doc, "owner", json_create_string("admin"));
  
  /* Copy settings */
  json_value_t* copy_data = json_object_get(body, "copy_data");
  int should_copy_data = copy_data && copy_data->type == JSON_BOOLEAN && 
                         copy_data->value.boolean;
  
  /* Save library metadata */
  if (db_collection_exists(ctx->db, "documents")) {
    json_value_t* result = db_insert_document(ctx->db, "documents", lib_doc);
    if (result) {
      json_free(result);
    }
  }
  json_free(lib_doc);
  
  /* Copy collections structure */
  int collections_copied = 0;
  if (db_collection_exists(ctx->db, "documents")) {
    json_value_t* coll_query = json_create_object();
    json_object_set(coll_query, "type", json_create_string("collection"));
    json_object_set(coll_query, "library", json_create_string(source_library));
    
    json_value_t* coll_results = db_query_documents(ctx->db, "documents", coll_query);
    if (coll_results) {
      json_value_t* coll_docs = json_object_get(coll_results, "documents");
      if (coll_docs && coll_docs->type == JSON_ARRAY) {
        size_t num_collections = json_array_size(coll_docs);
        for (size_t i = 0; i < num_collections; i++) {
          json_value_t* coll_doc = json_array_get(coll_docs, i);
          json_value_t* coll_name_val = json_object_get(coll_doc, "name");
          if (coll_name_val && coll_name_val->type == JSON_STRING) {
            const char* coll_name = coll_name_val->value.string;
            
            /* Create collection in target library */
            char target_coll_path[512];
            snprintf(target_coll_path, sizeof(target_coll_path), "%s/%s", 
                    target_library, coll_name);
            
            if (db_create_collection(ctx->db, target_coll_path) == 0) {
              collections_copied++;
              
              /* Copy data if requested */
              if (should_copy_data) {
                char source_coll_path[512];
                snprintf(source_coll_path, sizeof(source_coll_path), "%s/%s", 
                        source_library, coll_name);
                
                json_value_t* empty_query = json_create_object();
                json_value_t* results = db_query_documents(ctx->db, source_coll_path, empty_query);
                if (results) {
                  json_value_t* documents = json_object_get(results, "documents");
                  if (documents && documents->type == JSON_ARRAY) {
                    size_t num_docs = json_array_size(documents);
                    for (size_t j = 0; j < num_docs; j++) {
                      json_value_t* doc = json_array_get(documents, j);
                      json_value_t* doc_copy = json_clone(doc);
                      /* Remove uuid to generate new one */
                      json_object_remove(doc_copy, "uuid");
                      db_insert_document(ctx->db, target_coll_path, doc_copy);
                      json_free(doc_copy);
                    }
                  }
                  json_free(results);
                }
                json_free(empty_query);
              }
            }
          }
        }
      }
      json_free(coll_results);
    }
    json_free(coll_query);
  }
  
  /* Return success response */
  json_value_t* response = json_create_object();
  json_object_set(response, "success", json_create_boolean(1));
  json_object_set(response, "source", json_create_string(source_library));
  json_object_set(response, "target", json_create_string(target_library));
  json_object_set(response, "collections_copied", json_create_number(collections_copied));
  json_object_set(response, "data_copied", json_create_boolean(should_copy_data));
  
  json_free(body);
  if (source_library) free((void*)source_library);
  
  char* response_str = json_stringify(response);
  json_free(response);
  
  return create_http_response(HTTP_CREATED, response_str, "application/json");
}

/* Get available library templates */
http_response_t* api_handle_get_library_templates(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request) {
    return create_http_response(HTTP_BAD_REQUEST,
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Query for library templates */
  json_value_t* query = json_create_object();
  json_object_set(query, "type", json_create_string("library_template"));
  
  json_value_t* results = db_query_documents(ctx->db, "documents", query);
  json_free(query);
  
  if (!results) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                 "{\"error\":\"Failed to query templates\"}", "application/json");
  }
  
  /* Build response */
  json_value_t* response = json_create_object();
  json_value_t* templates_array = json_create_array();
  
  json_value_t* documents = json_object_get(results, "documents");
  if (documents && documents->type == JSON_ARRAY) {
    size_t num_templates = json_array_size(documents);
    for (size_t i = 0; i < num_templates; i++) {
      json_value_t* template_doc = json_array_get(documents, i);
      
      /* Create simplified template info */
      json_value_t* template_info = json_create_object();
      
      /* Copy basic fields */
      json_value_t* name = json_object_get(template_doc, "name");
      if (name) json_object_set(template_info, "name", json_clone(name));
      
      json_value_t* display_name = json_object_get(template_doc, "display_name");
      if (display_name) json_object_set(template_info, "display_name", json_clone(display_name));
      
      json_value_t* description = json_object_get(template_doc, "description");
      if (description) json_object_set(template_info, "description", json_clone(description));
      
      /* Count collections */
      json_value_t* collections = json_object_get(template_doc, "collections");
      if (collections && collections->type == JSON_ARRAY) {
        json_object_set(template_info, "collection_count", 
                       json_create_number(json_array_size(collections)));
      }
      
      json_array_append(templates_array, template_info);
    }
  }
  
  json_object_set(response, "templates", templates_array);
  json_free(results);
  
  char* response_str = json_stringify(response);
  json_free(response);
  
  return create_http_response(HTTP_OK, response_str, "application/json");
}