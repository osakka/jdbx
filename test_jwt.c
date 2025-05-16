#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Simple mocks for the JWT functions
typedef struct {
    char* alg;
    char* typ;
} jwt_header_t;

typedef struct {
    char* iss;
    char* sub;
    char* aud;
    time_t exp;
    time_t nbf;
    time_t iat;
    char* jti;
    void* claims;
} jwt_payload_t;

typedef struct {
    jwt_header_t* header;
    jwt_payload_t* payload;
    char* signature;
    char* token_str;
} jwt_token_t;

// Mock API context structure
typedef struct {
    void* db;
    void* rbac;
    const char* jwt_secret;
    void* transaction_manager;
} api_context_t;

// Function to create API context
api_context_t* api_create_context(void* db, void* rbac, const char* jwt_secret) {
    if (!db || !rbac || !jwt_secret) {
        return NULL;
    }
    
    api_context_t* ctx = (api_context_t*)malloc(sizeof(api_context_t));
    if (!ctx) {
        return NULL;
    }
    
    ctx->db = db;
    ctx->rbac = rbac;
    
    // Before fix:
    // ctx->jwt_secret = jwt_secret;
    
    // After fix: Make a copy of the JWT secret
    ctx->jwt_secret = strdup(jwt_secret);
    if (!ctx->jwt_secret) {
        free(ctx);
        return NULL;
    }
    
    printf("API context created with JWT secret: '%s'\n", ctx->jwt_secret);
    
    return ctx;
}

// Function to free API context
void api_free_context(api_context_t* ctx) {
    if (ctx) {
        // After fix: Free the JWT secret
        if (ctx->jwt_secret) {
            free((void*)ctx->jwt_secret);
        }
        free(ctx);
    }
}

// Simulate changing the original secret to show the fix works
void change_original_secret(char* secret) {
    if (secret) {
        strcpy(secret, "CHANGED_SECRET");
    }
}

int main() {
    // Create a secret
    char original_secret[256] = "ORIGINAL_SECRET_TO_TEST";
    
    printf("Original JWT secret: '%s'\n", original_secret);
    
    // Create an API context with this secret (some dummy pointers for db and rbac)
    api_context_t* ctx = api_create_context((void*)1, (void*)1, original_secret);
    if (!ctx) {
        printf("Failed to create API context\n");
        return 1;
    }
    
    // Now change the original secret to simulate it being modified elsewhere
    change_original_secret(original_secret);
    
    printf("Modified original secret: '%s'\n", original_secret);
    printf("API context secret remains: '%s'\n", ctx->jwt_secret);
    
    // Without the fix, ctx->jwt_secret would now be "CHANGED_SECRET" (which is bad)
    // With the fix, ctx->jwt_secret still contains "ORIGINAL_SECRET_TO_TEST" (which is good)
    
    // Verify the fix worked - the context's secret should still be the original
    if (strcmp(ctx->jwt_secret, "ORIGINAL_SECRET_TO_TEST") == 0) {
        printf("SUCCESS: Fix worked - API context maintains its own copy of the secret!\n");
    } else {
        printf("FAILURE: Fix didn't work - API context secret changed to '%s'\n", ctx->jwt_secret);
    }
    
    // Clean up
    api_free_context(ctx);
    
    return 0;
}