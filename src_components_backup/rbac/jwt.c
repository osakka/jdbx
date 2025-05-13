#include "jsondb/rbac/jwt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

/* Create new JWT token */
jwt_token_t* jwt_create(const char* secret) {
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
    
    token->signature = NULL;
    token->token_str = NULL;
    
    return token;
}

/* Free JWT token */
void jwt_free(jwt_token_t* token) {
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
        free(token->payload);
    }
    
    /* Free other fields */
    if (token->signature) free(token->signature);
    if (token->token_str) free(token->token_str);
    
    free(token);
}

/* Set JWT algorithm */
void jwt_set_algorithm(jwt_token_t* token, const char* alg) {
    if (!token || !token->header || !alg) {
        return;
    }
    
    if (token->header->alg) {
        free(token->header->alg);
    }
    
    token->header->alg = strdup(alg);
}

/* Set JWT issuer */
void jwt_set_issuer(jwt_token_t* token, const char* iss) {
    if (!token || !token->payload || !iss) {
        return;
    }
    
    if (token->payload->iss) {
        free(token->payload->iss);
    }
    
    token->payload->iss = strdup(iss);
}

/* Set JWT subject */
void jwt_set_subject(jwt_token_t* token, const char* sub) {
    if (!token || !token->payload || !sub) {
        return;
    }
    
    if (token->payload->sub) {
        free(token->payload->sub);
    }
    
    token->payload->sub = strdup(sub);
}

/* Set JWT audience */
void jwt_set_audience(jwt_token_t* token, const char* aud) {
    if (!token || !token->payload || !aud) {
        return;
    }
    
    if (token->payload->aud) {
        free(token->payload->aud);
    }
    
    token->payload->aud = strdup(aud);
}

/* Set JWT expiration */
void jwt_set_expiration(jwt_token_t* token, time_t exp) {
    if (!token || !token->payload) {
        return;
    }
    
    token->payload->exp = exp;
}

/* Set JWT not before */
void jwt_set_not_before(jwt_token_t* token, time_t nbf) {
    if (!token || !token->payload) {
        return;
    }
    
    token->payload->nbf = nbf;
}

/* Set JWT issued at */
void jwt_set_issued_at(jwt_token_t* token, time_t iat) {
    if (!token || !token->payload) {
        return;
    }
    
    token->payload->iat = iat;
}

/* Set JWT ID */
void jwt_set_jwt_id(jwt_token_t* token, const char* jti) {
    if (!token || !token->payload || !jti) {
        return;
    }
    
    if (token->payload->jti) {
        free(token->payload->jti);
    }
    
    token->payload->jti = strdup(jti);
}

/* Add claim to JWT */
void jwt_add_claim(jwt_token_t* token, const char* key, json_value_t* value) {
    if (!token || !token->payload || !token->payload->claims || !key || !value) {
        return;
    }
    
    json_object_set(token->payload->claims, key, value);
}

/* Get claim from JWT */
json_value_t* jwt_get_claim(jwt_token_t* token, const char* key) {
    if (!token || !token->payload || !token->payload->claims || !key) {
        return NULL;
    }
    
    return json_object_get(token->payload->claims, key);
}

/* Simple SHA-256 implementation for JWT */
static void simple_sha256(const char* input, size_t input_len, unsigned char* output) {
    /* This is a placeholder for a real SHA-256 implementation
       In a real implementation, we would use OpenSSL or another crypto library */
    size_t i;
    unsigned int hash = 5381;

    /* DJB2 hash algorithm as a placeholder */
    for (i = 0; i < input_len; i++) {
        hash = ((hash << 5) + hash) + input[i];
    }

    /* Convert to bytes */
    for (i = 0; i < 32; i++) {
        output[i] = (hash >> (i % 4) * 8) & 0xFF;
    }
}

