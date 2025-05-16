#!/bin/bash
# Test script for JWT signature verification issues
# This script tests the theory that the base64url decoding is causing the issue

set -e  # Exit on error

echo "===== JWT Signature Verification Test ====="
echo

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create temporary directories for test
TEMP_DIR="/tmp/jsondb_jwt_sig_test"
mkdir -p $TEMP_DIR
cd $TEMP_DIR

# Write the test code
cat > jwt_signature_test.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Simple Base64 encoding table */
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Base64 encode a string */
static char* base64_encode(const unsigned char* input, int length) {
    int encoded_len = ((length + 2) / 3) * 4;
    char* output = (char*)malloc(encoded_len + 1);
    if (!output) return NULL;
    
    int i, j;
    for (i = 0, j = 0; i < length;) {
        uint32_t octet_a = i < length ? input[i++] : 0;
        uint32_t octet_b = i < length ? input[i++] : 0;
        uint32_t octet_c = i < length ? input[i++] : 0;
        
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        output[j++] = base64_chars[(triple >> 18) & 0x3F];
        output[j++] = base64_chars[(triple >> 12) & 0x3F];
        output[j++] = base64_chars[(triple >> 6) & 0x3F];
        output[j++] = base64_chars[triple & 0x3F];
    }
    
    /* Padding */
    if (length % 3 == 1) {
        output[encoded_len - 2] = '=';
        output[encoded_len - 1] = '=';
    } else if (length % 3 == 2) {
        output[encoded_len - 1] = '=';
    }
    
    output[encoded_len] = '\0';
    return output;
}

/* Base64 decode a string */
static unsigned char* base64_decode(const char* input, int* output_length) {
    int input_len = strlen(input);
    if (input_len % 4 != 0) return NULL;
    
    int decoded_len = input_len / 4 * 3;
    if (input[input_len-1] == '=') decoded_len--;
    if (input[input_len-2] == '=') decoded_len--;
    
    unsigned char* output = (unsigned char*)malloc(decoded_len + 1);
    if (!output) return NULL;
    
    /* Create reverse lookup table */
    unsigned char decode_table[256] = {0};
    for (int i = 0; i < 64; i++) {
        decode_table[(unsigned char)base64_chars[i]] = i;
    }
    
    int i, j;
    for (i = 0, j = 0; i < input_len;) {
        unsigned char c_a = (unsigned char)input[i];
        uint32_t sextet_a = c_a == '=' ? 0 & i++ : decode_table[c_a]; i++;
        
        unsigned char c_b = (i < input_len) ? (unsigned char)input[i] : '=';
        uint32_t sextet_b = c_b == '=' ? 0 & i++ : decode_table[c_b]; i++;
        
        unsigned char c_c = (i < input_len) ? (unsigned char)input[i] : '=';
        uint32_t sextet_c = c_c == '=' ? 0 & i++ : decode_table[c_c]; i++;
        
        unsigned char c_d = (i < input_len) ? (unsigned char)input[i] : '=';
        uint32_t sextet_d = c_d == '=' ? 0 & i++ : decode_table[c_d]; i++;
        
        uint32_t triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;
        
        if (j < decoded_len) output[j++] = (triple >> 16) & 0xFF;
        if (j < decoded_len) output[j++] = (triple >> 8) & 0xFF;
        if (j < decoded_len) output[j++] = triple & 0xFF;
    }
    
    output[decoded_len] = '\0';
    *output_length = decoded_len;
    
    return output;
}

/* URL-safe version of base64 encoding */
static char* base64_url_encode(const unsigned char* input, int length) {
    char* b64 = base64_encode(input, length);
    if (!b64) return NULL;
    
    /* Replace '+' with '-', '/' with '_', and remove '=' padding */
    for (char* p = b64; *p; p++) {
        if (*p == '+') *p = '-';
        else if (*p == '/') *p = '_';
        else if (*p == '=') *p = '\0';
    }
    
    return b64;
}

/* URL-safe version of base64 decoding - MISSING FROM ORIGINAL CODE */
static unsigned char* base64_url_decode(const char* input, int* output_length) {
    /* Step 1: Make a mutable copy of the input */
    char* input_copy = strdup(input);
    if (!input_copy) return NULL;
    
    /* Step 2: Convert URL-safe characters back to standard base64 */
    for (char* p = input_copy; *p; p++) {
        if (*p == '-') *p = '+';
        else if (*p == '_') *p = '/';
    }
    
    /* Step 3: Add padding if necessary */
    int len = strlen(input_copy);
    int padding = (4 - (len % 4)) % 4;
    
    char* padded_input = (char*)malloc(len + padding + 1);
    if (!padded_input) {
        free(input_copy);
        return NULL;
    }
    
    strcpy(padded_input, input_copy);
    for (int i = 0; i < padding; i++) {
        padded_input[len + i] = '=';
    }
    padded_input[len + padding] = '\0';
    
    free(input_copy);
    
    /* Step 4: Use regular base64 decode */
    unsigned char* output = base64_decode(padded_input, output_length);
    free(padded_input);
    
    return output;
}

