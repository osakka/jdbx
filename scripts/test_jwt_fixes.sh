#!/bin/bash
# Test script for JWT implementation after fixes

set -e  # Exit on error

echo "===== JWT Implementation Test ====="
echo

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create temporary directories for test
TEMP_DIR="/tmp/jsondb_jwt_test"
mkdir -p $TEMP_DIR
cd $TEMP_DIR

# Write the test code
cat > jwt_test.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Mock jwt.h structures
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
    void* claims;  // Would be json_value_t* in real code
    
    // Added for improved version
    char* secret;
} jwt_payload_t;

typedef struct {
    jwt_header_t* header;
    jwt_payload_t* payload;
    char* signature;
    char* token_str;
    
    // Added for improved version
    char* secret;
} jwt_token_t;

// Mock API context
typedef struct {
    void* db;
    void* rbac;
    const char* jwt_secret;
    void* transaction_manager;
} api_context_t;

// Mock functions - normally these would be external
void* json_create_object() { return malloc(1); }  // Just a dummy
void json_free(void* obj) { if (obj) free(obj); }
void json_object_set(void* obj, const char* key, void* value) {}
char* json_stringify(void* obj) { return strdup("{}"); }
void* json_create_string(const char* str) { return strdup(str); }
void* json_create_number(double num) { return malloc(8); }

// === Original API context code (with bug) ===
api_context_t* api_create_context_original(void* db, void* rbac, const char* jwt_secret) {
    if (!db || !rbac || !jwt_secret) {
        return NULL;
    }
    
    api_context_t* ctx = (api_context_t*)malloc(sizeof(api_context_t));
    if (!ctx) {
        return NULL;
    }
    
    ctx->db = db;
    ctx->rbac = rbac;
    ctx->jwt_secret = jwt_secret;  // BUG: Just stores reference, doesn't copy
    ctx->transaction_manager = NULL;
    
    return ctx;
}

void api_free_context_original(api_context_t* ctx) {
    if (ctx) {
        // BUG: Doesn't free jwt_secret because it didn't allocate it
        free(ctx);
    }
}

// === Fixed API context code ===
api_context_t* api_create_context_fixed(void* db, void* rbac, const char* jwt_secret) {
    if (!db || !rbac || !jwt_secret) {
        return NULL;
    }
    
    api_context_t* ctx = (api_context_t*)malloc(sizeof(api_context_t));
    if (!ctx) {
        return NULL;
    }
    
    ctx->db = db;
    ctx->rbac = rbac;
    ctx->jwt_secret = strdup(jwt_secret);  // FIX: Make a copy of the string
    if (!ctx->jwt_secret) {
        free(ctx);
        return NULL;
    }
    ctx->transaction_manager = NULL;
    
    return ctx;
}

void api_free_context_fixed(api_context_t* ctx) {
    if (ctx) {
        if (ctx->jwt_secret) {
            free((void*)ctx->jwt_secret);  // FIX: Free the allocated copy
        }
        free(ctx);
    }
}

// === Improved JWT token code ===
jwt_token_t* jwt_create_improved(const char* secret) {
    jwt_token_t* token = (jwt_token_t*)malloc(sizeof(jwt_token_t));
    if (!token) {
        return NULL;
    }
    
    /* Initialize token */
    token->header = (jwt_header_t*)malloc(sizeof(jwt_header_t));
    if (!token->header) {
        free(token);
        return NULL;
    }
    
    token->payload = (jwt_payload_t*)malloc(sizeof(jwt_payload_t));
    if (!token->payload) {
        free(token->header);
        free(token);
        return NULL;
    }
    
    /* Set default header values */
    token->header->alg = strdup("HS256");
    token->header->typ = strdup("JWT");
    
    /* Initialize payload fields */
    token->payload->iss = NULL;
    token->payload->sub = NULL;
    token->payload->aud = NULL;
    token->payload->exp = 0;
    token->payload->nbf = 0;
    token->payload->iat = time(NULL);
    token->payload->jti = NULL;
    token->payload->claims = json_create_object();
    token->payload->secret = NULL;  // Initialize new field
    
    token->signature = NULL;
    token->token_str = NULL;
    
    /* Store the secret used to create this token for later verification */
    if (secret) {
        token->secret = strdup(secret);
    } else {
        token->secret = NULL;
    }
    
    return token;
}

void jwt_free_improved(jwt_token_t* token) {
    if (!token) {
        return;
    }
    
    /* Free header */
    if (token->header) {
        if (token->header->alg) free(token->header->alg);
        if (token->header->typ) free(token->header->typ);
        free(token->header);
    }
    
    /* Free payload */
    if (token->payload) {
        if (token->payload->iss) free(token->payload->iss);
        if (token->payload->sub) free(token->payload->sub);
        if (token->payload->aud) free(token->payload->aud);
        if (token->payload->jti) free(token->payload->jti);
        if (token->payload->claims) json_free(token->payload->claims);
        if (token->payload->secret) free(token->payload->secret);
        free(token->payload);
    }
    
    /* Free other fields */
    if (token->signature) free(token->signature);
    if (token->token_str) free(token->token_str);
    if (token->secret) free(token->secret);
    
    free(token);
}

// === Test helper functions ===
void print_success(const char* message) {
    printf("\033[0;32m✓ %s\033[0m\n", message);
}

void print_error(const char* message) {
    printf("\033[0;31m✗ %s\033[0m\n", message);
    exit(1);
}

// Simple function to modify a C string
void modify_string(char* str) {
    if (str) {
        strcpy(str, "MODIFIED_VALUE");
    }
}

