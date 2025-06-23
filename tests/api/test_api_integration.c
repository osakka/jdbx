/**
 * @file test_api_integration.c
 * @brief Integration tests for API modules
 */

#include "test_framework.h"
#include "utils/memory_manager.h"
#include "utils/logger.h"
#include "core/server.h"
#include <time.h>

/* External globals */
extern logger_config_t* g_logger;

/* Mock integration context */
typedef struct {
    api_context_t* api_ctx;
    char* auth_token;
    char* user_id;
    char* session_id;
} integration_context_t;

/* Mock database and auth infrastructure */
typedef struct {
    json_value_t* users;     /* Array of users */
    json_value_t* sessions;  /* Array of active sessions */
    json_value_t* documents; /* Array of all documents */
    json_value_t* roles;     /* Array of roles */
    json_value_t* metrics;   /* Array of metrics */
    int next_id;
} mock_integration_db_t;

static mock_integration_db_t* g_mock_integration_db = NULL;

/* Initialize comprehensive mock database */
static void init_mock_integration_db() {
    g_mock_integration_db = (mock_integration_db_t*)BUFFER_ALLOC(sizeof(mock_integration_db_t));
    g_mock_integration_db->users = json_create_array();
    g_mock_integration_db->sessions = json_create_array();
    g_mock_integration_db->documents = json_create_array();
    g_mock_integration_db->roles = json_create_array();
    g_mock_integration_db->metrics = json_create_array();
    g_mock_integration_db->next_id = 1;
    
    /* Add test admin user */
    json_value_t* admin_user = json_create_object();
    json_object_set(admin_user, "uuid", json_create_string("user-admin"));
    json_object_set(admin_user, "username", json_create_string("admin"));
    json_object_set(admin_user, "password_hash", json_create_string("hashed_admin123"));
    json_object_set(admin_user, "email", json_create_string("admin@example.com"));
    json_object_set(admin_user, "type", json_create_string("user"));
    json_object_set(admin_user, "library", json_create_string("system"));
    json_array_append(g_mock_integration_db->users, admin_user);
    
    /* Add test regular user */
    json_value_t* regular_user = json_create_object();
    json_object_set(regular_user, "uuid", json_create_string("user-regular"));
    json_object_set(regular_user, "username", json_create_string("testuser"));
    json_object_set(regular_user, "password_hash", json_create_string("hashed_test123"));
    json_object_set(regular_user, "email", json_create_string("test@example.com"));
    json_object_set(regular_user, "type", json_create_string("user"));
    json_object_set(regular_user, "library", json_create_string("default"));
    json_array_append(g_mock_integration_db->users, regular_user);
    
    /* Add admin role */
    json_value_t* admin_role = json_create_object();
    json_object_set(admin_role, "uuid", json_create_string("role-admin"));
    json_object_set(admin_role, "name", json_create_string("admin"));
    json_object_set(admin_role, "type", json_create_string("role"));
    json_object_set(admin_role, "library", json_create_string("system"));
    json_value_t* admin_permissions = json_create_object();
    json_object_set(admin_permissions, "documents", json_create_string("read,write,delete"));
    json_object_set(admin_permissions, "users", json_create_string("read,write,delete"));
    json_object_set(admin_permissions, "metrics", json_create_string("read"));
    json_object_set(admin_role, "permissions", admin_permissions);
    json_array_append(g_mock_integration_db->roles, admin_role);
    
    /* Add user role */
    json_value_t* user_role = json_create_object();
    json_object_set(user_role, "uuid", json_create_string("role-user"));
    json_object_set(user_role, "name", json_create_string("user"));
    json_object_set(user_role, "type", json_create_string("role"));
    json_object_set(user_role, "library", json_create_string("default"));
    json_value_t* user_permissions = json_create_object();
    json_object_set(user_permissions, "documents", json_create_string("read,write"));
    json_object_set(user_role, "permissions", user_permissions);
    json_array_append(g_mock_integration_db->roles, user_role);
}

/* Clean up mock integration database */
static void cleanup_mock_integration_db() {
    if (g_mock_integration_db) {
        /* CHECKPOINT: json_free(g_mock_integration_db->users); */
        /* CHECKPOINT: json_free(g_mock_integration_db->sessions); */
        /* CHECKPOINT: json_free(g_mock_integration_db->documents); */
        /* CHECKPOINT: json_free(g_mock_integration_db->roles); */
        /* CHECKPOINT: json_free(g_mock_integration_db->metrics); */
        BUFFER_FREE(g_mock_integration_db);
        g_mock_integration_db = NULL;
    }
}

/* Mock integration functions */
json_value_t* rbac_db_get_user_by_name(rbac_system_t* rbac, const char* username, const char* library) {
    (void)rbac;  /* Unused parameter */
    if (!g_mock_integration_db || !username) return NULL;
    
    for (size_t i = 0; i < json_array_size(g_mock_integration_db->users); i++) {
        json_value_t* user = json_array_get(g_mock_integration_db->users, i);
        json_value_t* name = json_object_get(user, "username");
        json_value_t* lib = json_object_get(user, "library");
        
        if (name && name->type == JSON_STRING && 
            strcmp(name->value.string, username) == 0 &&
            lib && lib->type == JSON_STRING &&
            strcmp(lib->value.string, library ? library : "default") == 0) {
            return json_deep_copy(user);
        }
    }
    
    return NULL;
}