/* Simple HMAC-SHA256 for JWT signing */
static void hmac_sha256(const char* key, size_t key_len, 
                       const char* data, size_t data_len, 
                       unsigned char* output) {
    /* This is a very simplified HMAC implementation
       In a real implementation, use OpenSSL HMAC functions */
    
    /* Prepare key */
    unsigned char k_ipad[64] = {0};
    unsigned char k_opad[64] = {0};
    unsigned char key_hash[32] = {0};
    
    if (key_len > 64) {
        /* Hash the key if it's too long */
        /* Using our simple SHA256 implementation */
        simple_sha256(key, key_len, key_hash);
        key = (const char*)key_hash;
        key_len = 32;
    }
    
    /* XOR key with ipad and opad values */
    size_t i;
    for (i = 0; i < key_len; i++) {
        k_ipad[i] = key[i] ^ 0x36;
        k_opad[i] = key[i] ^ 0x5c;
    }
    for (; i < 64; i++) {
        k_ipad[i] = 0x36;
        k_opad[i] = 0x5c;
    }
    
    /* Perform inner hash */
    unsigned char inner_hash[32];
    
    /* Concatenate k_ipad with data */
    size_t total_len = 64 + data_len;
    char* inner_data = malloc(total_len);
    if (!inner_data) return;
    
    memcpy(inner_data, k_ipad, 64);
    memcpy(inner_data + 64, data, data_len);
    
    /* Hash inner_data */
    simple_sha256(inner_data, total_len, inner_hash);
    free(inner_data);
    
    /* Perform outer hash */
    /* Concatenate k_opad with inner_hash */
    char outer_data[96]; /* 64 + 32 */
    memcpy(outer_data, k_opad, 64);
    memcpy(outer_data + 64, inner_hash, 32);

    /* Hash outer_data */
    simple_sha256(outer_data, 96, output);
}

/* Sign JWT token */
static char* jwt_sign(const char* header_payload, const char* secret, const char* alg) {
    unsigned char digest[32]; /* SHA-256 output size */
    
    /* For simplicity, we only support HS256 */
    if (strcmp(alg, "HS256") != 0) {
        return NULL;  /* Unsupported algorithm */
    }
    
    /* Compute HMAC */
    hmac_sha256(secret, strlen(secret), 
               header_payload, strlen(header_payload), 
               digest);
    
    /* Base64url encode */
    return base64_url_encode(digest, 32);
}

/* Encode JWT token */
char* jwt_encode(jwt_token_t* token, const char* secret) {
    if (!token || !token->header || !token->payload || !secret) {
        return NULL;
    }
    
    /* Create header JSON */
    json_value_t* header_json = json_create_object();
    if (!header_json) {
        return NULL;
    }
    
    json_object_set(header_json, "alg", json_create_string(token->header->alg));
    json_object_set(header_json, "typ", json_create_string(token->header->typ));
    
    char* header_str = json_stringify(header_json);
    json_free(header_json);
    
    if (!header_str) {
        return NULL;
    }
    
    /* Base64url encode header */
    char* header_enc = base64_url_encode((unsigned char*)header_str, strlen(header_str));
    free(header_str);
    
    if (!header_enc) {
        return NULL;
    }
    
    /* Create payload JSON */
    json_value_t* payload_json = json_create_object();
    if (!payload_json) {
        free(header_enc);
        return NULL;
    }
    
    /* Add standard claims */
    if (token->payload->iss) {
        json_object_set(payload_json, "iss", json_create_string(token->payload->iss));
    }
    
    if (token->payload->sub) {
        json_object_set(payload_json, "sub", json_create_string(token->payload->sub));
    }
    
    if (token->payload->aud) {
        json_object_set(payload_json, "aud", json_create_string(token->payload->aud));
    }
    
    if (token->payload->exp > 0) {
        json_object_set(payload_json, "exp", json_create_number(token->payload->exp));
    }
    
    if (token->payload->nbf > 0) {
        json_object_set(payload_json, "nbf", json_create_number(token->payload->nbf));
    }
    
    if (token->payload->iat > 0) {
        json_object_set(payload_json, "iat", json_create_number(token->payload->iat));
    }
    
    if (token->payload->jti) {
        json_object_set(payload_json, "jti", json_create_string(token->payload->jti));
    }
    
    /* Add custom claims */
    for (size_t i = 0; i < token->payload->claims->value.object.size; i++) {
        json_object_entry_t* entry = &token->payload->claims->value.object.entries[i];
        
        /* Create deep copy of claim value */
        char* value_str = json_stringify(entry->value);
        if (value_str) {
            json_value_t* value_copy = json_parse(value_str);
            free(value_str);
            
            if (value_copy) {
                json_object_set(payload_json, entry->key, value_copy);
            }
        }
    }
    
    char* payload_str = json_stringify(payload_json);
    json_free(payload_json);
    
    if (!payload_str) {
        free(header_enc);
        return NULL;
    }
    
    /* Base64url encode payload */
    char* payload_enc = base64_url_encode((unsigned char*)payload_str, strlen(payload_str));
    free(payload_str);
    
    if (!payload_enc) {
        free(header_enc);
        return NULL;
    }
    
    /* Create header.payload string for signing */
    char* header_payload = (char*)malloc(strlen(header_enc) + strlen(payload_enc) + 2);
    if (!header_payload) {
        free(header_enc);
        free(payload_enc);
        return NULL;
    }
    
    sprintf(header_payload, "%s.%s", header_enc, payload_enc);
    
    /* Sign token */
    char* signature = jwt_sign(header_payload, secret, token->header->alg);
    if (!signature) {
        free(header_enc);
        free(payload_enc);
        free(header_payload);
        return NULL;
    }
    
    /* Create final token string */
    char* token_str = (char*)malloc(strlen(header_payload) + strlen(signature) + 2);
    if (!token_str) {
        free(header_enc);
        free(payload_enc);
        free(header_payload);
        free(signature);
        return NULL;
    }
    
    sprintf(token_str, "%s.%s", header_payload, signature);
    
    /* Store signature and token string */
    if (token->signature) {
        free(token->signature);
    }
    token->signature = signature;
    
    if (token->token_str) {
        free(token->token_str);
    }
    token->token_str = strdup(token_str);
    
    /* Clean up */
    free(header_enc);
    free(payload_enc);
    free(header_payload);
    
    return token_str;
}

