/**
 * @file test_api_auth.c
 * @brief Unit tests for API authentication module
 */

#include "test_framework.h"
#include "api/api_auth.h"
#include "rbac/rbac_db.h"
#include "rbac/jwt.h"
#include "utils/memory_manager.h"
#include "utils/logger.h"
#include "core/server.h"
#include <time.h>

/* External globals */
extern logger_config_t* g_logger;

/* Mock user database */
typedef struct {
    json_value_t* users;     /* Array of users */
    json_value_t* sessions;  /* Array of active sessions */
    json_value_t* documents; /* Array of all documents */
    int next_id;
} mock_auth_db_t;

static mock_auth_db_t* g_mock_auth_db = NULL;

/* Initialize mock auth database */
static void init_mock_auth_db() {
    g_mock_auth_db = (mock_auth_db_t*)BUFFER_ALLOC(sizeof(mock_auth_db_t));
    g_mock_auth_db->users = json_create_array();
    g_mock_auth_db->sessions = json_create_array();
    g_mock_auth_db->documents = json_create_array();
    g_mock_auth_db->next_id = 1;
    
    /* Add test user */
    json_value_t* user = json_create_object();
    json_object_set(user, "uuid", json_create_string("user-1"));
    json_object_set(user, "username", json_create_string("testuser"));
    json_object_set(user, "password_hash", json_create_string("hashed_password123")); /* Mock hash */
    json_object_set(user, "email", json_create_string("test@example.com"));
    json_object_set(user, "type", json_create_string("user"));
    json_object_set(user, "library", json_create_string("default"));
    json_array_append(g_mock_auth_db->users, user);
}

/* Clean up mock auth database */
static void cleanup_mock_auth_db() {
    if (g_mock_auth_db) {
        /* CHECKPOINT: json_free(g_mock_auth_db->users); */
        /* CHECKPOINT: json_free(g_mock_auth_db->sessions); */
        /* CHECKPOINT: json_free(g_mock_auth_db->documents); */
        BUFFER_FREE(g_mock_auth_db);
        g_mock_auth_db = NULL;
    }
}