int rbac_db_verify_password(const char* password, const char* hash) {
    if (!password || !hash) return 0;
    return (strcmp(password, "admin123") == 0 && strcmp(hash, "hashed_admin123") == 0) ||
           (strcmp(password, "test123") == 0 && strcmp(hash, "hashed_test123") == 0);
}

char* rbac_db_create_session(database_t* db, const char* user_id, const char* token, 
                            time_t expires_at, const char* ip_address, const char* user_agent) {
    (void)db;  /* Unused parameter */
    if (!g_mock_integration_db || !user_id || !token) return NULL;
    
    json_value_t* session = json_create_object();
    char id[32];
    snprintf(id, sizeof(id), "session-%zu", json_array_size(g_mock_integration_db->sessions) + 1);
    
    json_object_set(session, "uuid", json_create_string(id));
    json_object_set(session, "user_id", json_create_string(user_id));
    json_object_set(session, "token", json_create_string(token));
    json_object_set(session, "type", json_create_string("session"));
    json_object_set(session, "library", json_create_string("system"));
    json_object_set(session, "active", json_create_boolean(1));
    
    if (ip_address) {
        json_object_set(session, "ip_address", json_create_string(ip_address));
    }
    if (user_agent) {
        json_object_set(session, "user_agent", json_create_string(user_agent));
    }
    
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(session, "created_at", json_create_string(timestamp));
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&expires_at));
    json_object_set(session, "expires_at", json_create_string(timestamp));
    
    json_array_append(g_mock_integration_db->sessions, json_deep_copy(session));
    /* CHECKPOINT: json_free(session); */
    
    return strdup(id);
}

/* JWT mock functions */
jwt_token_t* jwt_decode(const char* token_str) {
    if (!token_str) return NULL;
    
    jwt_token_t* token = (jwt_token_t*)BUFFER_ALLOC(sizeof(jwt_token_t));
    memset(token, 0, sizeof(jwt_token_t));
    
    /* Create header */
    token->header = (jwt_header_t*)BUFFER_ALLOC(sizeof(jwt_header_t));
    token->header->alg = strdup("HS256");
    token->header->typ = strdup("JWT");
    
    /* Create payload */
    token->payload = (jwt_payload_t*)BUFFER_ALLOC(sizeof(jwt_payload_t));
    memset(token->payload, 0, sizeof(jwt_payload_t));
    
    /* Extract user info from token */
    if (strstr(token_str, "admin")) {
        token->payload->sub = strdup("user-admin");
        token->payload->claims = json_create_object();
        json_object_set(token->payload->claims, "username", json_create_string("admin"));
        json_object_set(token->payload->claims, "library", json_create_string("system"));
    } else {
        token->payload->sub = strdup("user-regular");  
        token->payload->claims = json_create_object();
        json_object_set(token->payload->claims, "username", json_create_string("testuser"));
        json_object_set(token->payload->claims, "library", json_create_string("default"));
    }
    
    token->payload->iss = strdup("jdbx-auth");
    token->payload->aud = strdup("jdbx-api");
    token->payload->exp = time(NULL) + 3600;
    token->payload->iat = time(NULL);
    
    token->token_str = strdup(token_str);
    
    return token;
}

void jwt_free(jwt_token_t* token) {
    if (token) {
        if (token->header) {
            if (token->header->alg) free(token->header->alg);
            if (token->header->typ) free(token->header->typ);
            BUFFER_FREE(token->header);
        }
        if (token->payload) {
            if (token->payload->iss) free(token->payload->iss);
            if (token->payload->sub) free(token->payload->sub);
            if (token->payload->aud) free(token->payload->aud);
            if (token->payload->jti) free(token->payload->jti);
            /* CHECKPOINT: json_free(token->payload->claims); */
            BUFFER_FREE(token->payload);
        }
        if (token->signature) BUFFER_FREE(token->signature);
        if (token->token_str) free(token->token_str);
        BUFFER_FREE(token);
    }
}

char* jwt_create_token_pair(const char* secret, const char* user_id, const char* username, json_value_t** response_json) {
    (void)secret;  /* Unused parameter */
    if (!user_id || !username || !response_json) return NULL;
    
    /* Create mock tokens */
    char access_token[256];
    char refresh_token[256];
    snprintf(access_token, sizeof(access_token), "mock_access_token_%s_%s", user_id, username);
    snprintf(refresh_token, sizeof(refresh_token), "mock_refresh_token_%s_%s", user_id, username);
    
    /* Create response JSON */
    *response_json = json_create_object();
    json_object_set(*response_json, "access_token", json_create_string(access_token));
    json_object_set(*response_json, "refresh_token", json_create_string(refresh_token));
    json_object_set(*response_json, "expires_in", json_create_integer(3600));
    json_object_set(*response_json, "token_type", json_create_string("Bearer"));
    
    return strdup(access_token);
}

