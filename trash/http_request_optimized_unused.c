/**
 * Optimized HTTP request parsing implementation
 * 
 * This version minimizes memory allocations by parsing in-place
 * and using string views instead of copying strings.
 */

#include "core/server.h"
#include "utils/buffer_pool.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* String view structure for zero-copy parsing */
typedef struct string_view {
    const char* data;
    size_t length;
} string_view_t;

/* Helper to trim whitespace from a string view */
static string_view_t trim_whitespace(string_view_t sv) {
    while (sv.length > 0 && isspace(*sv.data)) {
        sv.data++;
        sv.length--;
    }
    while (sv.length > 0 && isspace(sv.data[sv.length - 1])) {
        sv.length--;
    }
    return sv;
}

/* Helper to find a character in a string view */
static const char* sv_find_char(string_view_t sv, char c) {
    for (size_t i = 0; i < sv.length; i++) {
        if (sv.data[i] == c) {
            return &sv.data[i];
        }
    }
    return NULL;
}

/* Helper to create a null-terminated string from a string view using buffer pool */
static char* sv_to_string(string_view_t sv) {
    if (sv.length == 0) return NULL;
    
    char* str = buffer_pool_alloc(sv.length + 1);
    if (!str) return NULL;
    
    memcpy(str, sv.data, sv.length);
    str[sv.length] = '\0';
    return str;
}

/* Helper to compare string view with string (case insensitive) */
static int sv_casecmp(string_view_t sv, const char* str) {
    size_t str_len = strlen(str);
    if (sv.length != str_len) return -1;
    
    for (size_t i = 0; i < sv.length; i++) {
        if (tolower(sv.data[i]) != tolower(str[i])) {
            return -1;
        }
    }
    return 0;
}

/* Parse HTTP request optimized version */
http_request_t* parse_http_request_optimized(const char* request_str) {
    if (!request_str) return NULL;
    
    /* Allocate request structure from buffer pool */
    http_request_t* request = buffer_pool_alloc(sizeof(http_request_t));
    if (!request) return NULL;
    
    memset(request, 0, sizeof(http_request_t));
    
    /* Find end of headers */
    const char* headers_end = strstr(request_str, "\r\n\r\n");
    if (!headers_end) {
        buffer_pool_free(request);
        return NULL;
    }
    
    /* Parse request line */
    const char* line_end = strstr(request_str, "\r\n");
    if (!line_end || line_end > headers_end) {
        buffer_pool_free(request);
        return NULL;
    }
    
    /* Parse method, path, and version */
    string_view_t request_line = {request_str, line_end - request_str};
    
    /* Find first space */
    const char* space1 = sv_find_char(request_line, ' ');
    if (!space1) {
        buffer_pool_free(request);
        return NULL;
    }
    
    /* Method */
    string_view_t method_sv = {request_line.data, space1 - request_line.data};
    char method_str[16];
    if (method_sv.length >= sizeof(method_str)) {
        buffer_pool_free(request);
        return NULL;
    }
    memcpy(method_str, method_sv.data, method_sv.length);
    method_str[method_sv.length] = '\0';
    request->method = parse_http_method(method_str);
    
    /* Find second space */
    const char* space2 = strchr(space1 + 1, ' ');
    if (!space2 || space2 > line_end) {
        buffer_pool_free(request);
        return NULL;
    }
    
    /* URL (path and query) */
    string_view_t url_sv = {space1 + 1, space2 - space1 - 1};
    const char* query_start = sv_find_char(url_sv, '?');
    
    if (query_start) {
        /* Has query string */
        string_view_t path_sv = {url_sv.data, query_start - url_sv.data};
        string_view_t query_sv = {query_start + 1, url_sv.length - (query_start - url_sv.data) - 1};
        
        request->path = sv_to_string(path_sv);
        request->query = sv_to_string(query_sv);
    } else {
        /* No query string */
        request->path = sv_to_string(url_sv);
        request->query = NULL;
    }
    
    /* Parse headers */
    const char* header_start = line_end + 2;  /* Skip \r\n */
    
    while (header_start < headers_end) {
        const char* header_end = strstr(header_start, "\r\n");
        if (!header_end || header_end > headers_end) break;
        
        string_view_t header_line = {header_start, header_end - header_start};
        
        /* Find colon */
        const char* colon = sv_find_char(header_line, ':');
        if (!colon) {
            header_start = header_end + 2;
            continue;
        }
        
        string_view_t header_name = {header_line.data, colon - header_line.data};
        string_view_t header_value = {colon + 1, header_line.length - (colon - header_line.data) - 1};
        header_value = trim_whitespace(header_value);
        
        /* Check specific headers we care about */
        if (sv_casecmp(header_name, "content-type") == 0) {
            request->content_type = sv_to_string(header_value);
        } else if (sv_casecmp(header_name, "content-length") == 0) {
            char len_str[32];
            if (header_value.length < sizeof(len_str)) {
                memcpy(len_str, header_value.data, header_value.length);
                len_str[header_value.length] = '\0';
                request->content_length = atoi(len_str);
            }
        } else if (sv_casecmp(header_name, "authorization") == 0) {
            request->authorization = sv_to_string(header_value);
        } else if (sv_casecmp(header_name, "cookie") == 0) {
            request->cookie_header = sv_to_string(header_value);
        } else if (sv_casecmp(header_name, "origin") == 0) {
            request->origin = sv_to_string(header_value);
        } else if (sv_casecmp(header_name, "user-agent") == 0) {
            request->user_agent = sv_to_string(header_value);
        }
        
        header_start = header_end + 2;
    }
    
    /* Parse body if present */
    if (request->content_length > 0) {
        const char* body_start = headers_end + 4;  /* Skip \r\n\r\n */
        request->body = buffer_pool_alloc(request->content_length + 1);
        if (request->body) {
            memcpy(request->body, body_start, request->content_length);
            request->body[request->content_length] = '\0';
        }
    }
    
    /* Parse cookies if present */
    if (request->cookie_header) {
        parse_cookies(request);
    }
    
    return request;
}

/* Free HTTP request using buffer pool */
void free_http_request_optimized(http_request_t* request) {
    if (!request) return;
    
    /* Free all allocated strings */
    if (request->path) buffer_pool_free(request->path);
    if (request->query) buffer_pool_free(request->query);
    if (request->body) buffer_pool_free(request->body);
    if (request->content_type) buffer_pool_free(request->content_type);
    if (request->authorization) buffer_pool_free(request->authorization);
    if (request->cookie_header) buffer_pool_free(request->cookie_header);
    if (request->origin) buffer_pool_free(request->origin);
    if (request->user_agent) buffer_pool_free(request->user_agent);
    
    /* Free cookies (linked list) */
    cookie_t* cookie = request->cookies;
    while (cookie) {
        cookie_t* next = cookie->next;
        if (cookie->name) buffer_pool_free(cookie->name);
        if (cookie->value) buffer_pool_free(cookie->value);
        buffer_pool_free(cookie);
        cookie = next;
    }
    
    /* Free the request structure itself */
    buffer_pool_free(request);
}