/* Mock RBAC functions */
json_value_t* rbac_db_get_user_by_name(rbac_system_t* rbac, const char* username, const char* library) {
    (void)rbac;  /* Unused parameter */
    if (!g_mock_auth_db || !username) return NULL;
    
    for (size_t i = 0; i < json_array_size(g_mock_auth_db->users); i++) {
        json_value_t* user = json_array_get(g_mock_auth_db->users, i);
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
    /* Simple mock - in real implementation this would use PBKDF2 */
    if (!password || !hash) return 0;
    return strcmp(password, "password123") == 0 && strcmp(hash, "hashed_password123") == 0;
}

rbac_user_t* rbac_db_create_user(database_t* db, const char* username, const char* password) {
    (void)db;  /* Unused parameter */
    if (!g_mock_auth_db || !username || !password) return NULL;
    
    /* Check if user already exists */
    json_value_t* existing = rbac_db_get_user_by_name(NULL, username, "default");
    if (existing) {
        /* CHECKPOINT: json_free(existing); */
        return NULL;
    }
    
    /* Create new user */
    json_value_t* user = json_create_object();
    char id[32];
    snprintf(id, sizeof(id), "user-%zu", json_array_size(g_mock_auth_db->users) + 1);
    
    json_object_set(user, "uuid", json_create_string(id));
    json_object_set(user, "username", json_create_string(username));
    json_object_set(user, "password_hash", json_create_string("hashed_newpass")); /* Mock hash */
    json_object_set(user, "type", json_create_string("user"));
    json_object_set(user, "library", json_create_string("default"));
    
    /* Add timestamp */
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(user, "created_at", json_create_string(timestamp));
    
    json_array_append(g_mock_auth_db->users, json_deep_copy(user));
    
    /* Return as rbac_user_t* (same pointer for mock) */
    return (rbac_user_t*)user;
}

/* Mock JWT functions */
char* jwt_create_token_pair(const char* secret, const char* user_id, const char* username, json_value_t** response_json) {
    (void)secret;  /* Unused parameter */
    /* Create response object */
    json_value_t* response = json_create_object();
    
    /* Create mock access token */
    char access_token[256];
    snprintf(access_token, sizeof(access_token), "mock_jwt_token_%s_%s_3600", user_id, username);
    json_object_set(response, "access_token", json_create_string(access_token));
    
    /* Create mock refresh token */
    char refresh_token[256];
    snprintf(refresh_token, sizeof(refresh_token), "mock_refresh_token_%s_%s_86400", user_id, username);
    json_object_set(response, "refresh_token", json_create_string(refresh_token));
    
    /* Set expiry times */
    json_object_set(response, "access_token_expires_in", json_create_integer(3600));
    json_object_set(response, "refresh_token_expires_in", json_create_integer(86400));
    
    /* Add user info */
    json_value_t* user_info = json_create_object();
    json_object_set(user_info, "id", json_create_string(user_id));
    json_object_set(user_info, "username", json_create_string(username));
    json_object_set(response, "user", user_info);
    
    /* Return the response */
    if (response_json) {
        *response_json = response;
    }
    
    return json_stringify(response);
}

jwt_token_t* jwt_decode(const char* token_str) {
    if (!token_str) return NULL;
    
    /* Mock verification - check if it starts with our mock prefix */
    if (strncmp(token_str, "mock_jwt_token_", 15) != 0 && 
        strncmp(token_str, "mock_refresh_token_", 19) != 0) {
        return NULL;
    }
    
    /* Create mock token */
    jwt_token_t* token = (jwt_token_t*)BUFFER_ALLOC(sizeof(jwt_token_t));
    memset(token, 0, sizeof(jwt_token_t));
    
    /* Create header */
    token->header = (jwt_header_t*)BUFFER_ALLOC(sizeof(jwt_header_t));
    token->header->alg = strdup("HS256");
    token->header->typ = strdup("JWT");
    
    /* Create payload */
    token->payload = (jwt_payload_t*)BUFFER_ALLOC(sizeof(jwt_payload_t));
    memset(token->payload, 0, sizeof(jwt_payload_t));
    
    /* Extract user_id from mock token */
    const char* p = strstr(token_str, "_token_") + 7;  /* Skip to after "_token_" */
    const char* user_id_start = p;
    while (*p && *p != '_') p++;
    
    if (*p == '_') {
        size_t len = p - user_id_start;
        token->payload->sub = (char*)BUFFER_ALLOC(len + 1);
        strncpy(token->payload->sub, user_id_start, len);
        token->payload->sub[len] = '\0';
    }
    
    /* Set other required fields */
    token->payload->iss = strdup("jdbx-auth");
    token->payload->aud = strdup("jdbx-api");
    token->payload->exp = time(NULL) + 3600;  /* 1 hour from now */
    token->payload->iat = time(NULL);
    
    /* Add claims object with user info */
    token->payload->claims = json_create_object();
    json_object_set(token->payload->claims, "username", json_create_string("testuser"));
    json_object_set(token->payload->claims, "library", json_create_string("default"));
    
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
            if (token->payload->iss) BUFFER_FREE(token->payload->iss);
            if (token->payload->sub) BUFFER_FREE(token->payload->sub);
            if (token->payload->aud) BUFFER_FREE(token->payload->aud);
            if (token->payload->jti) BUFFER_FREE(token->payload->jti);
            /* CHECKPOINT: json_free(token->payload->claims); */
            BUFFER_FREE(token->payload);
        }
        if (token->signature) BUFFER_FREE(token->signature);
        if (token->token_str) free(token->token_str);
        BUFFER_FREE(token);
    }
}

/* Mock session functions */
char* rbac_db_create_session(database_t* db, const char* user_id, const char* token, 
                            time_t expires_at, const char* ip_address, const char* user_agent) {
    (void)db;  /* Unused parameter */
    if (!g_mock_auth_db || !user_id || !token) return NULL;
    
    
    json_value_t* session = json_create_object();
    char id[32];
    snprintf(id, sizeof(id), "session-%zu", json_array_size(g_mock_auth_db->sessions) + 1);
    
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
    
    /* Add timestamps */
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(session, "created_at", json_create_string(timestamp));
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&expires_at));
    json_object_set(session, "expires_at", json_create_string(timestamp));
    
    json_array_append(g_mock_auth_db->sessions, json_deep_copy(session));
    
    /* Also add to documents array so db_query_documents can find it */
    json_array_append(g_mock_auth_db->documents, json_deep_copy(session));
    
    /* CHECKPOINT: json_free(session); */
    
    /* Return the session ID */
    return strdup(id);
}