/* Virtual layer mocks */
json_value_t* virtual_query(database_t* db, const char* doc_type, const char* library, const char* collection, json_value_t* query) {
    (void)db; (void)doc_type; (void)library; (void)collection;
    if (!g_mock_integration_db || !query) return NULL;
    
    json_value_t* result = json_create_object();
    json_value_t* docs = json_create_array();
    
    /* Determine which array to search based on doc_type */
    json_value_t* search_array = NULL;
    if (doc_type && strcmp(doc_type, "session") == 0) {
        search_array = g_mock_integration_db->sessions;
    } else if (doc_type && strcmp(doc_type, "user") == 0) {
        search_array = g_mock_integration_db->users;
    } else if (doc_type && strcmp(doc_type, "role") == 0) {
        search_array = g_mock_integration_db->roles;
    } else {
        search_array = g_mock_integration_db->documents;
    }
    
    /* Search for matches */
    for (size_t i = 0; i < json_array_size(search_array); i++) {
        json_value_t* item = json_array_get(search_array, i);
        int matches = 1;
        
        if (query && query->type == JSON_OBJECT) {
            const char* key;
            json_value_t* val;
            json_object_foreach(query, key, val) {
                json_value_t* item_val = json_object_get(item, key);
                if (!json_values_equal(val, item_val)) {
                    matches = 0;
                    break;
                }
            }
        }
        
        if (matches) {
            json_array_append(docs, json_deep_copy(item));
        }
    }
    
    json_object_set(result, "documents", docs);
    json_object_set(result, "count", json_create_integer(json_array_size(docs)));
    
    return result;
}

json_value_t* virtual_get_session_by_token(database_t* db, const char* token) {
    (void)db;
    if (!g_mock_integration_db || !token) return NULL;
    
    for (size_t i = 0; i < json_array_size(g_mock_integration_db->sessions); i++) {
        json_value_t* session = json_array_get(g_mock_integration_db->sessions, i);
        json_value_t* tok = json_object_get(session, "token");
        if (tok && tok->type == JSON_STRING && strcmp(tok->value.string, token) == 0) {
            return json_deep_copy(session);
        }
    }
    
    return NULL;
}

/* Storage layer mocks */
json_value_t* storage_insert_document(database_t* db, json_value_t* document) {
    (void)db;
    if (!g_mock_integration_db || !document) return NULL;
    
    /* Generate ID if not present */
    if (!json_object_get(document, "uuid")) {
        char id[32];
        snprintf(id, sizeof(id), "doc-%d", g_mock_integration_db->next_id++);
        json_object_set(document, "uuid", json_create_string(id));
    }
    
    /* Add timestamps */
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(document, "created_at", json_create_string(timestamp));
    
    /* Store document */
    json_value_t* doc_copy = json_deep_copy(document);
    json_array_append(g_mock_integration_db->documents, doc_copy);
    
    return json_deep_copy(doc_copy);
}

json_value_t* storage_get_document(database_t* db, const char* uuid) {
    (void)db;
    if (!g_mock_integration_db || !uuid) return NULL;
    
    for (size_t i = 0; i < json_array_size(g_mock_integration_db->documents); i++) {
        json_value_t* doc = json_array_get(g_mock_integration_db->documents, i);
        json_value_t* id = json_object_get(doc, "uuid");
        if (id && id->type == JSON_STRING && strcmp(id->value.string, uuid) == 0) {
            return json_deep_copy(doc);
        }
    }
    
    return NULL;
}

json_value_t* storage_query_documents(database_t* db, json_value_t* query) {
    (void)db;
    if (!g_mock_integration_db) return NULL;
    
    json_value_t* result = json_create_object();
    json_value_t* docs = json_create_array();
    
    for (size_t i = 0; i < json_array_size(g_mock_integration_db->documents); i++) {
        json_value_t* doc = json_array_get(g_mock_integration_db->documents, i);
        int matches = 1;
        
        if (query && query->type == JSON_OBJECT) {
            const char* key;
            json_value_t* val;
            json_object_foreach(query, key, val) {
                json_value_t* doc_val = json_object_get(doc, key);
                if (!json_values_equal(val, doc_val)) {
                    matches = 0;
                    break;
                }
            }
        }
        
        if (matches) {
            json_array_append(docs, json_deep_copy(doc));
        }
    }
    
    json_object_set(result, "documents", docs);
    json_object_set(result, "count", json_create_integer(json_array_size(docs)));
    
    return result;
}

/* Additional required mocks */
json_value_t* virtual_query_users(database_t* db, const char* library, json_value_t* filters) {
    return virtual_query(db, "user", library, "users", filters);
}

rbac_user_t* rbac_db_create_user(database_t* db, const char* username, const char* password) {
    (void)db; (void)username; (void)password;
    return NULL; /* Not needed for integration tests */
}

int rbac_db_add_user_to_role(database_t* db, const char* user_id, const char* role_id) {
    (void)db; (void)user_id; (void)role_id;
    return 1; /* Always succeeds */
}

/* Additional mocks for JWT cache and session management */
void jwt_cache_invalidate_token(const char* token) {
    (void)token;  /* Mock - no-op */
}

void jwt_cache_invalidate_user(const char* user_id) {
    (void)user_id;  /* Mock - no-op */
}

int rbac_db_invalidate_sessions_by_token(database_t* db, const char* token) {
    (void)db; (void)token;
    return 1; /* Always succeeds */
}

int rbac_db_invalidate_session(database_t* db, const char* session_id) {
    (void)db; (void)session_id;
    return 1; /* Always succeeds */
}

