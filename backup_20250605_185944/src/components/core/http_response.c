#include "core/server.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* HTTP method string to enum conversion */
http_method_t parse_http_method(const char* method_str) {
  if (strcmp(method_str, "GET") == 0) return HTTP_GET;
  if (strcmp(method_str, "POST") == 0) return HTTP_POST;
  if (strcmp(method_str, "PUT") == 0) return HTTP_PUT;
  if (strcmp(method_str, "DELETE") == 0) return HTTP_DELETE;
  if (strcmp(method_str, "PATCH") == 0) return HTTP_PATCH;
  if (strcmp(method_str, "HEAD") == 0) return HTTP_HEAD;
  if (strcmp(method_str, "OPTIONS") == 0) return HTTP_OPTIONS;
  return HTTP_UNKNOWN;
}

/* HTTP status code to string conversion */
const char* http_status_string(http_status_t status) {
  switch (status) {
    case HTTP_OK:
      return "200 OK";
    case HTTP_CREATED:
      return "201 Created";
    case HTTP_NO_CONTENT:
      return "204 No Content";
    case HTTP_FOUND:
      return "302 Found";
    case HTTP_BAD_REQUEST:
      return "400 Bad Request";
    case HTTP_UNAUTHORIZED:
      return "401 Unauthorized";
    case HTTP_FORBIDDEN:
      return "403 Forbidden";
    case HTTP_NOT_FOUND:
      return "404 Not Found";
    case HTTP_METHOD_NOT_ALLOWED:
      return "405 Method Not Allowed";
    case HTTP_INTERNAL_SERVER_ERROR:
      return "500 Internal Server Error";
    default:
      return "500 Internal Server Error";
  }
}

/* Create HTTP response */
http_response_t* create_http_response(http_status_t status, const char* body, const char* content_type) {
  http_response_t* response = (http_response_t*)malloc(sizeof(http_response_t));
  if (!response) {
    return NULL;
  }
  
  response->status = status;
  response->body = body ? strdup(body) : NULL;
  response->content_type = content_type ? strdup(content_type) : strdup("application/json");
  response->content_length = response->body ? strlen(response->body) : 0;
  response->headers = NULL;
  response->num_headers = 0;
  
  return response;
}

/* Create HTTP response with binary data */
http_response_t* create_http_response_binary(http_status_t status, const char* body, size_t body_size, const char* content_type) {
  http_response_t* response = (http_response_t*)malloc(sizeof(http_response_t));
  if (!response) {
    return NULL;
  }
  
  response->status = status;
  
  /* For binary data, allocate and copy the exact bytes */
  if (body && body_size > 0) {
    response->body = (char*)malloc(body_size);
    if (response->body) {
      memcpy(response->body, body, body_size);
      response->content_length = body_size;
    } else {
      free(response);
      return NULL;
    }
  } else {
    response->body = NULL;
    response->content_length = 0;
  }
  
  response->content_type = content_type ? strdup(content_type) : strdup("application/octet-stream");
  response->headers = NULL;
  response->num_headers = 0;
  
  return response;
}

/* Create an error response */
http_response_t* http_response_error(const char* message, int status_code) {
  char buffer[512];
  snprintf(buffer, sizeof(buffer), "{\"error\":\"%s\"}", message);
  return create_http_response((http_status_t)status_code, buffer, "application/json");
}

/* Create a JSON response */
http_response_t* http_response_json(json_value_t* json, int status_code) {
  if (!json) {
    return http_response_error("Invalid JSON data", 500);
  }
  
  char* json_str = json_stringify(json);
  if (!json_str) {
    return http_response_error("Failed to stringify JSON", 500);
  }
  
  http_response_t* response = create_http_response((http_status_t)status_code, json_str, "application/json");
  buffer_pool_free_safe(json_str);
  
  return response;
}

/* Serialize HTTP response to string */
char* serialize_http_response(http_response_t* response) {
  if (!response) {
    return NULL;
  }

  /* Calculate response size with a larger base size and safety margin */
  int response_size = 1024; /* Increased base size for headers */
  if (response->body) {
    response_size += response->content_length;
  }

  /* Add space for additional headers with larger allocation per header */
  response_size += response->num_headers * 256;

  /* Add safety margin to prevent buffer overflows */
  response_size += 1024;

  /* Allocate response string */
  char* response_str = (char*)malloc(response_size);
  if (!response_str) {
    return NULL;
  }
  
  /* Create response */
  size_t written = 0;
  size_t remaining = response_size;

  /* Status line */
  int n = snprintf(response_str + written, remaining, "HTTP/1.1 %s\r\n", http_status_string(response->status));
  if (n < 0 || (size_t)n >= remaining) {
    free(response_str);
    return NULL;
  }
  written += n;
  remaining -= n;

  /* Headers */
  n = snprintf(response_str + written, remaining, "Content-Type: %s\r\n", response->content_type);
  if (n < 0 || (size_t)n >= remaining) {
    free(response_str);
    return NULL;
  }
  written += n;
  remaining -= n;

  n = snprintf(response_str + written, remaining, "Content-Length: %zu\r\n", response->content_length);
  if (n < 0 || (size_t)n >= remaining) {
    free(response_str);
    return NULL;
  }
  written += n;
  remaining -= n;

  n = snprintf(response_str + written, remaining, "Connection: close\r\n");
  if (n < 0 || (size_t)n >= remaining) {
    free(response_str);
    return NULL;
  }
  written += n;
  remaining -= n;

  /* Additional headers */
  for (size_t i = 0; i < response->num_headers; i++) {
    if (response->headers[i]) {
      n = snprintf(response_str + written, remaining, "%s\r\n", response->headers[i]);
      if (n < 0 || (size_t)n >= remaining) {
        free(response_str);
        return NULL;
      }
      written += n;
      remaining -= n;
    }
  }

  /* End of headers */
  n = snprintf(response_str + written, remaining, "\r\n");
  if (n < 0 || (size_t)n >= remaining) {
    free(response_str);
    return NULL;
  }
  written += n;
  remaining -= n;

  /* Body */
  if (response->body && response->content_length > 0) {
    if (response->content_length + 1 > remaining) { /* +1 for null terminator */
      free(response_str);
      return NULL;
    }
    memcpy(response_str + written, response->body, response->content_length);
    written += response->content_length;
  }

  /* Ensure null-termination */
  if (remaining > 0) {
    response_str[written] = '\0';
  } else {
    free(response_str);
    return NULL;
  }
  
  return response_str;
}

