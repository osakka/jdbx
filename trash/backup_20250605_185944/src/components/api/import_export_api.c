#include "api/api.h"
#include "utils/import_export.h"
#include "utils/json_helpers.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Handle data export requests
 * 
 * This endpoint allows exporting data from one or more collections as JSON
 * 
 * HTTP Method: GET
 * URL: /api/export
 * Query Parameters:
 *  - collection: (optional) Collection to export. If not specified, all collections are exported.
 *  - format: (optional) Export format. Currently only "json" is supported (default).
 *  - pretty: (optional) Whether to pretty-print the JSON output (default: true).
 * 
 * Response:
 *  - 200 OK: Export successful, response contains the exported data.
 *  - 400 Bad Request: Invalid parameters.
 *  - 404 Not Found: Collection not found.
 *  - 500 Internal Server Error: Export operation failed.
 */
http_response_t* api_handle_export(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db) {
    return http_response_error("Invalid API context", HTTP_INTERNAL_SERVER_ERROR);
  }
  
  /* Parse query parameters */
  char collection[256] = {0};
  char format[32] = "json";
  /* pretty parameter is not used currently, but parsing is left in for future use */
  
  if (request->query) {
    /* Extract collection parameter */
    const char* coll_param = strstr(request->query, "collection=");
    if (coll_param) {
      coll_param += 11; /* Skip "collection=" */
      const char* end = strchr(coll_param, '&');
      if (end) {
        size_t len = end - coll_param;
        if (len < sizeof(collection)) {
          strncpy(collection, coll_param, len);
          collection[len] = '\0';
        }
      } else {
        strncpy(collection, coll_param, sizeof(collection) - 1);
      }
    }
    
    /* Extract format parameter */
    const char* format_param = strstr(request->query, "format=");
    if (format_param) {
      format_param += 7; /* Skip "format=" */
      const char* end = strchr(format_param, '&');
      if (end) {
        size_t len = end - format_param;
        if (len < sizeof(format)) {
          strncpy(format, format_param, len);
          format[len] = '\0';
        }
      } else {
        strncpy(format, format_param, sizeof(format) - 1);
      }
    }
    
    /* Parsing for pretty parameter has been removed as it's not currently used
     * We will reimplement this when pretty-printing functionality is added
     */
  }
  
  /* Currently only JSON format is supported */
  if (strcmp(format, "json") != 0) {
    return http_response_error("Unsupported format. Only 'json' is currently supported",
                 HTTP_BAD_REQUEST);
  }
  
  /* Export data */
  json_value_t* exported_data = NULL;
  
  if (collection[0] != '\0') {
    /* Export a specific collection */
    if (!db_get_collection(ctx->db, collection)) {
      return http_response_error("Collection not found", HTTP_NOT_FOUND);
    }
    
    exported_data = json_export_collection(ctx->db, collection);
    if (!exported_data) {
      return http_response_error("Failed to export collection", HTTP_INTERNAL_SERVER_ERROR);
    }
  } else {
    /* Export all collections */
    exported_data = json_export_database(ctx->db);
    if (!exported_data) {
      return http_response_error("Failed to export database", HTTP_INTERNAL_SERVER_ERROR);
    }
  }
  
  /* Create response */
  http_response_t* response = http_response_json(exported_data, HTTP_OK);
  
  /* Set additional headers */
  char filename[300];
  if (collection[0] != '\0') {
    snprintf(filename, sizeof(filename), 
        "attachment; filename=\"%s_export_%ld.json\"", 
        collection, (long)time(NULL));
  } else {
    snprintf(filename, sizeof(filename), 
        "attachment; filename=\"database_export_%ld.json\"", 
        (long)time(NULL));
  }
  
  char content_disp[512];
  snprintf(content_disp, sizeof(content_disp), 
      "Content-Disposition: %s", filename);
  
  add_response_header(response, content_disp);
  
  /* Free the exported data - http_response_json has made a copy */
  json_free(exported_data);
  
  return response;
}

/**
 * Handle data import requests
 * 
 * This endpoint allows importing data into one or more collections from JSON
 * 
 * HTTP Method: POST
 * URL: /api/import
 * Body: JSON data to import
 * Query Parameters:
 *  - collection: (optional) Collection to import into. If not specified, imports all collections.
 *  - mode: (optional) Import mode: "merge" (default) or "replace".
 * 
 * Response:
 *  - 200 OK: Import successful, response contains summary info.
 *  - 400 Bad Request: Invalid parameters or import data.
 *  - 500 Internal Server Error: Import operation failed.
 */
http_response_t* api_handle_import(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !ctx->db || !request || !request->body) {
    return http_response_error("Invalid request", HTTP_BAD_REQUEST);
  }
  
  /* Parse query parameters */
  char collection[256] = {0};
  char mode[32] = "merge";
  
  if (request->query) {
    /* Extract collection parameter */
    const char* coll_param = strstr(request->query, "collection=");
    if (coll_param) {
      coll_param += 11; /* Skip "collection=" */
      const char* end = strchr(coll_param, '&');
      if (end) {
        size_t len = end - coll_param;
        if (len < sizeof(collection)) {
          strncpy(collection, coll_param, len);
          collection[len] = '\0';
        }
      } else {
        strncpy(collection, coll_param, sizeof(collection) - 1);
      }
    }
    
    /* Extract mode parameter */
    const char* mode_param = strstr(request->query, "mode=");
    if (mode_param) {
      mode_param += 5; /* Skip "mode=" */
      const char* end = strchr(mode_param, '&');
      if (end) {
        size_t len = end - mode_param;
        if (len < sizeof(mode)) {
          strncpy(mode, mode_param, len);
          mode[len] = '\0';
        }
      } else {
        strncpy(mode, mode_param, sizeof(mode) - 1);
      }
    }
  }
  
  /* Validate mode */
  if (strcmp(mode, "merge") != 0 && strcmp(mode, "replace") != 0) {
    return http_response_error("Unsupported mode. Use 'merge' or 'replace'",
                 HTTP_BAD_REQUEST);
  }
  
  /* Parse JSON data */
  json_value_t* import_data = json_parse(request->body);
  if (!import_data) {
    return http_response_error("Invalid JSON data", HTTP_BAD_REQUEST);
  }
  
  /* Import data */
  int replace_mode = (strcmp(mode, "replace") == 0);
  int result;
  int docs_imported = 0;
  
  if (collection[0] != '\0') {
    /* Import into specific collection */
    result = json_import_collection(ctx->db, collection, import_data, replace_mode, &docs_imported);
  } else {
    /* Import all collections */
    result = json_import_database(ctx->db, import_data, replace_mode, &docs_imported);
  }
  
  /* Free the import data */
  json_free(import_data);
  
  if (!result) {
    return http_response_error("Import failed", HTTP_INTERNAL_SERVER_ERROR);
  }
  
  /* Create success response with import summary */
  json_value_t* summary = json_create_object();
  if (!summary) {
    return http_response_error("Failed to create import summary", HTTP_INTERNAL_SERVER_ERROR);
  }
  
  json_object_set(summary, "success", json_create_boolean(1));
  json_object_set(summary, "documents_imported", json_create_integer(docs_imported));
  json_object_set(summary, "mode", json_create_string(mode));
  
  if (collection[0] != '\0') {
    json_object_set(summary, "collection", json_create_string(collection));
  } else {
    json_object_set(summary, "collection", json_create_string("all"));
  }
  
  return http_response_json(summary, HTTP_OK);
}