int rbac_db_invalidate_session(database_t* db, const char* session_id) {
    (void)db;  /* Unused parameter */
    if (!g_mock_auth_db || !session_id) return 0;
    
    for (size_t i = 0; i < json_array_size(g_mock_auth_db->sessions); i++) {
        json_value_t* session = json_array_get(g_mock_auth_db->sessions, i);
        json_value_t* id = json_object_get(session, "uuid");
        
        if (id && id->type == JSON_STRING && strcmp(id->value.string, session_id) == 0) {
            json_array_remove(g_mock_auth_db->sessions, i);
            return 1;
        }
    }
    
    return 0;
}

int rbac_db_invalidate_sessions_by_token(database_t* db, const char* token) {
    (void)db;  /* Unused parameter */
    if (!g_mock_auth_db || !token) return 0;
    
    int count = 0;
    size_t i = 0;
    while (i < json_array_size(g_mock_auth_db->sessions)) {
        json_value_t* session = json_array_get(g_mock_auth_db->sessions, i);
        json_value_t* tok = json_object_get(session, "token");
        
        if (tok && tok->type == JSON_STRING && strcmp(tok->value.string, token) == 0) {
            json_array_remove(g_mock_auth_db->sessions, i);
            count++;
            /* Don't increment i since we removed an element */
        } else {
            i++;
        }
    }
    
    return count > 0;
}

/* Mock JWT cache functions */
void jwt_cache_invalidate_token(const char* token) {
    (void)token;  /* Unused parameter */
    /* Mock implementation - no-op for tests */
}

void jwt_cache_invalidate_user(const char* user_id) {
    (void)user_id;  /* Unused parameter */
    /* Mock implementation - no-op for tests */
}

/* Mock rbac_db_add_user_to_role */
int rbac_db_add_user_to_role(database_t* db, const char* user_id, const char* role_id) {
    (void)db;       /* Unused parameter */
    (void)user_id;  /* Unused parameter */
    (void)role_id;  /* Unused parameter */
    /* Mock implementation - always succeeds */
    return 1;
}

/* Mock virtual_query - the main function used by logout/session functions */
json_value_t* virtual_query(database_t* db, const char* doc_type, const char* library, const char* collection, json_value_t* query) {
    (void)db;          /* Unused parameter */
    (void)doc_type;    /* Unused parameter */
    (void)library;     /* Unused parameter */
    (void)collection;  /* Unused parameter */
    
    if (!g_mock_auth_db || !query) {
        return NULL;
    }
    
    
    json_value_t* result = json_create_object();
    json_value_t* docs = json_create_array();
    
    /* Search through sessions array for matches */
    for (size_t i = 0; i < json_array_size(g_mock_auth_db->sessions); i++) {
        json_value_t* session = json_array_get(g_mock_auth_db->sessions, i);
        int matches = 1;
        
        /* Check if query criteria match */
        if (query && query->type == JSON_OBJECT) {
            const char* key;
            json_value_t* val;
            json_object_foreach(query, key, val) {
                json_value_t* session_val = json_object_get(session, key);
                if (!json_values_equal(val, session_val)) {
                    matches = 0;
                    break;
                }
            }
        }
        
        if (matches) {
            json_array_append(docs, json_deep_copy(session));
        }
    }
    
    json_object_set(result, "documents", docs);
    json_object_set(result, "count", json_create_integer(json_array_size(docs)));
    
    return result;
}

/* Mock virtual_get_session_by_token - used by get_current_session */
json_value_t* virtual_get_session_by_token(database_t* db, const char* token) {
    (void)db;  /* Unused parameter */
    if (!g_mock_auth_db || !token) return NULL;
    
    
    /* Search in sessions */
    for (size_t i = 0; i < json_array_size(g_mock_auth_db->sessions); i++) {
        json_value_t* session = json_array_get(g_mock_auth_db->sessions, i);
        json_value_t* tok = json_object_get(session, "token");
        if (tok && tok->type == JSON_STRING && strcmp(tok->value.string, token) == 0) {
            return json_deep_copy(session);
        }
    }
    
    return NULL;
}