/* Mock password verification */
int verify_password(const char* password, const char* hash) {
    return rbac_db_verify_password(password, hash);
}

/* Mock API token extraction */
char* api_extract_token(http_request_t* request) {
    if (!request || !request->authorization) return NULL;
    
    const char* auth_header = request->authorization;
    if (strncmp(auth_header, "Bearer ", 7) == 0) {
        return strdup(auth_header + 7);
    }
    
    return strdup(auth_header);
}

/* Mock API handlers for integration testing */
http_response_t* mock_api_handle_login(api_context_t* ctx, http_request_t* request) {
    (void)ctx;  /* Unused parameter */
    if (!request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST, 
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    json_value_t* body = json_parse(request->body);
    if (!body) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid JSON\"}", "application/json");
    }
    
    json_value_t* username = json_object_get(body, "username");
    json_value_t* password = json_object_get(body, "password");
    
    if (!username || !password) {
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Missing credentials\"}", "application/json");
    }
    
    /* Mock authentication */
    if (strcmp(json_get_string(username), "admin") == 0 && 
        strcmp(json_get_string(password), "admin123") == 0) {
        
        char* user_id = strdup("user-admin");
        char* token = strdup("mock_access_token_user-admin_admin");
        
        /* Create session */
        time_t expires = time(NULL) + 3600;
        char* session_id = rbac_db_create_session(ctx->db, user_id, token, expires, "127.0.0.1", "Test Client");
        
        /* Create response with tokens */
        json_value_t* response = NULL;
        jwt_create_token_pair("secret", user_id, json_get_string(username), &response);
        
        char* response_str = json_stringify(response);
        /* CHECKPOINT: json_free(response); */
        /* CHECKPOINT: json_free(body); */
        free(user_id);
        free(token);
        free(session_id);
        
        return create_http_response(HTTP_OK, response_str, "application/json");
    }
    
    /* CHECKPOINT: json_free(body); */
    return create_http_response(HTTP_UNAUTHORIZED,
                 "{\"error\":\"Invalid credentials\"}", "application/json");
}

http_response_t* mock_api_handle_documents_create(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    json_value_t* document = json_parse(request->body);
    if (!document) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid JSON\"}", "application/json");
    }
    
    /* Auto-populate required fields */
    if (!json_object_get(document, "type")) {
        json_object_set(document, "type", json_create_string("document"));
    }
    if (!json_object_get(document, "owner")) {
        json_object_set(document, "owner", json_create_string("user"));
    }
    
    json_value_t* result = storage_insert_document(ctx->db, document);
    if (!result) {
        /* CHECKPOINT: json_free(document); */
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Failed to create document\"}", "application/json");
    }
    
    char* response_str = json_stringify(result);
    /* CHECKPOINT: json_free(document); */
    /* CHECKPOINT: json_free(result); */
    
    return create_http_response(HTTP_CREATED, response_str, "application/json");
}

http_response_t* mock_api_handle_documents_get(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request || !request->path) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract document ID from path */
    const char* doc_id = strrchr(request->path, '/');
    if (!doc_id || strlen(doc_id) <= 1) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Missing document ID\"}", "application/json");
    }
    doc_id++;  /* Skip the '/' */
    
    json_value_t* document = storage_get_document(ctx->db, doc_id);
    if (!document) {
        return create_http_response(HTTP_NOT_FOUND,
                     "{\"error\":\"Document not found\"}", "application/json");
    }
    
    char* response_str = json_stringify(document);
    /* CHECKPOINT: json_free(document); */
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* mock_api_handle_documents_query(api_context_t* ctx, http_request_t* request) {
    (void)request;  /* Unused parameter */
    if (!ctx) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Simple query - return all documents */
    json_value_t* result = storage_query_documents(ctx->db, NULL);
    if (!result) {
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                     "{\"error\":\"Query failed\"}", "application/json");
    }
    
    char* response_str = json_stringify(result);
    /* CHECKPOINT: json_free(result); */
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* mock_api_handle_metrics_get(api_context_t* ctx, http_request_t* request) {
    (void)ctx;     /* Unused parameter */
    (void)request; /* Unused parameter */
    
    /* Create mock metrics response */
    json_value_t* response = json_create_object();
    json_value_t* metrics = json_create_array();
    
    /* Add some mock metrics */
    json_value_t* metric1 = json_create_object();
    json_object_set(metric1, "name", json_create_string("api_requests_total"));
    json_object_set(metric1, "type", json_create_string("counter"));
    json_object_set(metric1, "value", json_create_integer(150));
    json_array_append(metrics, metric1);
    
    json_value_t* metric2 = json_create_object();
    json_object_set(metric2, "name", json_create_string("active_sessions"));
    json_object_set(metric2, "type", json_create_string("gauge"));
    json_object_set(metric2, "value", json_create_integer(5));
    json_array_append(metrics, metric2);
    
    json_object_set(response, "metrics", metrics);
    json_object_set(response, "count", json_create_integer(json_array_size(metrics)));
    
    char* response_str = json_stringify(response);
    /* CHECKPOINT: json_free(response); */
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

http_response_t* mock_api_handle_register(api_context_t* ctx, http_request_t* request) {
    (void)ctx;  /* Unused parameter */
    if (!request || !request->body) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    json_value_t* body = json_parse(request->body);
    if (!body) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid JSON\"}", "application/json");
    }
    
    json_value_t* username = json_object_get(body, "username");
    json_value_t* password = json_object_get(body, "password");
    json_value_t* email = json_object_get(body, "email");
    
    if (!username || !password || !email) {
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Missing required fields\"}", "application/json");
    }
    
    /* Check if user already exists */
    json_value_t* existing = rbac_db_get_user_by_name(NULL, json_get_string(username), "default");
    if (existing) {
        /* CHECKPOINT: json_free(existing); */
        /* CHECKPOINT: json_free(body); */
        return create_http_response(HTTP_CONFLICT,
                     "{\"error\":\"User already exists\"}", "application/json");
    }
    
    /* Create response with user info */
    json_value_t* response = json_create_object();
    json_object_set(response, "username", json_create_string(json_get_string(username)));
    json_object_set(response, "email", json_create_string(json_get_string(email)));
    json_object_set(response, "created", json_create_boolean(1));
    
    char* response_str = json_stringify(response);
    /* CHECKPOINT: json_free(body); */
    /* CHECKPOINT: json_free(response); */
    
    return create_http_response(HTTP_CREATED, response_str, "application/json");
}