/* Decode JWT token */
jwt_token_t* jwt_decode(const char* token_str) {
    if (!token_str) {
        return NULL;
    }
    
    /* Split token into parts */
    char* token_copy = strdup(token_str);
    char* header_b64 = strtok(token_copy, ".");
    if (!header_b64) {
        free(token_copy);
        return NULL;
    }
    
    char* payload_b64 = strtok(NULL, ".");
    if (!payload_b64) {
        free(token_copy);
        return NULL;
    }
    
    char* signature_b64 = strtok(NULL, ".");
    if (!signature_b64) {
        free(token_copy);
        return NULL;
    }
    
    /* Decode header */
    int header_len;
    unsigned char* header_json = base64_decode(header_b64, &header_len);
    if (!header_json) {
        free(token_copy);
        return NULL;
    }
    
    /* Parse header JSON */
    json_value_t* header = json_parse((const char*)header_json);
    free(header_json);
    
    if (!header || header->type != JSON_OBJECT) {
        if (header) json_free(header);
        free(token_copy);
        return NULL;
    }
    
    /* Decode payload */
    int payload_len;
    unsigned char* payload_json = base64_decode(payload_b64, &payload_len);
    if (!payload_json) {
        json_free(header);
        free(token_copy);
        return NULL;
    }
    
    /* Parse payload JSON */
    json_value_t* payload = json_parse((const char*)payload_json);
    free(payload_json);
    
    if (!payload || payload->type != JSON_OBJECT) {
        json_free(header);
        if (payload) json_free(payload);
        free(token_copy);
        return NULL;
    }
    
    /* Create token */
    jwt_token_t* token = jwt_create(NULL);
    if (!token) {
        json_free(header);
        json_free(payload);
        free(token_copy);
        return NULL;
    }
    
    /* Set header values */
    json_value_t* alg = json_object_get(header, "alg");
    if (alg && alg->type == JSON_STRING) {
        if (token->header->alg) {
            free(token->header->alg);
        }
        token->header->alg = strdup(alg->value.string);
    }
    
    json_value_t* typ = json_object_get(header, "typ");
    if (typ && typ->type == JSON_STRING) {
        if (token->header->typ) {
            free(token->header->typ);
        }
        token->header->typ = strdup(typ->value.string);
    }
    
    /* Set payload values */
    json_value_t* iss = json_object_get(payload, "iss");
    if (iss && iss->type == JSON_STRING) {
        if (token->payload->iss) {
            free(token->payload->iss);
        }
        token->payload->iss = strdup(iss->value.string);
    }
    
    json_value_t* sub = json_object_get(payload, "sub");
    if (sub && sub->type == JSON_STRING) {
        if (token->payload->sub) {
            free(token->payload->sub);
        }
        token->payload->sub = strdup(sub->value.string);
    }
    
    json_value_t* aud = json_object_get(payload, "aud");
    if (aud && aud->type == JSON_STRING) {
        if (token->payload->aud) {
            free(token->payload->aud);
        }
        token->payload->aud = strdup(aud->value.string);
    }
    
    json_value_t* exp = json_object_get(payload, "exp");
    if (exp && exp->type == JSON_NUMBER) {
        token->payload->exp = (time_t)exp->value.number;
    }
    
    json_value_t* nbf = json_object_get(payload, "nbf");
    if (nbf && nbf->type == JSON_NUMBER) {
        token->payload->nbf = (time_t)nbf->value.number;
    }
    
    json_value_t* iat = json_object_get(payload, "iat");
    if (iat && iat->type == JSON_NUMBER) {
        token->payload->iat = (time_t)iat->value.number;
    }
    
    json_value_t* jti = json_object_get(payload, "jti");
    if (jti && jti->type == JSON_STRING) {
        if (token->payload->jti) {
            free(token->payload->jti);
        }
        token->payload->jti = strdup(jti->value.string);
    }
    
    /* Add all other claims */
    for (size_t i = 0; i < payload->value.object.size; i++) {
        json_object_entry_t* entry = &payload->value.object.entries[i];
        
        /* Skip standard claims */
        if (strcmp(entry->key, "iss") == 0 || strcmp(entry->key, "sub") == 0 ||
            strcmp(entry->key, "aud") == 0 || strcmp(entry->key, "exp") == 0 ||
            strcmp(entry->key, "nbf") == 0 || strcmp(entry->key, "iat") == 0 ||
            strcmp(entry->key, "jti") == 0) {
            continue;
        }
        
        /* Create deep copy of claim value */
        char* value_str = json_stringify(entry->value);
        if (value_str) {
            json_value_t* value_copy = json_parse(value_str);
            free(value_str);
            
            if (value_copy) {
                json_object_set(token->payload->claims, entry->key, value_copy);
            }
        }
    }
    
    /* Set signature and token string */
    token->signature = strdup(signature_b64);
    token->token_str = strdup(token_str);
    
    /* Clean up */
    json_free(header);
    json_free(payload);
    free(token_copy);
    
    return token;
}

