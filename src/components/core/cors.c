#include "core/server.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Initialize CORS configuration with defaults */
void init_cors_config(cors_config_t* cors) {
  if (!cors) {
    return;
  }
  
  /* Enable CORS by default */
  cors->enabled = 1;
  
  /* Initialize arrays */
  cors->allowed_origins = NULL;
  cors->allowed_origins_count = 0;

  cors->allowed_methods = NULL;
  cors->allowed_methods_count = 0;

  cors->allowed_headers = NULL;
  cors->allowed_headers_count = 0;
  
  /* Allow credentials by default */
  cors->allow_credentials = 1;
  
  /* Set max age to 86400 seconds (24 hours) */
  cors->max_age = 86400;
  
  /* Add default allowed methods */
  add_cors_allowed_method(cors, "GET");
  add_cors_allowed_method(cors, "POST");
  add_cors_allowed_method(cors, "PUT");
  add_cors_allowed_method(cors, "DELETE");
  add_cors_allowed_method(cors, "PATCH");
  add_cors_allowed_method(cors, "OPTIONS");
  
  /* Add default allowed headers */
  add_cors_allowed_header(cors, "Content-Type");
  add_cors_allowed_header(cors, "Authorization");
  add_cors_allowed_header(cors, "X-Requested-With");
  add_cors_allowed_header(cors, "Accept");
  
  /* Add default allowed origin (allow all) */
  add_cors_allowed_origin(cors, "*");
}

/* Free CORS configuration */
void free_cors_config(cors_config_t* cors) {
  if (!cors) {
    return;
  }

  /* Free allowed origins */
  if (cors->allowed_origins) {
    for (int i = 0; i < cors->allowed_origins_count; i++) {
      if (cors->allowed_origins[i]) {
        BUFFER_FREE(cors->allowed_origins[i]);
        cors->allowed_origins[i] = NULL;
      }
    }
    BUFFER_FREE(cors->allowed_origins);
    cors->allowed_origins = NULL;
  }
  cors->allowed_origins_count = 0;

  /* Free allowed methods */
  if (cors->allowed_methods) {
    for (int i = 0; i < cors->allowed_methods_count; i++) {
      if (cors->allowed_methods[i]) {
        BUFFER_FREE(cors->allowed_methods[i]);
        cors->allowed_methods[i] = NULL;
      }
    }
    BUFFER_FREE(cors->allowed_methods);
    cors->allowed_methods = NULL;
  }
  cors->allowed_methods_count = 0;

  /* Free allowed headers */
  if (cors->allowed_headers) {
    for (int i = 0; i < cors->allowed_headers_count; i++) {
      if (cors->allowed_headers[i]) {
        BUFFER_FREE(cors->allowed_headers[i]);
        cors->allowed_headers[i] = NULL;
      }
    }
    BUFFER_FREE(cors->allowed_headers);
    cors->allowed_headers = NULL;
  }
  cors->allowed_headers_count = 0;

  /* Reset all other fields to default values */
  cors->enabled = 0;
  cors->allow_credentials = 0;
  cors->max_age = 86400;
}

/* Add allowed origin to CORS configuration */
int add_cors_allowed_origin(cors_config_t* cors, const char* origin) {
  if (!cors || !origin) {
    return 0;
  }

  /* Allocate or reallocate array */
  char** new_origins = (char**)BUFFER_REALLOC(cors->allowed_origins,
                    (cors->allowed_origins_count + 1) * sizeof(char*));
  if (!new_origins) {
    return 0;
  }

  /* Add origin to array */
  cors->allowed_origins = new_origins;
  cors->allowed_origins[cors->allowed_origins_count] = BUFFER_STRDUP(origin);
  cors->allowed_origins_count++;

  return 1;
}

/* Add allowed method to CORS configuration */
int add_cors_allowed_method(cors_config_t* cors, const char* method) {
  if (!cors || !method) {
    return 0;
  }

  /* Allocate or reallocate array */
  char** new_methods = (char**)BUFFER_REALLOC(cors->allowed_methods,
                    (cors->allowed_methods_count + 1) * sizeof(char*));
  if (!new_methods) {
    return 0;
  }

  /* Add method to array */
  cors->allowed_methods = new_methods;
  cors->allowed_methods[cors->allowed_methods_count] = BUFFER_STRDUP(method);
  cors->allowed_methods_count++;

  return 1;
}

/* Add allowed header to CORS configuration */
int add_cors_allowed_header(cors_config_t* cors, const char* header) {
  if (!cors || !header) {
    return 0;
  }

  /* Allocate or reallocate array */
  char** new_headers = (char**)BUFFER_REALLOC(cors->allowed_headers,
                    (cors->allowed_headers_count + 1) * sizeof(char*));
  if (!new_headers) {
    return 0;
  }

  /* Add header to array */
  cors->allowed_headers = new_headers;
  cors->allowed_headers[cors->allowed_headers_count] = BUFFER_STRDUP(header);
  cors->allowed_headers_count++;

  return 1;
}

/* Check if origin is allowed */
int is_cors_allowed_origin(cors_config_t* cors, const char* origin) {
  if (!cors || !origin) {
    return 0;
  }
  
  /* Check if CORS is enabled */
  if (!cors->enabled) {
    return 0;
  }
  
  /* Check if all origins are allowed */
  for (int i = 0; i < cors->allowed_origins_count; i++) {
    if (strcmp(cors->allowed_origins[i], "*") == 0) {
      return 1;
    }
  }

  /* Check if origin is in allowed list */
  for (int i = 0; i < cors->allowed_origins_count; i++) {
    if (strcmp(cors->allowed_origins[i], origin) == 0) {
      return 1;
    }
  }
  
  return 0;
}