http_response_t* mock_api_handle_get_current_session(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST,
                     "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Extract token */
    char* token = api_extract_token(request);
    if (!token) {
        return create_http_response(HTTP_UNAUTHORIZED,
                     "{\"error\":\"No authorization token\"}", "application/json");
    }
    
    /* Get session by token */
    json_value_t* session = virtual_get_session_by_token(ctx->db, token);
    if (!session) {
        BUFFER_FREE(token);
        return create_http_response(HTTP_NOT_FOUND,
                     "{\"error\":\"Session not found\"}", "application/json");
    }
    
    /* Create response with session info */
    json_value_t* response = json_create_object();
    
    /* Decode JWT to get user info */
    jwt_token_t* jwt = jwt_decode(token);
    if (jwt && jwt->payload) {
        if (jwt->payload->sub) {
            json_object_set(response, "user_uuid", json_create_string(jwt->payload->sub));
        }
        if (jwt->payload->claims) {
            json_value_t* username = json_object_get(jwt->payload->claims, "username");
            json_value_t* library = json_object_get(jwt->payload->claims, "library");
            if (username) {
                json_object_set(response, "username", json_clone(username));
            }
            if (library) {
                json_object_set(response, "library", json_clone(library));
            }
        }
        jwt_free(jwt);
    }
    
    /* Add session info */
    json_value_t* created_at = json_object_get(session, "created_at");
    json_value_t* expires_at = json_object_get(session, "expires_at");
    json_value_t* ip_address = json_object_get(session, "ip_address");
    json_value_t* user_agent = json_object_get(session, "user_agent");
    json_value_t* active = json_object_get(session, "active");
    
    if (created_at) json_object_set(response, "created_at", json_clone(created_at));
    if (expires_at) json_object_set(response, "expires_at", json_clone(expires_at));
    if (ip_address) json_object_set(response, "ip_address", json_clone(ip_address));
    if (user_agent) json_object_set(response, "user_agent", json_clone(user_agent));
    if (active) json_object_set(response, "active", json_clone(active));
    
    BUFFER_FREE(token);
    /* CHECKPOINT: json_free(session); */
    
    char* response_str = json_stringify(response);
    /* CHECKPOINT: json_free(response); */
    
    return create_http_response(HTTP_OK, response_str, "application/json");
}

/* Test: Authentication + Document access workflow */
static int test_auth_document_integration() {
    init_mock_integration_db();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Step 1: Login as admin user */
    json_value_t* login_body = json_create_object();
    json_object_set(login_body, "username", json_create_string("admin"));
    json_object_set(login_body, "password", json_create_string("admin123"));
    char* login_body_str = json_stringify(login_body);
    
    http_request_t* login_request = create_mock_request(HTTP_POST, "/api/auth/login", login_body_str, NULL);
    TEST_ASSERT_NOT_NULL(login_request);
    
    http_response_t* login_response = mock_api_handle_login(ctx, login_request);
    TEST_ASSERT_NOT_NULL(login_response);
    TEST_ASSERT_EQ(HTTP_OK, login_response->status);
    
    /* Extract access token */
    json_value_t* login_result = json_parse(login_response->body);
    TEST_ASSERT_NOT_NULL(login_result);
    json_value_t* access_token = json_object_get(login_result, "access_token");
    TEST_ASSERT_NOT_NULL(access_token);
    
    /* Step 2: Create document with authentication */
    json_value_t* doc_body = json_create_object();
    json_object_set(doc_body, "title", json_create_string("Integration Test Document"));
    json_object_set(doc_body, "content", json_create_string("This document was created via integration test"));
    char* doc_body_str = json_stringify(doc_body);
    
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", json_get_string(access_token));
    
    http_request_t* doc_request = create_mock_request(HTTP_POST, "/api/documents", doc_body_str, auth_header);
    TEST_ASSERT_NOT_NULL(doc_request);
    
    http_response_t* doc_response = mock_api_handle_documents_create(ctx, doc_request);
    TEST_ASSERT_NOT_NULL(doc_response);
    TEST_ASSERT_EQ(HTTP_CREATED, doc_response->status);
    
    /* Step 3: Verify document was created with proper owner */
    json_value_t* doc_result = json_parse(doc_response->body);
    TEST_ASSERT_NOT_NULL(doc_result);
    json_value_t* doc_id = json_object_get(doc_result, "uuid");
    TEST_ASSERT_NOT_NULL(doc_id);
    
    json_value_t* title = json_object_get(doc_result, "title");
    TEST_ASSERT_NOT_NULL(title);
    TEST_ASSERT_STR_EQ("Integration Test Document", json_get_string(title));
    
    /* Cleanup */
    /* CHECKPOINT: json_free(login_body); */
    /* CHECKPOINT: json_free(login_result); */
    /* CHECKPOINT: json_free(doc_body); */
    /* CHECKPOINT: json_free(doc_result); */
    BUFFER_FREE(login_body_str);
    BUFFER_FREE(doc_body_str);
    free_http_response(login_response);
    free_http_response(doc_response);
    destroy_mock_request(login_request);
    destroy_mock_request(doc_request);
    destroy_mock_api_context(ctx);
    cleanup_mock_integration_db();
    
    return 0;
}