/* Verify JWT token */
int jwt_verify(const char* token_str, const char* secret) {
    if (!token_str || !secret) {
        return 0;
    }
    
    /* Decode token */
    jwt_token_t* token = jwt_decode(token_str);
    if (!token) {
        return 0;
    }
    
    /* Check token validity */
    time_t now = time(NULL);
    
    /* Check expiration */
    if (token->payload->exp > 0 && now > token->payload->exp) {
        jwt_free(token);
        return 0;
    }
    
    /* Check not before */
    if (token->payload->nbf > 0 && now < token->payload->nbf) {
        jwt_free(token);
        return 0;
    }
    
    /* Split token into parts */
    char* token_copy = strdup(token_str);
    char* header_b64 = strtok(token_copy, ".");
    if (!header_b64) {
        jwt_free(token);
        free(token_copy);
        return 0;
    }
    
    char* payload_b64 = strtok(NULL, ".");
    if (!payload_b64) {
        jwt_free(token);
        free(token_copy);
        return 0;
    }
    
    char* signature_b64 = strtok(NULL, ".");
    if (!signature_b64) {
        jwt_free(token);
        free(token_copy);
        return 0;
    }
    
    /* Create header.payload string for verification */
    char* header_payload = (char*)malloc(strlen(header_b64) + strlen(payload_b64) + 2);
    if (!header_payload) {
        jwt_free(token);
        free(token_copy);
        return 0;
    }
    
    sprintf(header_payload, "%s.%s", header_b64, payload_b64);
    
    /* Generate signature */
    char* signature = jwt_sign(header_payload, secret, token->header->alg);
    if (!signature) {
        jwt_free(token);
        free(token_copy);
        free(header_payload);
        return 0;
    }
    
    /* Compare signatures */
    int result = strcmp(signature, signature_b64) == 0;
    
    /* Clean up */
    jwt_free(token);
    free(token_copy);
    free(header_payload);
    free(signature);
    
    return result;
}