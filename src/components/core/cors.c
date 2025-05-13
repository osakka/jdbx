#include "core/server.h"
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
                free(cors->allowed_origins[i]);
                cors->allowed_origins[i] = NULL;
            }
        }
        free(cors->allowed_origins);
        cors->allowed_origins = NULL;
    }
    cors->allowed_origins_count = 0;

    /* Free allowed methods */
    if (cors->allowed_methods) {
        for (int i = 0; i < cors->allowed_methods_count; i++) {
            if (cors->allowed_methods[i]) {
                free(cors->allowed_methods[i]);
                cors->allowed_methods[i] = NULL;
            }
        }
        free(cors->allowed_methods);
        cors->allowed_methods = NULL;
    }
    cors->allowed_methods_count = 0;

    /* Free allowed headers */
    if (cors->allowed_headers) {
        for (int i = 0; i < cors->allowed_headers_count; i++) {
            if (cors->allowed_headers[i]) {
                free(cors->allowed_headers[i]);
                cors->allowed_headers[i] = NULL;
            }
        }
        free(cors->allowed_headers);
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
    char** new_origins = (char**)realloc(cors->allowed_origins,
                                        (cors->allowed_origins_count + 1) * sizeof(char*));
    if (!new_origins) {
        return 0;
    }

    /* Add origin to array */
    cors->allowed_origins = new_origins;
    cors->allowed_origins[cors->allowed_origins_count] = strdup(origin);
    cors->allowed_origins_count++;

    return 1;
}

/* Add allowed method to CORS configuration */
int add_cors_allowed_method(cors_config_t* cors, const char* method) {
    if (!cors || !method) {
        return 0;
    }

    /* Allocate or reallocate array */
    char** new_methods = (char**)realloc(cors->allowed_methods,
                                        (cors->allowed_methods_count + 1) * sizeof(char*));
    if (!new_methods) {
        return 0;
    }

    /* Add method to array */
    cors->allowed_methods = new_methods;
    cors->allowed_methods[cors->allowed_methods_count] = strdup(method);
    cors->allowed_methods_count++;

    return 1;
}

/* Add allowed header to CORS configuration */
int add_cors_allowed_header(cors_config_t* cors, const char* header) {
    if (!cors || !header) {
        return 0;
    }

    /* Allocate or reallocate array */
    char** new_headers = (char**)realloc(cors->allowed_headers,
                                        (cors->allowed_headers_count + 1) * sizeof(char*));
    if (!new_headers) {
        return 0;
    }

    /* Add header to array */
    cors->allowed_headers = new_headers;
    cors->allowed_headers[cors->allowed_headers_count] = strdup(header);
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
    (void)cors; /* Avoid unused parameter warning */
    (void)origin; /* Avoid unused parameter warning */
    /* Make sure we have a valid response */
    if (!response) {
        return NULL;
    }

    /* Always add CORS headers for simplicity */
    add_response_header(response, "Access-Control-Allow-Origin: *");
    add_response_header(response, "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS");
    add_response_header(response, "Access-Control-Allow-Headers: Content-Type, Authorization");
    add_response_header(response, "Access-Control-Allow-Credentials: true");
    add_response_header(response, "Access-Control-Max-Age: 86400");

    return response;

    /* The code below is unreachable due to the return statement above */
    /* It is kept as a reference for more complex CORS implementation */
    /* but should be removed or properly enabled in the future */

    /*
    // Set Access-Control-Allow-Methods header
    if (cors && cors->allowed_methods_count > 0) {
        char methods[512] = "Access-Control-Allow-Methods: ";
        for (int i = 0; i < cors->allowed_methods_count; i++) {
            strcat(methods, cors->allowed_methods[i]);
            if (i < cors->allowed_methods_count - 1) {
                strcat(methods, ", ");
            }
        }
        add_response_header(response, methods);
    }

    // Set Access-Control-Allow-Headers header
    if (cors && cors->allowed_headers_count > 0) {
        char headers[512] = "Access-Control-Allow-Headers: ";
        for (int i = 0; i < cors->allowed_headers_count; i++) {
            strcat(headers, cors->allowed_headers[i]);
            if (i < cors->allowed_headers_count - 1) {
                strcat(headers, ", ");
            }
        }
        add_response_header(response, headers);
    }

    // Set Access-Control-Max-Age header
    if (cors) {
        char max_age[64];
        sprintf(max_age, "Access-Control-Max-Age: %d", cors->max_age);
        add_response_header(response, max_age);
    }
    */

    return response;
}