/* Apply CORS headers to response */
http_response_t* apply_cors_headers(http_response_t* response,
                  cors_config_t* cors,
                  const char* origin) {
  /* Make sure we have a valid response */
  if (!response) {
    return NULL;
  }

  printf("CORS: Applying CORS headers, origin=%s\n", origin ? origin : "NULL");
  
  if (!cors || !cors->enabled) {
    printf("CORS: No config or disabled, using permissive defaults\n");
    /* If CORS is not configured, add permissive defaults */
    add_response_header(response, "Access-Control-Allow-Origin: *");
    add_response_header(response, "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS");
    add_response_header(response, "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With, Accept");
    add_response_header(response, "Access-Control-Max-Age: 86400");
    return response;
  }

  /* Set Access-Control-Allow-Origin header */
  /* When using credentials, we can't use wildcard origin - must specify exact origin */
  if (cors->allow_credentials && origin) {
    /* Check if this specific origin is allowed */
    if (is_cors_allowed_origin(cors, origin) || is_cors_allowed_origin(cors, "*")) {
      char origin_header[512];
      snprintf(origin_header, sizeof(origin_header), "Access-Control-Allow-Origin: %s", origin);
      add_response_header(response, origin_header);
      add_response_header(response, "Vary: Origin"); /* Important with dynamic origin */
    } 
  } else {
    /* If no credentials required or no origin in request, 
      use wildcard if allowed or specific origin */
    int found_wildcard = 0;
    
    /* Check for wildcard in allowed origins */
    for (int i = 0; i < cors->allowed_origins_count; i++) {
      if (strcmp(cors->allowed_origins[i], "*") == 0) {
        found_wildcard = 1;
        break;
      }
    }
    
    if (found_wildcard) {
      add_response_header(response, "Access-Control-Allow-Origin: *");
    } else if (origin && is_cors_allowed_origin(cors, origin)) {
      char origin_header[512];
      snprintf(origin_header, sizeof(origin_header), "Access-Control-Allow-Origin: %s", origin);
      add_response_header(response, origin_header);
      add_response_header(response, "Vary: Origin");
    }
  }
  
  /* Set Access-Control-Allow-Credentials header if needed */
  if (cors->allow_credentials) {
    add_response_header(response, "Access-Control-Allow-Credentials: true");
  }

  /* Set Access-Control-Allow-Methods header */
  if (cors->allowed_methods_count > 0) {
    /* Calculate required size */
    size_t header_size = strlen("Access-Control-Allow-Methods: ") + 1;
    for (int i = 0; i < cors->allowed_methods_count; i++) {
      header_size += strlen(cors->allowed_methods[i]);
      if (i < cors->allowed_methods_count - 1) {
        header_size += 2; /* ", " */
      }
    }
    
    /* Allocate buffer */
    char* methods = (char*)BUFFER_ALLOC(header_size);
    if (!methods) {
      LOG_ERROR("Failed to allocate memory for CORS methods header");
      return response;
    }
    
    /* Build header */
    strcpy(methods, "Access-Control-Allow-Methods: ");
    size_t offset = strlen(methods);
    
    for (int i = 0; i < cors->allowed_methods_count; i++) {
      size_t method_len = strlen(cors->allowed_methods[i]);
      memcpy(methods + offset, cors->allowed_methods[i], method_len);
      offset += method_len;
      
      if (i < cors->allowed_methods_count - 1) {
        memcpy(methods + offset, ", ", 2);
        offset += 2;
      }
    }
    methods[offset] = '\0';
    
    add_response_header(response, methods);
    BUFFER_FREE(methods);
  }

  /* Set Access-Control-Allow-Headers header */
  if (cors->allowed_headers_count > 0) {
    /* Calculate required size */
    size_t header_size = strlen("Access-Control-Allow-Headers: ") + 1;
    for (int i = 0; i < cors->allowed_headers_count; i++) {
      header_size += strlen(cors->allowed_headers[i]);
      if (i < cors->allowed_headers_count - 1) {
        header_size += 2; /* ", " */
      }
    }
    
    /* Allocate buffer */
    char* headers = (char*)BUFFER_ALLOC(header_size);
    if (!headers) {
      LOG_ERROR("Failed to allocate memory for CORS headers header");
      return response;
    }
    
    /* Build header */
    strcpy(headers, "Access-Control-Allow-Headers: ");
    size_t offset = strlen(headers);
    
    for (int i = 0; i < cors->allowed_headers_count; i++) {
      size_t header_len = strlen(cors->allowed_headers[i]);
      memcpy(headers + offset, cors->allowed_headers[i], header_len);
      offset += header_len;
      
      if (i < cors->allowed_headers_count - 1) {
        memcpy(headers + offset, ", ", 2);
        offset += 2;
      }
    }
    headers[offset] = '\0';
    
    add_response_header(response, headers);
    BUFFER_FREE(headers);
  }

  /* Set Access-Control-Max-Age header */
  char max_age[64];
  snprintf(max_age, sizeof(max_age), "Access-Control-Max-Age: %d", cors->max_age);
  add_response_header(response, max_age);

  return response;
}