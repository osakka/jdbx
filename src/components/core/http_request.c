#include "core/server.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Parse cookies from Cookie header */
void parse_cookies(http_request_t* request) {
  if (!request || !request->cookie_header) {
    return;
  }
  
  char* cookie_str = BUFFER_STRDUP(request->cookie_header);
  if (!cookie_str) {
    return;
  }
  
  /* Split cookie string by '; ' */
  char* saveptr;
  char* cookie_pair = strtok_r(cookie_str, ";", &saveptr);
  
  while (cookie_pair) {
    /* Trim leading whitespace */
    while (*cookie_pair == ' ') {
      cookie_pair++;
    }
    
    /* Find equals sign separating name and value */
    char* equals = strchr(cookie_pair, '=');
    if (equals) {
      /* Create cookie */
      cookie_t* cookie = (cookie_t*)BUFFER_ALLOC(sizeof(cookie_t));
      if (cookie) {
        /* Get name and value */
        *equals = '\0';
        cookie->name = BUFFER_STRDUP(cookie_pair);
        cookie->value = BUFFER_STRDUP(equals + 1);
        cookie->next = NULL;
        
        /* Add to cookies list */
        if (!request->cookies) {
          request->cookies = cookie;
        } else {
          cookie_t* last = request->cookies;
          while (last->next) {
            last = last->next;
          }
          last->next = cookie;
        }
      }
    }
    
    cookie_pair = strtok_r(NULL, ";", &saveptr);
  }
  
  BUFFER_FREE(cookie_str);
}

/* Get cookie value by name */
char* get_cookie_value(http_request_t* request, const char* name) {
  if (!request || !request->cookies || !name) {
    return NULL;
  }
  
  cookie_t* cookie = request->cookies;
  while (cookie) {
    if (cookie->name && strcmp(cookie->name, name) == 0) {
      return cookie->value;
    }
    cookie = cookie->next;
  }
  
  return NULL;
}