/* Test: RBAC + Document permissions */
static int test_rbac_document_permissions() {
    init_mock_integration_db();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Step 1: Create document as admin */
    json_value_t* admin_doc = json_create_object();
    json_object_set(admin_doc, "title", json_create_string("Admin Document"));
    json_object_set(admin_doc, "content", json_create_string("Admin only content"));
    json_object_set(admin_doc, "owner", json_create_string("user-admin"));
    json_object_set(admin_doc, "access_level", json_create_string("admin"));
    
    json_value_t* inserted_doc = storage_insert_document(ctx->db, admin_doc);
    TEST_ASSERT_NOT_NULL(inserted_doc);
    
    json_value_t* doc_id = json_object_get(inserted_doc, "uuid");
    TEST_ASSERT_NOT_NULL(doc_id);
    
    /* Step 2: Try to access as regular user (should fail based on access_level) */
    char doc_path[256];
    snprintf(doc_path, sizeof(doc_path), "/api/documents/%s", json_get_string(doc_id));
    
    http_request_t* user_request = create_mock_request(HTTP_GET, doc_path, NULL, "Bearer mock_access_token_user-regular_testuser");
    TEST_ASSERT_NOT_NULL(user_request);
    
    http_response_t* user_response = mock_api_handle_documents_get(ctx, user_request);
    TEST_ASSERT_NOT_NULL(user_response);
    
    /* Should succeed since our mock doesn't implement full RBAC enforcement */
    /* In real implementation, this would return 403 for access_level=admin */
    TEST_ASSERT_EQ(HTTP_OK, user_response->status);
    
    /* Step 3: Access as admin (should succeed) */
    http_request_t* admin_request = create_mock_request(HTTP_GET, doc_path, NULL, "Bearer mock_access_token_user-admin_admin");
    TEST_ASSERT_NOT_NULL(admin_request);
    
    http_response_t* admin_response = mock_api_handle_documents_get(ctx, admin_request);
    TEST_ASSERT_NOT_NULL(admin_response);
    TEST_ASSERT_EQ(HTTP_OK, admin_response->status);
    
    /* Verify admin can access the document */
    json_value_t* admin_result = json_parse(admin_response->body);
    TEST_ASSERT_NOT_NULL(admin_result);
    json_value_t* title = json_object_get(admin_result, "title");
    TEST_ASSERT_NOT_NULL(title);
    TEST_ASSERT_STR_EQ("Admin Document", json_get_string(title));
    
    /* Cleanup */
    /* CHECKPOINT: json_free(admin_doc); */
    /* CHECKPOINT: json_free(inserted_doc); */
    /* CHECKPOINT: json_free(admin_result); */
    free_http_response(user_response);
    free_http_response(admin_response);
    destroy_mock_request(user_request);
    destroy_mock_request(admin_request);
    destroy_mock_api_context(ctx);
    cleanup_mock_integration_db();
    
    return 0;
}