/* Mock virtual layer functions */
json_value_t* virtual_get(database_t* db, const char* uuid) {
    (void)db;  /* Unused parameter */
    if (!g_mock_auth_db || !uuid) return NULL;
    
    printf("\n  DEBUG: virtual_get looking for UUID: %s\n", uuid);
    printf("  DEBUG: sessions array size: %zu\n", json_array_size(g_mock_auth_db->sessions));
    
    /* Search in sessions */
    for (size_t i = 0; i < json_array_size(g_mock_auth_db->sessions); i++) {
        json_value_t* session = json_array_get(g_mock_auth_db->sessions, i);
        json_value_t* id = json_object_get(session, "uuid");
        if (id && id->type == JSON_STRING) {
            printf("  DEBUG: comparing with session UUID: %s\n", id->value.string);
            if (strcmp(id->value.string, uuid) == 0) {
                printf("  DEBUG: Found matching session!\n");
                return json_deep_copy(session);
            }
        }
    }
    
    /* Search in users */
    for (size_t i = 0; i < json_array_size(g_mock_auth_db->users); i++) {
        json_value_t* user = json_array_get(g_mock_auth_db->users, i);
        json_value_t* id = json_object_get(user, "uuid");
        if (id && id->type == JSON_STRING && strcmp(id->value.string, uuid) == 0) {
            return json_deep_copy(user);
        }
    }
    
    printf("  DEBUG: No matching document found for UUID: %s\n", uuid);
    return NULL;
}

json_value_t* virtual_update(database_t* db, const char* uuid, json_value_t* updates) {
    (void)db;  /* Unused parameter */
    if (!g_mock_auth_db || !uuid || !updates) return NULL;
    
    /* Update sessions */
    for (size_t i = 0; i < json_array_size(g_mock_auth_db->sessions); i++) {
        json_value_t* session = json_array_get(g_mock_auth_db->sessions, i);
        json_value_t* id = json_object_get(session, "uuid");
        if (id && id->type == JSON_STRING && strcmp(id->value.string, uuid) == 0) {
            /* Update fields */
            const char* key;
            json_value_t* val;
            json_object_foreach(updates, key, val) {
                json_object_set(session, key, json_deep_copy(val));
            }
            return json_deep_copy(session);
        }
    }
    
    /* Update users */
    for (size_t i = 0; i < json_array_size(g_mock_auth_db->users); i++) {
        json_value_t* user = json_array_get(g_mock_auth_db->users, i);
        json_value_t* id = json_object_get(user, "uuid");
        if (id && id->type == JSON_STRING && strcmp(id->value.string, uuid) == 0) {
            /* Update fields */
            const char* key;
            json_value_t* val;
            json_object_foreach(updates, key, val) {
                json_object_set(user, key, json_deep_copy(val));
            }
            return json_deep_copy(user);
        }
    }
    
    return NULL;
}

/* Mock RBAC permission checking */
int rbac_check_permission(rbac_system_t* rbac, const char* user_id, rbac_resource_type_t resource_type, 
                         const char* resource_id, rbac_permission_t permission) {
    (void)rbac;          /* Unused parameter */
    (void)user_id;       /* Unused parameter */
    (void)resource_type; /* Unused parameter */
    (void)resource_id;   /* Unused parameter */
    (void)permission;    /* Unused parameter */
    /* Mock implementation - always allow for tests */
    return 1;
}

/* Mock API helper */
char* api_extract_token(http_request_t* request) {
    if (!request || !request->authorization) return NULL;
    
    const char* auth_str = request->authorization;
    if (strncmp(auth_str, "Bearer ", 7) != 0) return NULL;
    
    return strdup(auth_str + 7);
}


/* Mock JWT create/encode */
jwt_token_t* jwt_create(const char* secret) {
    (void)secret;  /* Unused parameter */
    jwt_token_t* token = (jwt_token_t*)BUFFER_ALLOC(sizeof(jwt_token_t));
    memset(token, 0, sizeof(jwt_token_t));
    
    token->header = (jwt_header_t*)BUFFER_ALLOC(sizeof(jwt_header_t));
    token->header->alg = strdup("HS256");
    token->header->typ = strdup("JWT");
    
    token->payload = (jwt_payload_t*)BUFFER_ALLOC(sizeof(jwt_payload_t));
    memset(token->payload, 0, sizeof(jwt_payload_t));
    
    return token;
}

char* jwt_encode(jwt_token_t* token, const char* secret) {
    (void)secret;  /* Unused parameter */
    if (!token || !token->payload) return NULL;
    
    /* Create mock encoded token */
    char encoded[256];
    const char* sub = token->payload->sub ? token->payload->sub : "unknown";
    snprintf(encoded, sizeof(encoded), "mock_jwt_token_%s_encoded", sub);
    
    return strdup(encoded);
}


