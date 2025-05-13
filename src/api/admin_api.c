#include "components/api/api.h"
#include "components/core/server.h"
#include "components/database/database.h"
#include "components/rbac/rbac.h"
#include "components/rbac/jwt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Admin test route handler */
http_response_t* api_handle_admin_test(api_context_t* ctx, http_request_t* request) {
    (void)ctx; /* Avoid unused parameter warning */
    (void)request; /* Avoid unused parameter warning */
    printf("Admin test endpoint called\n");

    /* Create detailed response with debug info */
    json_value_t* test_obj = json_create_object();
    json_object_set(test_obj, "success", json_create_boolean(1));
    json_object_set(test_obj, "message", json_create_string("Admin test route is working!"));
    json_object_set(test_obj, "time", json_create_number((double)time(NULL)));

    /* Add server info */
    json_value_t* server_info = json_create_object();
    json_object_set(server_info, "version", json_create_string("1.0.0"));
    json_object_set(server_info, "api_path", json_create_string("/api/admin/test"));

    /* Add the server info to the response */
    json_object_set(test_obj, "server", server_info);

    /* Convert to string */
    char* response_str = json_stringify(test_obj);
    json_free(test_obj);

    /* Create response */
    http_response_t* response = create_http_response(HTTP_OK, response_str, "application/json");

    /* Add explicit CORS headers */
    add_response_header(response, "Access-Control-Allow-Origin: *");
    add_response_header(response, "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS");
    add_response_header(response, "Access-Control-Allow-Headers: Content-Type, Authorization");

    printf("Test endpoint returning response\n");
    return response;
}

/* Admin login handler */
http_response_t* api_handle_admin_login(api_context_t* ctx, http_request_t* request) {
    printf("Admin login called\n");

    if (!ctx || !request) {
        printf("Invalid context or request\n");
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }

    /* Check for missing body */
    if (!request->body) {
        printf("Missing request body\n");
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Missing request body\"}", "application/json");
    }

    /* Log received body */
    printf("Received login request body: %s\n", request->body);

    /* Parse request body */
    json_value_t* body = json_parse(request->body);
    if (!body || body->type != JSON_OBJECT) {
        if (body) json_free(body);
        printf("Invalid JSON body\n");
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Invalid request body\"}", "application/json");
    }

    /* Extract username and password */
    json_value_t* username_val = json_object_get(body, "username");
    json_value_t* password_val = json_object_get(body, "password");

    if (!username_val || username_val->type != JSON_STRING ||
        !password_val || password_val->type != JSON_STRING) {
        json_free(body);
        printf("Missing username or password fields\n");
        return create_http_response(HTTP_BAD_REQUEST,
                                  "{\"error\":\"Username and password required\"}", "application/json");
    }

    const char* username = username_val->value.string;
    const char* password = password_val->value.string;

    printf("Login attempt: username=%s\n", username);

    /* For demo purposes, just check for hardcoded admin credentials */
    /* In a real application, you'd use the RBAC system with proper roles */
    if (strcmp(username, "admin") != 0 || strcmp(password, "admin") != 0) {
        json_free(body);
        printf("Invalid credentials\n");
        return create_http_response(HTTP_UNAUTHORIZED,
                                  "{\"error\":\"Invalid credentials\"}", "application/json");
    }

    /* User authenticated, generate token */
    char* token = generate_admin_auth_token(username);
    if (!token) {
        json_free(body);
        printf("Failed to generate token\n");
        return create_http_response(HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"Failed to generate token\"}", "application/json");
    }

    printf("Generated token for %s\n", username);

    /* Create success response */
    json_value_t* response_obj = json_create_object();
    json_object_set(response_obj, "success", json_create_boolean(1));
    json_object_set(response_obj, "message", json_create_string("Login successful"));
    json_object_set(response_obj, "username", json_create_string(username));
    json_object_set(response_obj, "token", json_create_string(token)); /* Add token to the response for client-side storage */

    /* Serialize response */
    char* response_str = json_stringify(response_obj);
    json_free(response_obj);
    json_free(body);

    printf("Login successful, sending response\n");

    /* Create HTTP response with auth cookie */
    http_response_t* response = create_http_response(HTTP_OK, response_str, "application/json");
    response = create_auth_response(response, token);

    /* Add explicit CORS headers to ensure browser accepts the response */
    add_response_header(response, "Access-Control-Allow-Origin: *");
    add_response_header(response, "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS");
    add_response_header(response, "Access-Control-Allow-Headers: Content-Type, Authorization");
    add_response_header(response, "Access-Control-Allow-Credentials: true");

    /* Free token */
    free(token);

    return response;
}