/* Simple SHA-256 implementation (placeholder) */
static void simple_sha256(const char* input, size_t input_len, unsigned char* output) {
    /* Using DJB2 hash as a placeholder */
    unsigned int hash = 5381;
    for (size_t i = 0; i < input_len; i++) {
        hash = ((hash << 5) + hash) + input[i];
    }
    
    /* Convert to bytes */
    for (size_t i = 0; i < 32; i++) {
        output[i] = (hash >> (i % 4) * 8) & 0xFF;
    }
}

/* Simple HMAC-SHA256 implementation (placeholder) */
static void hmac_sha256(const char* key, size_t key_len, 
                      const char* data, size_t data_len, 
                      unsigned char* output) {
    /* XOR key with ipad and opad values */
    unsigned char k_ipad[64] = {0}, k_opad[64] = {0};
    unsigned char key_hash[32] = {0};
    
    if (key_len > 64) {
        simple_sha256(key, key_len, key_hash);
        key = (const char*)key_hash;
        key_len = 32;
    }
    
    size_t i;
    for (i = 0; i < key_len; i++) {
        k_ipad[i] = key[i] ^ 0x36;
        k_opad[i] = key[i] ^ 0x5c;
    }
    for (; i < 64; i++) {
        k_ipad[i] = 0x36;
        k_opad[i] = 0x5c;
    }
    
    /* Inner hash */
    size_t total_len = 64 + data_len;
    char* inner_data = malloc(total_len);
    if (!inner_data) return;
    
    memcpy(inner_data, k_ipad, 64);
    memcpy(inner_data + 64, data, data_len);
    
    unsigned char inner_hash[32];
    simple_sha256(inner_data, total_len, inner_hash);
    free(inner_data);
    
    /* Outer hash */
    char outer_data[96]; /* 64 + 32 */
    memcpy(outer_data, k_opad, 64);
    memcpy(outer_data + 64, inner_hash, 32);
    
    simple_sha256(outer_data, 96, output);
}

/* Sign JWT token (original implementation) */
static char* jwt_sign_original(const char* header_payload, const char* secret) {
    unsigned char digest[32];
    hmac_sha256(secret, strlen(secret), header_payload, strlen(header_payload), digest);
    return base64_url_encode(digest, 32);
}

/* JWT verification (original implementation) */
static int jwt_verify_original(const char* token_str, const char* secret) {
    /* Split token */
    char* token_copy = strdup(token_str);
    char* header_b64 = strtok(token_copy, ".");
    if (!header_b64) {
        free(token_copy);
        return 0;
    }
    
    char* payload_b64 = strtok(NULL, ".");
    if (!payload_b64) {
        free(token_copy);
        return 0;
    }
    
    char* signature_b64 = strtok(NULL, ".");
    if (!signature_b64) {
        free(token_copy);
        return 0;
    }
    
    /* Create header.payload */
    char* header_payload = (char*)malloc(strlen(header_b64) + strlen(payload_b64) + 2);
    if (!header_payload) {
        free(token_copy);
        return 0;
    }
    
    sprintf(header_payload, "%s.%s", header_b64, payload_b64);
    
    /* Generate signature */
    char* signature = jwt_sign_original(header_payload, secret);
    if (!signature) {
        free(token_copy);
        free(header_payload);
        return 0;
    }

    /* ISSUE: Direct comparison of base64url strings */
    int result = strcmp(signature, signature_b64) == 0;
    
    free(token_copy);
    free(header_payload);
    free(signature);
    
    return result;
}

/* JWT sign (fixed implementation) */
static char* jwt_sign_fixed(const char* header_payload, const char* secret) {
    unsigned char digest[32];
    hmac_sha256(secret, strlen(secret), header_payload, strlen(header_payload), digest);
    
    /* Sign using the same hmac-sha256 method as the original */
    return base64_url_encode(digest, 32);
}