/* Mock password functions */
int verify_password(const char* password, const char* hash) {
    /* Simple mock verification */
    return rbac_db_verify_password(password, hash);
}

char* hash_password(const char* password) {
    (void)password;  /* Unused parameter */
    /* Return mock hash */
    return strdup("hashed_password_new");
}

/* Mock admin/bootstrap functions */
int create_default_admin_role(database_t* db, char** admin_role_id_out) {
    (void)db;  /* Unused parameter */
    if (admin_role_id_out) {
        *admin_role_id_out = strdup("admin-role-1");
    }
    return 1;
}

json_value_t* create_default_admin_user(database_t* db, const char* username, const char* password_hash,
                                       const char* email, const char* role_id) {
    (void)db;            /* Unused parameter */
    (void)password_hash; /* Unused parameter */
    (void)role_id;       /* Unused parameter */
    
    json_value_t* admin = json_create_object();
    json_object_set(admin, "uuid", json_create_string("admin-user-1"));
    json_object_set(admin, "username", json_create_string(username));
    json_object_set(admin, "email", json_create_string(email ? email : "admin@example.com"));
    json_object_set(admin, "type", json_create_string("user"));
    json_object_set(admin, "library", json_create_string("system"));
    
    return admin;
}

int create_system_actors(database_t* db) {
    (void)db;  /* Unused parameter */
    return 1;
}

void config_load_bootstrap_admin_credentials(char** username_out, char** password_out, char** email_out) {
    if (username_out) *username_out = strdup("admin");
    if (password_out) *password_out = strdup("secure123456789");
    if (email_out) *email_out = strdup("admin@example.com");
}

/* Mock database functions */
json_value_t* db_insert_document(database_t* db, const char* library, const char* collection, json_value_t* document) {
    (void)db;         /* Unused parameter */
    (void)library;    /* Unused parameter */
    (void)collection; /* Unused parameter */
    
    if (!g_mock_auth_db || !document) return NULL;
    
    /* Generate ID if not present */
    if (!json_object_get(document, "uuid")) {
        char id[32];
        snprintf(id, sizeof(id), "doc-%d", g_mock_auth_db->next_id++);
        json_object_set(document, "uuid", json_create_string(id));
    }
    
    /* Add timestamp */
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(document, "created_at", json_create_string(timestamp));
    
    /* Store document */
    json_value_t* doc_copy = json_deep_copy(document);
    json_array_append(g_mock_auth_db->documents, doc_copy);
    
    /* Return the document with ID */
    return json_deep_copy(doc_copy);
}