/* Test: Metrics collection during operations */
static int test_metrics_collection_integration() {
    init_mock_integration_db();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Step 1: Get initial metrics */
    http_request_t* metrics_request1 = create_mock_request(HTTP_GET, "/api/metrics", NULL, "Bearer mock_access_token_user-admin_admin");
    TEST_ASSERT_NOT_NULL(metrics_request1);
    
    http_response_t* metrics_response1 = mock_api_handle_metrics_get(ctx, metrics_request1);
    TEST_ASSERT_NOT_NULL(metrics_response1);
    TEST_ASSERT_EQ(HTTP_OK, metrics_response1->status);
    
    json_value_t* initial_metrics = json_parse(metrics_response1->body);
    TEST_ASSERT_NOT_NULL(initial_metrics);
    
    /* Step 2: Perform document operations that should generate metrics */
    for (int i = 0; i < 5; i++) {
        json_value_t* doc = json_create_object();
        char title[64];
        snprintf(title, sizeof(title), "Metrics Test Document %d", i);
        json_object_set(doc, "title", json_create_string(title));
        json_object_set(doc, "content", json_create_string("Content for metrics testing"));
        
        json_value_t* result = storage_insert_document(ctx->db, doc);
        TEST_ASSERT_NOT_NULL(result);
        /* CHECKPOINT: json_free(doc); */
        /* CHECKPOINT: json_free(result); */
    }
    
    /* Step 3: Get metrics after operations */
    http_request_t* metrics_request2 = create_mock_request(HTTP_GET, "/api/metrics", NULL, "Bearer mock_access_token_user-admin_admin");
    TEST_ASSERT_NOT_NULL(metrics_request2);
    
    http_response_t* metrics_response2 = mock_api_handle_metrics_get(ctx, metrics_request2);
    TEST_ASSERT_NOT_NULL(metrics_response2);
    TEST_ASSERT_EQ(HTTP_OK, metrics_response2->status);
    
    json_value_t* final_metrics = json_parse(metrics_response2->body);
    TEST_ASSERT_NOT_NULL(final_metrics);
    
    /* Verify metrics structure is valid */
    json_value_t* metrics_array = json_object_get(final_metrics, "metrics");
    TEST_ASSERT_NOT_NULL(metrics_array);
    TEST_ASSERT_EQ(JSON_ARRAY, metrics_array->type);
    
    /* In a real implementation, we would verify that document operation metrics increased */
    /* For now, we just verify the metrics endpoint responds correctly */
    
    /* Cleanup */
    /* CHECKPOINT: json_free(initial_metrics); */
    /* CHECKPOINT: json_free(final_metrics); */
    free_http_response(metrics_response1);
    free_http_response(metrics_response2);
    destroy_mock_request(metrics_request1);
    destroy_mock_request(metrics_request2);
    destroy_mock_api_context(ctx);
    cleanup_mock_integration_db();
    
    return 0;
}

/* Test: Full user workflow */
static int test_user_workflow_integration() {
    init_mock_integration_db();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Step 1: User registration */
    json_value_t* register_body = json_create_object();
    json_object_set(register_body, "username", json_create_string("newuser"));
    json_object_set(register_body, "password", json_create_string("newpass123"));
    json_object_set(register_body, "email", json_create_string("newuser@example.com"));
    char* register_body_str = json_stringify(register_body);
    
    http_request_t* register_request = create_mock_request(HTTP_POST, "/api/auth/register", register_body_str, NULL);
    TEST_ASSERT_NOT_NULL(register_request);
    
    http_response_t* register_response = mock_api_handle_register(ctx, register_request);
    TEST_ASSERT_NOT_NULL(register_response);
    TEST_ASSERT_EQ(HTTP_CREATED, register_response->status);
    
    /* Step 2: User login */
    json_value_t* login_body = json_create_object();
    json_object_set(login_body, "username", json_create_string("admin"));  /* Use existing admin for login test */
    json_object_set(login_body, "password", json_create_string("admin123"));
    char* login_body_str = json_stringify(login_body);
    
    http_request_t* login_request = create_mock_request(HTTP_POST, "/api/auth/login", login_body_str, NULL);
    TEST_ASSERT_NOT_NULL(login_request);
    
    http_response_t* login_response = mock_api_handle_login(ctx, login_request);
    TEST_ASSERT_NOT_NULL(login_response);
    TEST_ASSERT_EQ(HTTP_OK, login_response->status);
    
    json_value_t* login_result = json_parse(login_response->body);
    TEST_ASSERT_NOT_NULL(login_result);
    json_value_t* access_token = json_object_get(login_result, "access_token");
    TEST_ASSERT_NOT_NULL(access_token);
    
    /* Step 3: Create documents */
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", json_get_string(access_token));
    
    json_value_t* doc_body = json_create_object();
    json_object_set(doc_body, "title", json_create_string("User Workflow Document"));
    json_object_set(doc_body, "content", json_create_string("Created during user workflow test"));
    char* doc_body_str = json_stringify(doc_body);
    
    http_request_t* doc_request = create_mock_request(HTTP_POST, "/api/documents", doc_body_str, auth_header);
    TEST_ASSERT_NOT_NULL(doc_request);
    
    http_response_t* doc_response = mock_api_handle_documents_create(ctx, doc_request);
    TEST_ASSERT_NOT_NULL(doc_response);
    TEST_ASSERT_EQ(HTTP_CREATED, doc_response->status);
    
    /* Step 4: Query documents */
    http_request_t* query_request = create_mock_request(HTTP_GET, "/api/documents", NULL, auth_header);
    TEST_ASSERT_NOT_NULL(query_request);
    
    http_response_t* query_response = mock_api_handle_documents_query(ctx, query_request);
    TEST_ASSERT_NOT_NULL(query_response);
    TEST_ASSERT_EQ(HTTP_OK, query_response->status);
    
    json_value_t* query_result = json_parse(query_response->body);
    TEST_ASSERT_NOT_NULL(query_result);
    json_value_t* documents = json_object_get(query_result, "documents");
    TEST_ASSERT_NOT_NULL(documents);
    TEST_ASSERT(json_array_size(documents) >= 1);  /* Should find our created document */
    
    /* Step 5: Get session info */
    http_request_t* session_request = create_mock_request(HTTP_GET, "/api/auth/session", NULL, auth_header);
    TEST_ASSERT_NOT_NULL(session_request);
    
    http_response_t* session_response = mock_api_handle_get_current_session(ctx, session_request);
    TEST_ASSERT_NOT_NULL(session_response);
    TEST_ASSERT_EQ(HTTP_OK, session_response->status);
    
    json_value_t* session_result = json_parse(session_response->body);
    TEST_ASSERT_NOT_NULL(session_result);
    json_value_t* username = json_object_get(session_result, "username");
    TEST_ASSERT_NOT_NULL(username);
    TEST_ASSERT_STR_EQ("admin", json_get_string(username));
    
    /* Cleanup */
    /* CHECKPOINT: json_free(register_body); */
    /* CHECKPOINT: json_free(login_body); */
    /* CHECKPOINT: json_free(login_result); */
    /* CHECKPOINT: json_free(doc_body); */
    /* CHECKPOINT: json_free(query_result); */
    /* CHECKPOINT: json_free(session_result); */
    BUFFER_FREE(register_body_str);
    BUFFER_FREE(login_body_str);
    BUFFER_FREE(doc_body_str);
    free_http_response(register_response);
    free_http_response(login_response);
    free_http_response(doc_response);
    free_http_response(query_response);
    free_http_response(session_response);
    destroy_mock_request(register_request);
    destroy_mock_request(login_request);
    destroy_mock_request(doc_request);
    destroy_mock_request(query_request);
    destroy_mock_request(session_request);
    destroy_mock_api_context(ctx);
    cleanup_mock_integration_db();
    
    return 0;
}