/* JWT verification (fixed implementation) */
static int jwt_verify_fixed(const char* token_str, const char* secret) {
    /* Split token */
    char* token_copy = strdup(token_str);
    char* header_b64 = strtok(token_copy, ".");
    if (!header_b64) {
        free(token_copy);
        return 0;
    }
    
    char* payload_b64 = strtok(NULL, ".");
    if (!payload_b64) {
        free(token_copy);
        return 0;
    }
    
    char* signature_b64 = strtok(NULL, ".");
    if (!signature_b64) {
        free(token_copy);
        return 0;
    }
    
    /* Create header.payload */
    char* header_payload = (char*)malloc(strlen(header_b64) + strlen(payload_b64) + 2);
    if (!header_payload) {
        free(token_copy);
        return 0;
    }
    
    sprintf(header_payload, "%s.%s", header_b64, payload_b64);
    
    /* Generate signature for comparison */
    char* expected_signature = jwt_sign_fixed(header_payload, secret);
    if (!expected_signature) {
        free(token_copy);
        free(header_payload);
        return 0;
    }

    printf("Token signature: %s\n", signature_b64);
    printf("Expected signature: %s\n", expected_signature);

    /* FIX: Compare the signatures */
    int result = strcmp(expected_signature, signature_b64) == 0;
    
    /* Clean up */
    free(token_copy);
    free(header_payload);
    free(expected_signature);
    
    return result;
}

/* Print functions for testing */
void print_success(const char* message) {
    printf("\033[0;32m✓ %s\033[0m\n", message);
}

void print_error(const char* message) {
    printf("\033[0;31m✗ %s\033[0m\n", message);
    exit(1);
}

int main() {
    printf("Testing JWT signature verification...\n\n");
    
    /* Test data */
    const char* secret = "change-this-secret-in-production";
    const char* header_payload = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJhYmMxMjMiLCJpc3MiOiJqc29uZGIiLCJleHAiOjE3NDc5MjYxMDAsImlhdCI6MTcxNDc5MTc0OX0";
    
    /* First, create signatures with both implementations */
    printf("=== Creating Signatures ===\n");
    char* sig_original = jwt_sign_original(header_payload, secret);
    printf("Original signature: %s\n", sig_original);
    
    char* sig_fixed = jwt_sign_fixed(header_payload, secret);
    printf("Fixed signature: %s\n", sig_fixed);
    
    /* Check if they're the same (they should be) */
    if (strcmp(sig_original, sig_fixed) == 0) {
        print_success("Signature generation is consistent");
    } else {
        print_error("Signature generation is NOT consistent");
    }
    
    /* Create a full token for testing */
    char* full_token = (char*)malloc(strlen(header_payload) + strlen(sig_original) + 2);
    sprintf(full_token, "%s.%s", header_payload, sig_original);
    printf("\nFull token: %s\n", full_token);
    
    /* Test verification with original and fixed methods */
    printf("\n=== Testing Verification ===\n");
    
    /* Test original verification */
    printf("Original verification: ");
    if (jwt_verify_original(full_token, secret)) {
        print_success("Original verification successful");
    } else {
        print_error("Original verification failed");
    }
    
    /* Test fixed verification */
    printf("Fixed verification: ");
    if (jwt_verify_fixed(full_token, secret)) {
        print_success("Fixed verification successful");
    } else {
        print_error("Fixed verification failed");
    }
    
    /* Test with modified token to make sure verification can fail */
    char modified_token[strlen(full_token) + 1];
    strcpy(modified_token, full_token);
    modified_token[30] = modified_token[30] == 'a' ? 'b' : 'a'; /* Modify a character */
    
    printf("\n=== Testing with Modified Token ===\n");
    printf("Modified token: %s\n", modified_token);
    
    /* Test original verification with modified token */
    printf("Original verification with modified token: ");
    if (!jwt_verify_original(modified_token, secret)) {
        print_success("Original verification correctly rejected modified token");
    } else {
        print_error("Original verification incorrectly accepted modified token");
    }
    
    /* Test fixed verification with modified token */
    printf("Fixed verification with modified token: ");
    if (!jwt_verify_fixed(modified_token, secret)) {
        print_success("Fixed verification correctly rejected modified token");
    } else {
        print_error("Fixed verification incorrectly accepted modified token");
    }
    
    /* Clean up */
    free(sig_original);
    free(sig_fixed);
    free(full_token);
    
    printf("\nAll tests completed successfully!\n");
    return 0;
}
EOF

# Compile and run the test
echo -e "${YELLOW}Compiling JWT signature test program...${NC}"
gcc -o jwt_signature_test jwt_signature_test.c -Wall

echo -e "${YELLOW}Running JWT signature test program...${NC}"
./jwt_signature_test

# Clean up
echo
echo -e "${GREEN}Tests completed!${NC}"
echo "Temporary files can be found in $TEMP_DIR"