/* Serialize HTTP response to string with length */
char* serialize_http_response_with_length(http_response_t* response, size_t* length) {
  if (!response || !length) {
    return NULL;
  }

  /* Calculate response size with a larger base size and safety margin */
  int response_size = 1024; /* Increased base size for headers */
  if (response->body) {
    response_size += response->content_length;
  }

  /* Add space for additional headers with larger allocation per header */
  response_size += response->num_headers * 256;

  /* Add safety margin to prevent buffer overflows */
  response_size += 1024;

  /* Allocate response string */
  char* response_str = (char*)malloc(response_size);
  if (!response_str) {
    return NULL;
  }
  
  /* Create response */
  size_t written = 0;
  size_t remaining = response_size;

  /* Status line */
  int n = snprintf(response_str + written, remaining, "HTTP/1.1 %s\r\n", http_status_string(response->status));
  if (n < 0 || (size_t)n >= remaining) {
    free(response_str);
    return NULL;
  }
  written += n;
  remaining -= n;

  /* Headers */
  n = snprintf(response_str + written, remaining, "Content-Type: %s\r\n", response->content_type);
  if (n < 0 || (size_t)n >= remaining) {
    free(response_str);
    return NULL;
  }
  written += n;
  remaining -= n;

  n = snprintf(response_str + written, remaining, "Content-Length: %zu\r\n", response->content_length);
  if (n < 0 || (size_t)n >= remaining) {
    free(response_str);
    return NULL;
  }
  written += n;
  remaining -= n;

  n = snprintf(response_str + written, remaining, "Connection: close\r\n");
  if (n < 0 || (size_t)n >= remaining) {
    free(response_str);
    return NULL;
  }
  written += n;
  remaining -= n;

  /* Additional headers */
  for (size_t i = 0; i < response->num_headers; i++) {
    if (response->headers[i]) {
      n = snprintf(response_str + written, remaining, "%s\r\n", response->headers[i]);
      if (n < 0 || (size_t)n >= remaining) {
        free(response_str);
        return NULL;
      }
      written += n;
      remaining -= n;
    }
  }

  /* End of headers */
  n = snprintf(response_str + written, remaining, "\r\n");
  if (n < 0 || (size_t)n >= remaining) {
    free(response_str);
    return NULL;
  }
  written += n;
  remaining -= n;

  /* Body */
  if (response->body && response->content_length > 0) {
    if (response->content_length > remaining) {
      free(response_str);
      return NULL;
    }
    memcpy(response_str + written, response->body, response->content_length);
    written += response->content_length;
  }

  /* Return the actual length written */
  *length = written;
  
  return response_str;
}

/* Add header to HTTP response */
int add_response_header(http_response_t* response, const char* header) {
  if (!response || !header) {
    return 0;
  }
  
  /* Allocate or reallocate headers array */
  char** new_headers = (char**)realloc(response->headers, 
                    (response->num_headers + 1) * sizeof(char*));
  if (!new_headers) {
    return 0;
  }
  
  response->headers = new_headers;
  response->headers[response->num_headers] = strdup(header);
  response->num_headers++;
  
  return 1;
}

/* Free HTTP response */
void free_http_response(http_response_t* response) {
  if (response) {
    if (response->body) buffer_pool_free_safe(response->body);
    if (response->content_type) free(response->content_type);
    
    /* Free headers */
    for (size_t i = 0; i < response->num_headers; i++) {
      if (response->headers[i]) {
        free(response->headers[i]);
      }
    }
    
    if (response->headers) {
      free(response->headers);
    }
    
    free(response);
  }
}

/* Create authentication response with cookie */
http_response_t* create_auth_response(http_response_t* response, const char* token) {
  if (!response || !token) {
    return response;
  }

  /* Create Set-Cookie header with SameSite attribute */
  char cookie_header[512];
  snprintf(cookie_header, sizeof(cookie_header),
       "Set-Cookie: %s=%s; Path=/; Max-Age=%d; HttpOnly; SameSite=Lax",
       ADMIN_AUTH_COOKIE_NAME, token, ADMIN_AUTH_COOKIE_TTL);

  /* Add header to response */
  if (!add_response_header(response, cookie_header)) {
    printf("Failed to add cookie header to response\n");
  }

  return response;
}