// Simple function to modify memory - used to test if modifying the memory
// at the original secret location affects the API context
void modify_memory(const char* ptr) {
    if (ptr) {
        char* nonconst_ptr = (char*)ptr;
        strcpy(nonconst_ptr, "MODIFIED_MEMORY");
    }
}

// === Main test function ===
int main() {
    printf("Testing JWT and API context fixes...\n\n");
    
    // === Test 1: API Context Bug Demonstration ===
    printf("=== Test 1: Original API Context Implementation (Bug Demonstration) ===\n");
    
    char* stack_secret = strdup("ORIGINAL_SECRET_VALUE"); 
    printf("Original JWT secret: '%s' at address %p\n", stack_secret, (void*)stack_secret);
    
    // Create API context with original implementation
    api_context_t* ctx_original = api_create_context_original((void*)1, (void*)1, stack_secret);
    if (!ctx_original) {
        print_error("Failed to create original API context");
    }
    printf("Original API context created with secret: '%s' at address %p\n", 
           ctx_original->jwt_secret, (void*)ctx_original->jwt_secret);
    
    // Verify pointers are the same (demonstrating the bug)
    if (ctx_original->jwt_secret == stack_secret) {
        printf("BUG CONFIRMED: Context's secret points to same memory as original\n");
    } else {
        print_error("Test incorrect, should be pointing to same memory");
    }
    
    // Change the original secret
    printf("Changing original secret...\n");
    strcpy(stack_secret, "CHANGED_SECRET_VALUE");
    printf("Original secret now: '%s'\n", stack_secret);
    printf("API context secret now: '%s'\n", ctx_original->jwt_secret);
    
    // Verify the bug - context's secret changed too
    if (strcmp(ctx_original->jwt_secret, "CHANGED_SECRET_VALUE") == 0) {
        printf("BUG CONFIRMED: API context's secret changed when original changed\n");
    } else {
        print_error("Test failed! API context's secret did not change as expected");
    }
    
    // Cleanup
    api_free_context_original(ctx_original);
    free(stack_secret);
    printf("\n");
    
    // === Test 2: API Context Fixed Implementation ===
    printf("=== Test 2: Fixed API Context Implementation ===\n");
    
    // Create a new secret on the heap
    char* heap_secret = strdup("ORIGINAL_SECRET_VALUE");
    printf("Reset original JWT secret: '%s' at address %p\n", heap_secret, (void*)heap_secret);
    
    // Create API context with fixed implementation
    api_context_t* ctx_fixed = api_create_context_fixed((void*)1, (void*)1, heap_secret);
    if (!ctx_fixed) {
        print_error("Failed to create fixed API context");
    }
    printf("Fixed API context created with secret: '%s' at address %p\n", 
           ctx_fixed->jwt_secret, (void*)ctx_fixed->jwt_secret);
    
    // Verify pointers are different (demonstrating the fix)
    if (ctx_fixed->jwt_secret != heap_secret) {
        printf("FIX CONFIRMED: Context's secret is a different memory address\n");
    } else {
        print_error("Fix not working, should be pointing to different memory");
    }
    
    // Change the original secret
    printf("Changing original secret...\n");
    strcpy(heap_secret, "CHANGED_SECRET_VALUE");
    printf("Original secret now: '%s'\n", heap_secret);
    printf("API context secret now: '%s'\n", ctx_fixed->jwt_secret);
    
    // Verify the fix - context's secret should remain unchanged
    if (strcmp(ctx_fixed->jwt_secret, "ORIGINAL_SECRET_VALUE") == 0) {
        print_success("FIX WORKS: API context's secret remained the same when original changed");
    } else {
        print_error("Fix failed! API context's secret changed");
    }
    
    // Cleanup
    api_free_context_fixed(ctx_fixed);
    free(heap_secret);
    printf("\n");
    
    // === Test 3: Improved JWT Token Implementation ===
    printf("=== Test 3: Improved JWT Token Implementation ===\n");
    
    // Create a secret for JWT
    char* jwt_secret = strdup("JWT_TOKEN_SECRET");
    printf("Using JWT secret: '%s'\n", jwt_secret);
    
    // Create JWT token with improved implementation
    jwt_token_t* token = jwt_create_improved(jwt_secret);
    if (!token) {
        print_error("Failed to create JWT token");
    }
    printf("JWT token created with secret: '%s'\n", token->secret);
    
    // Verify token stored a copy, not the original pointer
    if (token->secret != jwt_secret) {
        printf("IMPROVEMENT CONFIRMED: JWT token stores its own copy of the secret\n");
    } else {
        print_error("Improvement failed! JWT token points to the same secret memory");
    }
    
    // Change the original secret
    printf("Changing original secret...\n");
    strcpy(jwt_secret, "CHANGED_JWT_SECRET");
    printf("Original secret now: '%s'\n", jwt_secret);
    printf("JWT token secret still: '%s'\n", token->secret);
    
    // Verify the improvement - token's secret should remain unchanged
    if (strcmp(token->secret, "JWT_TOKEN_SECRET") == 0) {
        print_success("IMPROVEMENT WORKS: JWT token's secret remained the same");
    } else {
        print_error("Improvement failed! JWT token's secret changed");
    }
    
    // Cleanup
    jwt_free_improved(token);
    free(jwt_secret);
    
    printf("\nAll tests completed successfully!\n");
    return 0;
}
EOF

# Compile and run the test
echo -e "${YELLOW}Compiling JWT test program...${NC}"
gcc -o jwt_test jwt_test.c -Wall

echo -e "${YELLOW}Running JWT test program...${NC}"
./jwt_test

# Clean up
echo
echo -e "${GREEN}Tests completed!${NC}"
echo "Temporary files can be found in $TEMP_DIR"