/* Parse HTTP request from string */
http_request_t* parse_http_request(const char* request_str) {
  if (!request_str) {
    if (g_logger) {
      TRACE_NET("HTTP_PARSE: request_str is NULL.");
    }
    return NULL;
  }
  
  if (g_logger) {
    TRACE_NET("HTTP_PARSE: Starting to parse request, length=%zu", strlen(request_str));
    TRACE_NET("HTTP_PARSE: First 200 chars: %.200s", request_str);
  }
  
  http_request_t* request = (http_request_t*)BUFFER_ALLOC(sizeof(http_request_t));
  if (!request) {
    return NULL;
  }
  
  /* Initialize request fields */
  request->method = HTTP_UNKNOWN;
  request->path = NULL;
  request->query = NULL;
  request->body = NULL;
  request->content_type = NULL;
  request->authorization = NULL;
  request->cookie_header = NULL;
  request->cookies = NULL;
  request->origin = NULL;
  request->content_length = 0;
  request->user_agent = NULL;
  request->remote_addr = NULL;
  request->keep_alive = 0;  /* Default to close connection */
  
  /* Parse request line and headers */
  char* request_copy = BUFFER_STRDUP(request_str);
  if (!request_copy) {
    /* 🔧 FIX: Handle allocation failure for large requests */
    if (g_logger) {
      LOG_ERROR("Failed to allocate memory for request parsing");
    }
    BUFFER_FREE(request);
    return NULL;
  }
  
  char* line = strtok(request_copy, "\r\n");
  
  if (line) {
    /* Parse request line */
    char method_str[16] = {0};
    char url[2048] = {0};
    sscanf(line, "%15s %2047s", method_str, url);
    
    /* Set method */
    request->method = parse_http_method(method_str);
    
    if (g_logger) {
      TRACE_NET("HTTP_PARSE: method_str='%s', parsed_method=%d, path='%s'", 
          method_str, request->method, url);
    }
    
    /* Parse URL (path and query) */
    char* query_start = strchr(url, '?');
    if (query_start) {
      /* Split path and query */
      *query_start = '\0';
      query_start++;
      request->path = BUFFER_STRDUP(url);
      request->query = BUFFER_STRDUP(query_start);
    } else {
      /* No query part */
      request->path = BUFFER_STRDUP(url);
      request->query = NULL;
    }
    
    if (g_logger) {
      TRACE_NET("HTTP_PARSE: final path='%s', query='%s'", 
          request->path ? request->path : "NULL", 
          request->query ? request->query : "NULL");
    }
    
    /* Parse headers */
    line = strtok(NULL, "\r\n");
    while (line && *line) {
      /* Content-Type header */
      if (strncasecmp(line, "Content-Type:", 13) == 0) {
        char* value = line + 14;
        /* Trim leading whitespace before allocation */
        while (*value == ' ') {
          value++;
        }
        request->content_type = BUFFER_STRDUP(value);
      }
      
      /* Content-Length header */
      else if (strncasecmp(line, "Content-Length:", 15) == 0) {
        request->content_length = atoi(line + 16);
        if (g_logger) {
          TRACE_NET("HTTP_PARSE: Content-Length=%zu", request->content_length);
        }
      }
      
      /* Authorization header */
      else if (strncasecmp(line, "Authorization:", 14) == 0) {
        char* value = line + 15;
        /* Trim leading whitespace before allocation */
        while (*value == ' ') {
          value++;
        }
        request->authorization = BUFFER_STRDUP(value);
      }
      
      /* Cookie header */
      else if (strncasecmp(line, "Cookie:", 7) == 0) {
        char* value = line + 8;
        /* Trim leading whitespace before allocation */
        while (*value == ' ') {
          value++;
        }
        request->cookie_header = BUFFER_STRDUP(value);
      }
      
      /* Origin header */
      else if (strncasecmp(line, "Origin:", 7) == 0) {
        char* value = line + 8;
        /* Trim leading whitespace before allocation */
        while (*value == ' ') {
          value++;
        }
        request->origin = BUFFER_STRDUP(value);
      }
      
      /* User-Agent header */
      else if (strncasecmp(line, "User-Agent:", 11) == 0) {
        char* value = line + 12;
        /* Trim leading whitespace before allocation */
        while (*value == ' ') {
          value++;
        }
        request->user_agent = BUFFER_STRDUP(value);
      }
      
      /* Connection header */
      else if (strncasecmp(line, "Connection:", 11) == 0) {
        char* conn_value = line + 12;
        /* Trim leading whitespace */
        while (*conn_value == ' ') {
          conn_value++;
        }
        /* Check for keep-alive */
        if (strncasecmp(conn_value, "keep-alive", 10) == 0) {
          request->keep_alive = 1;
        } else if (strncasecmp(conn_value, "close", 5) == 0) {
          request->keep_alive = 0;
        }
        /* For HTTP/1.1, default is keep-alive unless explicitly closed */
      }
      
      line = strtok(NULL, "\r\n");
    }
    
    /* Parse body if present */
    if (request->content_length > 0) {
      /* Find the body start (after the double CRLF) */
      const char* body_start = strstr(request_str, "\r\n\r\n");
      if (body_start) {
        body_start += 4; /* Skip the double CRLF */
        request->body = BUFFER_STRDUP(body_start);
        if (g_logger) {
          TRACE_NET("HTTP_PARSE: Body found, length=%zu, content=%.100s", 
              strlen(request->body), request->body);
        }
      } else {
        if (g_logger) {
          TRACE_NET("HTTP_PARSE: Content-Length=%zu but no body found", request->content_length);
        }
      }
    }
    
    /* Parse cookies if present */
    if (request->cookie_header) {
      parse_cookies(request);
    }
  }
  
  BUFFER_FREE(request_copy);
  return request;
}

/* Free HTTP request */
void free_http_request(http_request_t* request) {
  if (request) {
    if (request->path) BUFFER_FREE(request->path);
    if (request->query) BUFFER_FREE(request->query);
    if (request->body) BUFFER_FREE(request->body);
    if (request->content_type) BUFFER_FREE(request->content_type);
    if (request->authorization) BUFFER_FREE(request->authorization);
    if (request->cookie_header) BUFFER_FREE(request->cookie_header);
    if (request->origin) BUFFER_FREE(request->origin);
    if (request->user_agent) BUFFER_FREE(request->user_agent);
    if (request->remote_addr) BUFFER_FREE(request->remote_addr);
    
    /* Free cookies */
    cookie_t* cookie = request->cookies;
    while (cookie) {
      cookie_t* next = cookie->next;
      if (cookie->name) BUFFER_FREE(cookie->name);
      if (cookie->value) BUFFER_FREE(cookie->value);
      BUFFER_FREE(cookie);
      cookie = next;
    }
    
    BUFFER_FREE(request);
  }
}