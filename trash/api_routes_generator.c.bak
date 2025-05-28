#include <stdio.h>
#include <string.h>
#include "core/server.h"
#include "api/api.h"

/* Forward declaration of routes array from api.c */
extern api_route_t routes[];

/* Generate JSON representation of all API routes */
const char* generate_api_routes_json(api_context_t* ctx) {
    static char json_buffer[64000];
    int offset = 0;
    
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "{\n  \"openapi\": \"3.0.0\",\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "  \"info\": {\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "    \"title\": \"JSONdb API\",\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "    \"version\": \"1.0.7\",\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "    \"description\": \"RESTful API for JSONdb - A lightweight JSON document database\"\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "  },\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "  \"servers\": [\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "    {\"url\": \"/\", \"description\": \"Current server\"}\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "  ],\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "  \"paths\": {\n");
    
    /* Use routes from context */
    const char* current_path = NULL;
    
    for (int i = 0; i < ctx->num_routes; i++) {
        api_route_t* route = &ctx->routes[i];
        
        /* Check if this is a new path */
        if (!current_path || strcmp(route->path, current_path) != 0) {
            /* Close previous path if exists */
            if (current_path) {
                offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "\n    }");
                if (i < ctx->num_routes) {
                    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, ",");
                }
                offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "\n");
            }
            
            /* Start new path */
            offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "    \"%s\": {\n", route->path);
            current_path = route->path;
        } else {
            /* Multiple methods for same path */
            offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, ",\n");
        }
        
        /* Add method */
        const char* method_str = "get";
        switch (route->method) {
            case HTTP_POST: method_str = "post"; break;
            case HTTP_PUT: method_str = "put"; break;
            case HTTP_DELETE: method_str = "delete"; break;
            default: method_str = "get"; break;
        }
        
        /* Generate operation ID from path and method */
        char operation_id[256];
        snprintf(operation_id, sizeof(operation_id), "%s_%s", method_str, route->path);
        
        /* Clean up operation ID */
        for (char* p = operation_id; *p; p++) {
            if (*p == '/' || *p == '-' || *p == ':') *p = '_';
        }
        
        /* Add operation */
        offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "      \"%s\": {\n", method_str);
        offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "        \"operationId\": \"%s\",\n", operation_id);
        offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "        \"summary\": \"%s %s\",\n", method_str, route->path);
        
        /* Add tags based on path */
        const char* tag = "General";
        if (strstr(route->path, "/collections")) tag = "Collections";
        else if (strstr(route->path, "/users") || strstr(route->path, "/roles")) tag = "RBAC";
        else if (strstr(route->path, "/auth")) tag = "Authentication";
        else if (strstr(route->path, "/metrics")) tag = "Metrics";
        else if (strstr(route->path, "/backup")) tag = "Backup";
        else if (strstr(route->path, "/schemas")) tag = "Schemas";
        else if (strstr(route->path, "/indexes")) tag = "Indexes";
        else if (strstr(route->path, "/js")) tag = "JavaScript";
        else if (strstr(route->path, "/cache")) tag = "Cache";
        else if (strstr(route->path, "/transaction")) tag = "Transactions";
        else if (strstr(route->path, "/visualization")) tag = "Visualization";
        else if (strstr(route->path, "/health")) tag = "Health";
        else if (strstr(route->path, "/admin")) tag = "Admin";
        
        offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "        \"tags\": [\"%s\"],\n", tag);
        
        /* Add security if required */
        if (route->requires_auth) {
            offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "        \"security\": [{\"bearerAuth\": []}],\n");
        }
        
        /* Add responses */
        offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "        \"responses\": {\n");
        offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "          \"200\": {\"description\": \"Success\"},\n");
        offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "          \"401\": {\"description\": \"Unauthorized\"},\n");
        offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "          \"404\": {\"description\": \"Not Found\"}\n");
        offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "        }\n");
        offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "      }");
    }
    
    /* Close the last path */
    if (current_path) {
        offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "\n    }\n");
    }
    
    /* Add components */
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "  },\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "  \"components\": {\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "    \"securitySchemes\": {\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "      \"bearerAuth\": {\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "        \"type\": \"http\",\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "        \"scheme\": \"bearer\"\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "      }\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "    }\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "  }\n");
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset, "}\n");
    
    return json_buffer;
}

/* API handler to return routes as OpenAPI JSON */
http_response_t* api_handle_openapi_spec(api_context_t* ctx, http_request_t* request) {
    (void)request;
    
    const char* openapi_json = generate_api_routes_json(ctx);
    return create_http_response(HTTP_OK, openapi_json, "application/json");
}