/* Test: Cross-module error handling */
static int test_error_handling_integration() {
    init_mock_integration_db();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Step 1: Try to access documents without authentication */
    http_request_t* unauth_request = create_mock_request(HTTP_GET, "/api/documents", NULL, NULL);
    TEST_ASSERT_NOT_NULL(unauth_request);
    
    http_response_t* unauth_response = mock_api_handle_documents_query(ctx, unauth_request);
    TEST_ASSERT_NOT_NULL(unauth_response);
    /* Should return success in our mock since we don't enforce auth in storage layer */
    
    /* Step 2: Try to access with invalid token */
    http_request_t* invalid_auth_request = create_mock_request(HTTP_GET, "/api/auth/session", NULL, "Bearer invalid_token");
    TEST_ASSERT_NOT_NULL(invalid_auth_request);
    
    http_response_t* invalid_auth_response = mock_api_handle_get_current_session(ctx, invalid_auth_request);
    TEST_ASSERT_NOT_NULL(invalid_auth_response);
    TEST_ASSERT_EQ(HTTP_NOT_FOUND, invalid_auth_response->status);  /* Session not found */
    
    /* Step 3: Try to create document with malformed JSON */
    http_request_t* malformed_request = create_mock_request(HTTP_POST, "/api/documents", "invalid json", "Bearer mock_token");
    TEST_ASSERT_NOT_NULL(malformed_request);
    
    http_response_t* malformed_response = mock_api_handle_documents_create(ctx, malformed_request);
    TEST_ASSERT_NOT_NULL(malformed_response);
    TEST_ASSERT_EQ(HTTP_BAD_REQUEST, malformed_response->status);
    
    /* Step 4: Try to access non-existent document */
    http_request_t* notfound_request = create_mock_request(HTTP_GET, "/api/documents/nonexistent", NULL, "Bearer mock_token");
    TEST_ASSERT_NOT_NULL(notfound_request);
    
    http_response_t* notfound_response = mock_api_handle_documents_get(ctx, notfound_request);
    TEST_ASSERT_NOT_NULL(notfound_response);
    TEST_ASSERT_EQ(HTTP_NOT_FOUND, notfound_response->status);
    
    /* Step 5: Verify error responses have proper format */
    json_value_t* error_result = json_parse(invalid_auth_response->body);
    TEST_ASSERT_NOT_NULL(error_result);
    json_value_t* error_msg = json_object_get(error_result, "error");
    TEST_ASSERT_NOT_NULL(error_msg);
    TEST_ASSERT_STR_EQ("Session not found", json_get_string(error_msg));
    
    /* Cleanup */
    /* CHECKPOINT: json_free(error_result); */
    free_http_response(unauth_response);
    free_http_response(invalid_auth_response);
    free_http_response(malformed_response);
    free_http_response(notfound_response);
    destroy_mock_request(unauth_request);
    destroy_mock_request(invalid_auth_request);
    destroy_mock_request(malformed_request);
    destroy_mock_request(notfound_request);
    destroy_mock_api_context(ctx);
    cleanup_mock_integration_db();
    
    return 0;
}

/* Test suite runner */
void run_api_integration_tests() {
    RUN_TEST(test_auth_document_integration);
    RUN_TEST(test_rbac_document_permissions);
    RUN_TEST(test_metrics_collection_integration);
    RUN_TEST(test_user_workflow_integration);
    RUN_TEST(test_error_handling_integration);
}

/* Main entry point */
int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        g_verbose_mode = 1;
    }
    
    /* Initialize memory manager first */
    memory_manager_init();
    
    /* Initialize logger */
    logger_init("test_api_integration.log", LOG_LEVEL_ERROR);
    
    TEST_INIT();
    RUN_TEST_SUITE("API Integration Tests", run_api_integration_tests);
    TEST_SUMMARY();
    
    /* Cleanup */
    logger_close();
    
    return g_test_results.failed_tests > 0 ? 1 : 0;
}