json_value_t* db_query_documents(database_t* db, const char* library, const char* collection, json_value_t* query) {
    (void)db;         /* Unused parameter */
    (void)library;    /* Unused parameter */
    (void)collection; /* Unused parameter */
    
    if (!g_mock_auth_db) return NULL;
    
    printf("\n  DEBUG: db_query_documents called for library=%s, collection=%s\n", 
           library ? library : "NULL", collection ? collection : "NULL");
    if (query) {
        char* query_str = json_stringify(query);
        printf("  DEBUG: query: %s\n", query_str);
        BUFFER_FREE(query_str);
    }
    
    json_value_t* result = json_create_object();
    json_value_t* docs = json_create_array();
    
    /* Simple query implementation - match all fields in query */
    for (size_t i = 0; i < json_array_size(g_mock_auth_db->documents); i++) {
        json_value_t* doc = json_array_get(g_mock_auth_db->documents, i);
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

/* Mock virtual user functions */
json_value_t* virtual_query_users(database_t* db, const char* library, json_value_t* filters) {
    (void)db;       /* Unused parameter */
    (void)library;  /* Unused parameter */
    if (!g_mock_auth_db) return NULL;
    
    json_value_t* result = json_create_object();
    json_value_t* docs = json_create_array();
    
    /* Apply filters if provided */
    for (size_t i = 0; i < json_array_size(g_mock_auth_db->users); i++) {
        json_value_t* user = json_array_get(g_mock_auth_db->users, i);
        int matches = 1;
        
        if (filters && filters->type == JSON_OBJECT) {
            const char* key;
            json_value_t* val;
            json_object_foreach(filters, key, val) {
                json_value_t* user_val = json_object_get(user, key);
                if (!json_values_equal(val, user_val)) {
                    matches = 0;
                    break;
                }
            }
        }
        
        if (matches) {
            json_array_append(docs, json_deep_copy(user));
        }
    }
    
    json_object_set(result, "documents", docs);
    json_object_set(result, "count", json_create_integer(json_array_size(docs)));
    
    return result;
}

json_value_t* virtual_create_user(database_t* db, const char* username, const char* password_hash,
                                 const char* email, const char* library) {
    (void)db;            /* Unused parameter */
    (void)password_hash; /* Unused parameter */
    (void)library;       /* Unused parameter */
    
    if (!g_mock_auth_db || !username) return NULL;
    
    /* Create new user */
    json_value_t* user = json_create_object();
    char id[32];
    snprintf(id, sizeof(id), "user-%zu", json_array_size(g_mock_auth_db->users) + 1);
    
    json_object_set(user, "uuid", json_create_string(id));
    json_object_set(user, "username", json_create_string(username));
    json_object_set(user, "password_hash", json_create_string("hashed_newpass"));
    if (email) {
        json_object_set(user, "email", json_create_string(email));
    }
    json_object_set(user, "type", json_create_string("user"));
    json_object_set(user, "library", json_create_string("default"));
    
    /* Add timestamp */
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    json_object_set(user, "created_at", json_create_string(timestamp));
    
    json_array_append(g_mock_auth_db->users, json_deep_copy(user));
    
    return user;
}

/* Test: User login with valid credentials */
static int test_auth_login_success() {
    init_mock_auth_db();
    
    mock_auth_context_t auth = {
        .token = "test-jwt-secret"
    };
    api_context_t* ctx = create_mock_api_context(&auth);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Create login request */
    json_value_t* creds = json_create_object();
    json_object_set(creds, "username", json_create_string("testuser"));
    json_object_set(creds, "password", json_create_string("password123"));
    
    char* body = json_stringify(creds);
    
    http_request_t* request = create_mock_request(HTTP_POST, "/api/auth/login", body, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_login(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    /* Verify response */
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    json_value_t* access_token = json_object_get(result, "access_token");
    TEST_ASSERT_NOT_NULL(access_token);
    TEST_ASSERT(strstr(json_get_string(access_token), "mock_jwt_token_") != NULL);
    
    json_value_t* refresh_token = json_object_get(result, "refresh_token");
    TEST_ASSERT_NOT_NULL(refresh_token);
    TEST_ASSERT(strstr(json_get_string(refresh_token), "mock_refresh_token_") != NULL);
    
    json_value_t* user = json_object_get(result, "user");
    TEST_ASSERT_NOT_NULL(user);
    
    /* Cleanup */
    /* CHECKPOINT: json_free(result); */
    /* CHECKPOINT: json_free(creds); */
    BUFFER_FREE(body);
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_auth_db();
    
    return 0;
}

/* Test: Login with invalid credentials */
static int test_auth_login_invalid() {
    init_mock_auth_db();
    
    mock_auth_context_t auth = {
        .token = "test-jwt-secret"
    };
    api_context_t* ctx = create_mock_api_context(&auth);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Create login request with wrong password */
    json_value_t* creds = json_create_object();
    json_object_set(creds, "username", json_create_string("testuser"));
    json_object_set(creds, "password", json_create_string("wrongpassword"));
    
    char* body = json_stringify(creds);
    
    http_request_t* request = create_mock_request(HTTP_POST, "/api/auth/login", body, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_login(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_UNAUTHORIZED, response->status);
    
    /* Cleanup */
    /* CHECKPOINT: json_free(creds); */
    BUFFER_FREE(body);
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_auth_db();
    
    return 0;
}

/* Test: User registration */
static int test_auth_register() {
    init_mock_auth_db();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Create registration request */
    json_value_t* reg = json_create_object();
    json_object_set(reg, "username", json_create_string("newuser"));
    json_object_set(reg, "password", json_create_string("newpass123"));
    json_object_set(reg, "email", json_create_string("new@example.com"));
    
    char* body = json_stringify(reg);
    
    http_request_t* request = create_mock_request(HTTP_POST, "/api/auth/register", body, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_register(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    
    if (response->status != HTTP_CREATED) {
        printf("\n  Registration failed with status %d\n", response->status);
        printf("  Response body: %s\n", response->body ? response->body : "NULL");
    }
    
    TEST_ASSERT_EQ(HTTP_CREATED, response->status);
    
    /* Verify response */
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    /* Check the actual response format */
    json_value_t* username = json_object_get(result, "username");
    TEST_ASSERT_NOT_NULL(username);
    TEST_ASSERT_STR_EQ("newuser", json_get_string(username));
    
    json_value_t* user_id = json_object_get(result, "user_id");
    TEST_ASSERT_NOT_NULL(user_id);
    
    json_value_t* message = json_object_get(result, "message");
    TEST_ASSERT_NOT_NULL(message);
    
    /* Cleanup */
    /* CHECKPOINT: json_free(result); */
    /* CHECKPOINT: json_free(reg); */
    BUFFER_FREE(body);
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_auth_db();
    
    return 0;
}

/* Test: Register with existing username */
static int test_auth_register_duplicate() {
    init_mock_auth_db();
    
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Try to register existing user */
    json_value_t* reg = json_create_object();
    json_object_set(reg, "username", json_create_string("testuser"));
    json_object_set(reg, "password", json_create_string("somepass"));
    
    char* body = json_stringify(reg);
    
    http_request_t* request = create_mock_request(HTTP_POST, "/api/auth/register", body, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_register(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_CONFLICT, response->status);
    
    /* Cleanup */
    /* CHECKPOINT: json_free(reg); */
    BUFFER_FREE(body);
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_auth_db();
    
    return 0;
}

/* Test: Token refresh */
static int test_auth_token_refresh() {
    init_mock_auth_db();
    
    mock_auth_context_t auth = {
        .token = "test-jwt-secret"
    };
    api_context_t* ctx = create_mock_api_context(&auth);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Create request with valid token */
    http_request_t* request = create_mock_request(HTTP_POST, "/api/auth/refresh", NULL, 
                                                 "Bearer mock_jwt_token_user-1_testuser_3600");
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_token_refresh(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    
    if (response->status != HTTP_OK) {
        printf("\n  Token refresh failed with status %d\n", response->status);
        printf("  Response body: %s\n", response->body ? response->body : "NULL");
    }
    
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    /* Verify new token */
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    json_value_t* access_token = json_object_get(result, "access_token");
    TEST_ASSERT_NOT_NULL(access_token);
    TEST_ASSERT(strstr(json_get_string(access_token), "mock_jwt_token_") != NULL);
    
    /* Cleanup */
    /* CHECKPOINT: json_free(result); */
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_auth_db();
    
    return 0;
}

/* Test: User logout */
static int test_auth_logout() {
    init_mock_auth_db();
    
    mock_auth_context_t auth = {
        .token = "test-jwt-secret"
    };
    api_context_t* ctx = create_mock_api_context(&auth);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Create a session first */
    time_t expires = time(NULL) + 3600;
    char* session_id = rbac_db_create_session(ctx->db, "user-1", 
                                             "mock_jwt_token_user-1_testuser_3600", 
                                             expires, "127.0.0.1", "Test Client");
    TEST_ASSERT_NOT_NULL(session_id);
    
    /* Create logout request */
    json_value_t* body_obj = json_create_object();
    json_object_set(body_obj, "session_id", json_create_string(session_id));
    char* body = json_stringify(body_obj);
    
    http_request_t* request = create_mock_request(HTTP_POST, "/api/auth/logout", body,
                                                 "Bearer mock_jwt_token_user-1_testuser_3600");
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_logout(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    
    if (response->status != HTTP_OK) {
        printf("\n  Logout failed with status %d\n", response->status);
        printf("  Response body: %s\n", response->body ? response->body : "NULL");
    }
    
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    /* Verify session was deleted */
    TEST_ASSERT_EQ(0, json_array_size(g_mock_auth_db->sessions));
    
    /* Cleanup */
    free(session_id);
    /* CHECKPOINT: json_free(body_obj); */
    BUFFER_FREE(body);
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_auth_db();
    
    return 0;
}

/* Test: Get current session */
static int test_auth_get_session() {
    init_mock_auth_db();
    
    mock_auth_context_t auth = {
        .token = "test-jwt-secret"
    };
    api_context_t* ctx = create_mock_api_context(&auth);
    TEST_ASSERT_NOT_NULL(ctx);
    
    /* Create a session first */
    time_t expires = time(NULL) + 3600;
    char* session_id = rbac_db_create_session(ctx->db, "user-1", 
                                             "mock_jwt_token_user-1_testuser_3600", 
                                             expires, "127.0.0.1", "Test Client");
    TEST_ASSERT_NOT_NULL(session_id);
    
    /* Create request with auth token */
    http_request_t* request = create_mock_request(HTTP_GET, "/api/auth/session", NULL,
                                                 "Bearer mock_jwt_token_user-1_testuser_3600");
    TEST_ASSERT_NOT_NULL(request);
    
    /* Call handler */
    http_response_t* response = api_handle_get_current_session(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status);
    
    /* Verify response contains user info */
    json_value_t* result = json_parse(response->body);
    TEST_ASSERT_NOT_NULL(result);
    
    
    json_value_t* user_uuid = json_object_get(result, "user_uuid");
    TEST_ASSERT_NOT_NULL(user_uuid);
    TEST_ASSERT_STR_EQ("user-1", json_get_string(user_uuid));
    
    /* Cleanup */
    free(session_id);
    /* CHECKPOINT: json_free(result); */
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    cleanup_mock_auth_db();
    
    return 0;
}

/* Test: Invalid request handling */
static int test_auth_invalid_requests() {
    api_context_t* ctx = create_mock_api_context(NULL);
    
    /* Test: NULL context */
    http_request_t* request = create_mock_request(HTTP_POST, "/api/auth/login", "{}", NULL);
    http_response_t* response = api_handle_login(NULL, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_BAD_REQUEST, response->status);
    free_http_response(response);
    
    /* Test: NULL request */
    response = api_handle_login(ctx, NULL);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_BAD_REQUEST, response->status);
    free_http_response(response);
    
    /* Test: Invalid JSON */
    destroy_mock_request(request);
    request = create_mock_request(HTTP_POST, "/api/auth/login", "invalid json", NULL);
    response = api_handle_login(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_BAD_REQUEST, response->status);
    free_http_response(response);
    
    /* Test: Missing required fields */
    destroy_mock_request(request);
    request = create_mock_request(HTTP_POST, "/api/auth/login", "{\"username\":\"test\"}", NULL);
    response = api_handle_login(ctx, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_BAD_REQUEST, response->status);
    free_http_response(response);
    
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    
    return 0;
}

/* Test: Performance of auth operations */
static int test_auth_performance_helper() {
    api_context_t* ctx = create_mock_api_context(NULL);
    
    /* Login */
    json_value_t* creds = json_create_object();
    json_object_set(creds, "username", json_create_string("testuser"));
    json_object_set(creds, "password", json_create_string("password123"));
    char* body = json_stringify(creds);
    
    http_request_t* request = create_mock_request(HTTP_POST, "/api/auth/login", body, NULL);
    http_response_t* response = api_handle_login(ctx, request);
    
    /* CHECKPOINT: json_free(creds); */
    BUFFER_FREE(body);
    free_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    
    return 0;
}

static int test_auth_performance() {
    init_mock_auth_db();
    
    perf_test_t perf = {
        .name = "Auth login",
        .iterations = 1000,
        .target_ms = 100.0,  /* 0.1ms per operation */
    };
    
    run_performance_test(&perf, test_auth_performance_helper);
    
    cleanup_mock_auth_db();
    
    return perf.passed ? 0 : 1;
}

/* Test suite runner */
void run_api_auth_tests() {
    RUN_TEST(test_auth_login_success);
    RUN_TEST(test_auth_login_invalid);
    RUN_TEST(test_auth_register);
    RUN_TEST(test_auth_register_duplicate);
    RUN_TEST(test_auth_token_refresh);
    RUN_TEST(test_auth_logout);
    RUN_TEST(test_auth_get_session);
    RUN_TEST(test_auth_invalid_requests);
    RUN_TEST(test_auth_performance);
}

/* Main entry point */
int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        g_verbose_mode = 1;
    }
    
    /* Initialize memory manager first */
    memory_manager_init();
    
    /* Initialize logger */
    logger_init("test_api_auth.log", LOG_LEVEL_ERROR);
    
    TEST_INIT();
    RUN_TEST_SUITE("API Authentication Module Tests", run_api_auth_tests);
    TEST_SUMMARY();
    
    /* Cleanup */
    logger_close();
    
    return g_test_results.failed_tests > 0 ? 1 : 0;
}