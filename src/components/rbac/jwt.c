#include "rbac/jwt.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
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
  char* output = (char*)BUFFER_ALLOC(encoded_len + 1);
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
  if (!input || !output_length) return NULL;
  
  int input_len = strlen(input);
  if (input_len % 4 != 0) return NULL;
  
  /* Validate input contains only valid base64 characters */
  for (int k = 0; k < input_len; k++) {
    char c = input[k];
    if (!(c >= 'A' && c <= 'Z') && 
        !(c >= 'a' && c <= 'z') && 
        !(c >= '0' && c <= '9') && 
        c != '+' && c != '/' && c != '=') {
      /* Invalid base64 character found */
      return NULL;
    }
  }
  
  int decoded_len = input_len / 4 * 3;
  if (input_len > 0 && input[input_len-1] == '=') decoded_len--;
  if (input_len > 1 && input[input_len-2] == '=') decoded_len--;
  
  unsigned char* output = (unsigned char*)BUFFER_ALLOC(decoded_len + 1);
  if (!output) return NULL;
  
  /* Create reverse lookup table */
  unsigned char decode_table[256];
  /* Initialize all to invalid value */
  memset(decode_table, 255, sizeof(decode_table));
  
  /* Set valid characters */
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

/* URL-safe version of base64 decoding - ADDED MISSING FUNCTION */
static unsigned char* base64_url_decode(const char* input, int* output_length) {
  /* Step 1: Make a mutable copy of the input */
  char* input_copy = buffer_pool_strdup(input);
  if (!input_copy) return NULL;
  
  /* Step 2: Convert URL-safe characters back to standard base64 */
  for (char* p = input_copy; *p; p++) {
    if (*p == '-') *p = '+';
    else if (*p == '_') *p = '/';
  }
  
  /* Step 3: Add padding if necessary */
  int len = strlen(input_copy);
  int padding = (4 - (len % 4)) % 4;
  
  char* padded_input = (char*)BUFFER_ALLOC(len + padding + 1);
  if (!padded_input) {
    buffer_pool_free(input_copy);
    return NULL;
  }
  
  strcpy(padded_input, input_copy);
  for (int i = 0; i < padding; i++) {
    padded_input[len + i] = '=';
  }
  padded_input[len + padding] = '\0';
  
  buffer_pool_free(input_copy);
  
  /* Step 4: Use regular base64 decode */
  unsigned char* output = base64_decode(padded_input, output_length);
  buffer_pool_free(padded_input);
  
  return output;
}

/* Simple SHA-256 implementation for JWT */
static void simple_sha256(const char* input, size_t input_len, unsigned char* output) {
  /* This is a placeholder for a real SHA-256 implementation
    In a real implementation, we would use OpenSSL or another crypto library */
  size_t i;
  
  /* Initialize hash values (first 32 bits of the fractional parts of the square roots of the first 8 primes) */
  uint32_t h0 = 0x6a09e667;
  uint32_t h1 = 0xbb67ae85;
  uint32_t h2 = 0x3c6ef372;
  uint32_t h3 = 0xa54ff53a;
  uint32_t h4 = 0x510e527f;
  uint32_t h5 = 0x9b05688c;
  uint32_t h6 = 0x1f83d9ab;
  uint32_t h7 = 0x5be0cd19;
  
  /* Simplified hash calculation */
  /* Just mix the input bytes with the initial hash values */
  for (i = 0; i < input_len; i++) {
    uint8_t byte = input[i];
    h0 = (h0 ^ byte) + ((h0 << 5) | (h0 >> 27));
    h1 = (h1 ^ byte) + ((h1 << 7) | (h1 >> 25));
    h2 = (h2 ^ byte) + ((h2 << 9) | (h2 >> 23));
    h3 = (h3 ^ byte) + ((h3 << 13) | (h3 >> 19));
    h4 = (h4 ^ byte) + ((h4 << 17) | (h4 >> 15));
    h5 = (h5 ^ byte) + ((h5 << 19) | (h5 >> 13));
    h6 = (h6 ^ byte) + ((h6 << 23) | (h6 >> 9));
    h7 = (h7 ^ byte) + ((h7 << 29) | (h7 >> 3));
  }
  
  /* Convert hash values to bytes */
  for (i = 0; i < 4; i++) {
    output[i]   = (h0 >> (24 - i * 8)) & 0xFF;
    output[i + 4] = (h1 >> (24 - i * 8)) & 0xFF;
    output[i + 8] = (h2 >> (24 - i * 8)) & 0xFF;
    output[i + 12] = (h3 >> (24 - i * 8)) & 0xFF;
    output[i + 16] = (h4 >> (24 - i * 8)) & 0xFF;
    output[i + 20] = (h5 >> (24 - i * 8)) & 0xFF;
    output[i + 24] = (h6 >> (24 - i * 8)) & 0xFF;
    output[i + 28] = (h7 >> (24 - i * 8)) & 0xFF;
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
  char* inner_data = BUFFER_ALLOC(total_len);
  if (!inner_data) return;
  
  memcpy(inner_data, k_ipad, 64);
  memcpy(inner_data + 64, data, data_len);
  
  /* Hash inner_data */
  simple_sha256(inner_data, total_len, inner_hash);
  BUFFER_FREE(inner_data);
  
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
    return NULL; /* Unsupported algorithm */
  }
  
  /* Compute HMAC */
  hmac_sha256(secret, strlen(secret), 
        header_payload, strlen(header_payload), 
        digest);
  
  /* Base64url encode */
  return base64_url_encode(digest, 32);
}

/* Create new JWT token */
jwt_token_t* jwt_create(const char* secret __attribute__((unused))) {
  jwt_token_t* token = (jwt_token_t*)BUFFER_ALLOC(sizeof(jwt_token_t));
  if (!token) {
    return NULL;
  }
  
  /* Initialize token */
  token->header = (jwt_header_t*)BUFFER_ALLOC(sizeof(jwt_header_t));
  if (!token->header) {
    buffer_pool_free(token);
    return NULL;
  }
  
  token->payload = (jwt_payload_t*)BUFFER_ALLOC(sizeof(jwt_payload_t));
  if (!token->payload) {
    buffer_pool_free(token->header);
    buffer_pool_free(token);
    return NULL;
  }
  
  /* Set default header values */
  token->header->alg = buffer_pool_strdup("HS256");
  token->header->typ = buffer_pool_strdup("JWT");
  
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
    if (token->header->alg) BUFFER_FREE(token->header->alg);
    if (token->header->typ) BUFFER_FREE(token->header->typ);
    BUFFER_FREE(token->header);
  }
  
  /* Free payload */
  if (token->payload) {
    if (token->payload->iss) BUFFER_FREE(token->payload->iss);
    if (token->payload->sub) BUFFER_FREE(token->payload->sub);
    if (token->payload->aud) BUFFER_FREE(token->payload->aud);
    if (token->payload->jti) BUFFER_FREE(token->payload->jti);
    if (token->payload->claims) {

        /* CHECKPOINT: json_free(token->payload->claims); */

    }
    BUFFER_FREE(token->payload);
  }
  
  /* Free other fields */
  if (token->signature) BUFFER_FREE(token->signature);
  if (token->token_str) BUFFER_FREE(token->token_str);  /* buffer_pool allocated */
  
  BUFFER_FREE(token);
}

/* Set JWT algorithm */
void jwt_set_algorithm(jwt_token_t* token, const char* alg) {
  if (!token || !token->header || !alg) {
    return;
  }
  
  if (token->header->alg) {
    BUFFER_FREE(token->header->alg);
  }
  
  token->header->alg = buffer_pool_strdup(alg);
}

/* Set JWT issuer */
void jwt_set_issuer(jwt_token_t* token, const char* iss) {
  if (!token || !token->payload || !iss) {
    return;
  }
  
  if (token->payload->iss) {
    BUFFER_FREE(token->payload->iss);
  }
  
  token->payload->iss = buffer_pool_strdup(iss);
}

/* Set JWT subject */
void jwt_set_subject(jwt_token_t* token, const char* sub) {
  if (!token || !token->payload || !sub) {
    return;
  }
  
  if (token->payload->sub) {
    BUFFER_FREE(token->payload->sub);
  }
  
  token->payload->sub = buffer_pool_strdup(sub);
}

/* Set JWT audience */
void jwt_set_audience(jwt_token_t* token, const char* aud) {
  if (!token || !token->payload || !aud) {
    return;
  }
  
  if (token->payload->aud) {
    BUFFER_FREE(token->payload->aud);
  }
  
  token->payload->aud = buffer_pool_strdup(aud);
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
    BUFFER_FREE(token->payload->jti);
  }
  
  token->payload->jti = BUFFER_STRDUP(jti);
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
  /* CHECKPOINT: json_free(header_json); */
  
  if (!header_str) {
    LOG_DEBUG("Failed to stringify header JSON.");
    return NULL;
  }
  
  LOG_DEBUG("Header string created: %s", header_str);
  
  /* Base64url encode header */
  char* header_enc = base64_url_encode((unsigned char*)header_str, strlen(header_str));
  BUFFER_FREE(header_str);
  
  LOG_DEBUG("Header encoded.");
  
  if (!header_enc) {
    return NULL;
  }
  
  /* Create payload JSON */
  json_value_t* payload_json = json_create_object();
  if (!payload_json) {
    BUFFER_FREE(header_enc);
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
    json_object_set(payload_json, "exp", json_create_integer(token->payload->exp));
  }
  
  if (token->payload->nbf > 0) {
    json_object_set(payload_json, "nbf", json_create_integer(token->payload->nbf));
  }
  
  if (token->payload->iat > 0) {
    json_object_set(payload_json, "iat", json_create_integer(token->payload->iat));
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
      BUFFER_FREE(value_str);
      
      if (value_copy) {
        json_object_set(payload_json, entry->key, value_copy);
      }
    }
  }
  
  char* payload_str = json_stringify(payload_json);
  /* CHECKPOINT: json_free(payload_json); */
  
  if (!payload_str) {
    BUFFER_FREE(header_enc);
    return NULL;
  }
  
  /* Base64url encode payload */
  char* payload_enc = base64_url_encode((unsigned char*)payload_str, strlen(payload_str));
  BUFFER_FREE(payload_str);
  
  if (!payload_enc) {
    buffer_pool_free(header_enc);
    return NULL;
  }
  
  /* Create header.payload string for signing */
  char* header_payload = (char*)BUFFER_ALLOC(strlen(header_enc) + strlen(payload_enc) + 2);
  if (!header_payload) {
    buffer_pool_free(header_enc);
    buffer_pool_free(payload_enc);
    return NULL;
  }
  
  sprintf(header_payload, "%s.%s", header_enc, payload_enc);
  
  /* Sign token */
  char* signature = jwt_sign(header_payload, secret, token->header->alg);
  if (!signature) {
    buffer_pool_free(header_enc);
    buffer_pool_free(payload_enc);
    buffer_pool_free(header_payload);
    return NULL;
  }
  
  /* Create final token string */
  char* token_str = (char*)BUFFER_ALLOC(strlen(header_payload) + strlen(signature) + 2);
  if (!token_str) {
    buffer_pool_free(header_enc);
    buffer_pool_free(payload_enc);
    buffer_pool_free(header_payload);
    buffer_pool_free(signature);
    return NULL;
  }
  
  sprintf(token_str, "%s.%s", header_payload, signature);
  
  /* Store signature and token string */
  if (token->signature) {
    BUFFER_FREE(token->signature);
  }
  token->signature = signature;
  
  if (token->token_str) {
    BUFFER_FREE(token->token_str);
  }
  token->token_str = BUFFER_STRDUP(token_str);
  
  /* Clean up */
  buffer_pool_free(header_enc);
  buffer_pool_free(payload_enc);
  buffer_pool_free(header_payload);
  
  return token_str;
}

/* Decode JWT token */
jwt_token_t* jwt_decode(const char* token_str) {
  if (!token_str || strlen(token_str) == 0) {
    return NULL;
  }
  
  /* Basic format validation - JWT must have at least 2 dots */
  int dot_count = 0;
  for (const char* p = token_str; *p; p++) {
    if (*p == '.') dot_count++;
  }
  if (dot_count != 2) {
    /* Invalid JWT format - must have exactly 2 dots */
    return NULL;
  }
  
  /* Split token into parts */
  char* token_copy = buffer_pool_strdup(token_str);
  if (!token_copy) {
    return NULL;
  }
  
  char* header_b64 = strtok(token_copy, ".");
  if (!header_b64 || strlen(header_b64) == 0) {
    BUFFER_FREE(token_copy);
    return NULL;
  }
  
  char* payload_b64 = strtok(NULL, ".");
  if (!payload_b64 || strlen(payload_b64) == 0) {
    BUFFER_FREE(token_copy);
    return NULL;
  }
  
  char* signature_b64 = strtok(NULL, ".");
  if (!signature_b64 || strlen(signature_b64) == 0) {
    BUFFER_FREE(token_copy);
    return NULL;
  }
  
  /* Decode header */
  int header_len;
  unsigned char* header_json = base64_url_decode(header_b64, &header_len);
  if (!header_json) {
    BUFFER_FREE(token_copy);
    return NULL;
  }
  
  /* Parse header JSON */
  json_value_t* header = json_parse((const char*)header_json);
  BUFFER_FREE(header_json);
  
  if (!header || header->type != JSON_OBJECT) {
    if (header) {

        /* CHECKPOINT: json_free(header); */

    }
    BUFFER_FREE(token_copy);
    return NULL;
  }
  
  /* Decode payload */
  int payload_len;
  unsigned char* payload_json = base64_url_decode(payload_b64, &payload_len);
  if (!payload_json) {
    /* CHECKPOINT: json_free(header); */
    BUFFER_FREE(token_copy);
    return NULL;
  }
  
  /* Parse payload JSON */
  json_value_t* payload = json_parse((const char*)payload_json);
  BUFFER_FREE(payload_json);
  
  if (!payload || payload->type != JSON_OBJECT) {
    /* CHECKPOINT: json_free(header); */
    if (payload) {

        /* CHECKPOINT: json_free(payload); */

    }
    BUFFER_FREE(token_copy);
    return NULL;
  }
  
  /* Create token */
  jwt_token_t* token = jwt_create(NULL);
  if (!token) {
    /* CHECKPOINT: json_free(header); */
    /* CHECKPOINT: json_free(payload); */
    BUFFER_FREE(token_copy);
    return NULL;
  }
  
  /* Set header values */
  json_value_t* alg = json_object_get(header, "alg");
  if (alg && alg->type == JSON_STRING) {
    if (token->header->alg) {
      BUFFER_FREE(token->header->alg);
    }
    token->header->alg = buffer_pool_strdup(alg->value.string);
  }
  
  json_value_t* typ = json_object_get(header, "typ");
  if (typ && typ->type == JSON_STRING) {
    if (token->header->typ) {
      BUFFER_FREE(token->header->typ);
    }
    token->header->typ = buffer_pool_strdup(typ->value.string);
  }
  
  /* Set payload values */
  json_value_t* iss = json_object_get(payload, "iss");
  if (iss && iss->type == JSON_STRING) {
    if (token->payload->iss) {
      BUFFER_FREE(token->payload->iss);
    }
    token->payload->iss = buffer_pool_strdup(iss->value.string);
  }
  
  json_value_t* sub = json_object_get(payload, "sub");
  if (sub && sub->type == JSON_STRING) {
    if (token->payload->sub) {
      BUFFER_FREE(token->payload->sub);
    }
    token->payload->sub = buffer_pool_strdup(sub->value.string);
  }
  
  json_value_t* aud = json_object_get(payload, "aud");
  if (aud && aud->type == JSON_STRING) {
    if (token->payload->aud) {
      BUFFER_FREE(token->payload->aud);
    }
    token->payload->aud = buffer_pool_strdup(aud->value.string);
  }
  
  json_value_t* exp = json_object_get(payload, "exp");
  if (exp && (exp->type == JSON_NUMBER || exp->type == JSON_INTEGER)) {
    if (exp->type == JSON_INTEGER) {
      token->payload->exp = (time_t)exp->value.integer;
    } else {
      token->payload->exp = (time_t)exp->value.number;
    }
  }
  
  json_value_t* nbf = json_object_get(payload, "nbf");
  if (nbf && (nbf->type == JSON_NUMBER || nbf->type == JSON_INTEGER)) {
    if (nbf->type == JSON_INTEGER) {
      token->payload->nbf = (time_t)nbf->value.integer;
    } else {
      token->payload->nbf = (time_t)nbf->value.number;
    }
  }
  
  json_value_t* iat = json_object_get(payload, "iat");
  if (iat && (iat->type == JSON_NUMBER || iat->type == JSON_INTEGER)) {
    if (iat->type == JSON_INTEGER) {
      token->payload->iat = (time_t)iat->value.integer;
    } else {
      token->payload->iat = (time_t)iat->value.number;
    }
  }
  
  json_value_t* jti = json_object_get(payload, "jti");
  if (jti && jti->type == JSON_STRING) {
    if (token->payload->jti) {
      BUFFER_FREE(token->payload->jti);
    }
    token->payload->jti = BUFFER_STRDUP(jti->value.string);
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
      BUFFER_FREE(value_str);
      
      if (value_copy) {
        json_object_set(token->payload->claims, entry->key, value_copy);
      }
    }
  }
  
  /* Set signature and token string */
  token->signature = buffer_pool_strdup(signature_b64);
  token->token_str = BUFFER_STRDUP(token_str);
  
  /* Clean up */
  /* CHECKPOINT: json_free(header); */
  /* CHECKPOINT: json_free(payload); */
  BUFFER_FREE(token_copy);
  
  return token;
}

/* Verify JWT token - FIXED VERSION */
int jwt_verify(const char* token_str, const char* secret) {
  if (!token_str || !secret) {
    return 0;
  }
  
  /* Decode token */
  jwt_token_t* token = jwt_decode(token_str);
  if (!token) {
    LOG_DEBUG("Failed to decode token.");
    return 0;
  }
  TRACE_RBAC("JWT: Token decoded successfully - sub: %s, exp: %ld", 
       token->payload->sub ? token->payload->sub : "NULL", 
       (long)token->payload->exp);
  
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
  char* token_copy = buffer_pool_strdup(token_str);
  char* header_b64 = strtok(token_copy, ".");
  if (!header_b64) {
    jwt_free(token);
    BUFFER_FREE(token_copy);
    return 0;
  }
  
  char* payload_b64 = strtok(NULL, ".");
  if (!payload_b64) {
    jwt_free(token);
    BUFFER_FREE(token_copy);
    return 0;
  }
  
  char* signature_b64 = strtok(NULL, ".");
  if (!signature_b64) {
    jwt_free(token);
    BUFFER_FREE(token_copy);
    return 0;
  }
  
  /* Create header.payload string for verification */
  char* header_payload = (char*)BUFFER_ALLOC(strlen(header_b64) + strlen(payload_b64) + 2);
  if (!header_payload) {
    jwt_free(token);
    BUFFER_FREE(token_copy);
    return 0;
  }
  
  sprintf(header_payload, "%s.%s", header_b64, payload_b64);
  
  /* Generate expected signature for verification */
  char* signature = jwt_sign(header_payload, secret, token->header->alg);
  if (!signature) {
    jwt_free(token);
    BUFFER_FREE(token_copy);
    BUFFER_FREE(header_payload);
    return 0;
  }
  
  /* Byte-by-byte comparison of signatures to prevent timing attacks */
  size_t signature_len = strlen(signature);
  size_t signature_b64_len = strlen(signature_b64);
  
  /* Check signature lengths match */
  if (signature_len != signature_b64_len) {
    jwt_free(token);
    BUFFER_FREE(token_copy);
    BUFFER_FREE(header_payload);
    BUFFER_FREE(signature);
    return 0;
  }
  
  /* Constant-time comparison of signatures */
  int result = 1;
  for (size_t i = 0; i < signature_len; i++) {
    if (signature[i] != signature_b64[i]) {
      result = 0;
      // Don't break early - this would create a timing side-channel
    }
  }
  
  /* Clean up */
  jwt_free(token);
  BUFFER_FREE(token_copy);
  BUFFER_FREE(header_payload);
  BUFFER_FREE(signature);
  
  TRACE_AUTH("JWT verification result: %d", result);
  return result;
}

/* Create a refresh token */
jwt_token_t* jwt_create_refresh_token(const char* secret, const char* user_id, time_t expiry) {
  TRACE_AUTH("Creating refresh token for user: %s", user_id ? user_id : "NULL");
  if (!secret || !user_id) {
    LOG_ERROR("Refresh token creation failed: invalid parameters.");
    return NULL;
  }
  
  /* Create new token */
  TRACE_AUTH("Creating refresh token structure.");
  jwt_token_t* token = jwt_create(secret);
  if (!token) {
    LOG_ERROR("Refresh token creation failed: unable to create token structure.");
    return NULL;
  }
  TRACE_AUTH("Refresh token structure created.");
  
  /* Set token claims */
  TRACE_AUTH("Setting refresh token claims.");
  jwt_set_subject(token, user_id);
  jwt_set_issuer(token, "jdbx");
  
  /* Set expiration (defaults to 7 days if not specified) */
  if (expiry == 0) {
    expiry = time(NULL) + (7 * 24 * 60 * 60); /* 7 days */
  }
  jwt_set_expiration(token, expiry);
  
  /* Add refresh token claim */
  TRACE_AUTH("Adding refresh token type claim.");
  jwt_add_claim(token, "type", json_create_string("refresh"));
  TRACE_AUTH("Refresh token creation completed.");
  
  return token;
}

/* Create both access and refresh tokens and return them as a pair */
char* jwt_create_token_pair(const char* secret, const char* user_id, const char* username, json_value_t** response_json) {
  TRACE_AUTH("Creating token pair for user: %s", username ? username : "NULL");
  if (!secret || !user_id || !username || !response_json) {
    LOG_ERROR("Token pair creation failed: invalid parameters.");
    return NULL;
  }
  
  /* Create access token */
  TRACE_AUTH("Creating access token.");
  jwt_token_t* access_token = jwt_create(secret);
  if (!access_token) {
    LOG_ERROR("Token pair creation failed: unable to create access token.");
    return NULL;
  }
  TRACE_AUTH("Access token created.");
  
  /* Set access token claims */
  TRACE_AUTH("Setting access token claims.");
  jwt_set_subject(access_token, user_id);
  jwt_set_issuer(access_token, "jdbx");
  jwt_set_expiration(access_token, time(NULL) + (30 * 60)); /* 30 minutes */
  jwt_add_claim(access_token, "username", json_create_string(username));
  jwt_add_claim(access_token, "type", json_create_string("access"));
  TRACE_AUTH("Access token claims set.");
  
  /* Create refresh token */
  TRACE_AUTH("Creating refresh token.");
  jwt_token_t* refresh_token = jwt_create_refresh_token(secret, user_id, 0); /* Use default expiry */
  if (!refresh_token) {
    LOG_ERROR("Token pair creation failed: unable to create refresh token.");
    jwt_free(access_token);
    return NULL;
  }
  TRACE_AUTH("Refresh token created.");
  
  /* Encode tokens */
  TRACE_AUTH("Encoding access token.");
  char* access_token_str = jwt_encode(access_token, secret);
  TRACE_AUTH("Encoding refresh token.");
  char* refresh_token_str = jwt_encode(refresh_token, secret);
  TRACE_AUTH("Both tokens encoded successfully.");
  
  /* Free token structures */
  TRACE_AUTH("Freeing token structures.");
  jwt_free(access_token);
  jwt_free(refresh_token);
  TRACE_AUTH("Token structures freed.");
  
  if (!access_token_str || !refresh_token_str) {
    if (access_token_str) BUFFER_FREE(access_token_str);  /* strdup allocated */
    if (refresh_token_str) BUFFER_FREE(refresh_token_str);  /* strdup allocated */
    return NULL;
  }
  
  /* Create response JSON */
  json_value_t* response = json_create_object();
  if (!response) {
    buffer_pool_free(access_token_str);
    buffer_pool_free(refresh_token_str);
    return NULL;
  }
  
  json_object_set(response, "token", json_create_string(access_token_str));
  json_object_set(response, "refresh_token", json_create_string(refresh_token_str));
  json_object_set(response, "user_id", json_create_string(user_id));
  json_object_set(response, "username", json_create_string(username));
  json_object_set(response, "expires_in", json_create_number(30 * 60)); /* 30 minutes in seconds */
  
  /* Create response string */
  char* response_str = json_stringify(response);
  
  /* Set the response JSON for the caller */
  *response_json = response;
  
  /* Clean up */
  buffer_pool_free(access_token_str);
  buffer_pool_free(refresh_token_str);
  
  return response_str;
}

/* Verify a refresh token and extract the user ID if valid */
int jwt_verify_refresh_token(const char* refresh_token, const char* secret, char** user_id) {
  if (!refresh_token || !secret || !user_id) {
    return 0;
  }
  
  /* Initialize the output parameter */
  *user_id = NULL;
  
  /* First verify the token signature and expiry */
  if (!jwt_verify(refresh_token, secret)) {
    return 0;
  }
  
  /* Decode the token to check its claims */
  jwt_token_t* token = jwt_decode(refresh_token);
  if (!token) {
    return 0;
  }
  
  /* Check if it's a refresh token */
  json_value_t* type_claim = jwt_get_claim(token, "type");
  if (!type_claim || type_claim->type != JSON_STRING ||
    strcmp(type_claim->value.string, "refresh") != 0) {
    jwt_free(token);
    return 0;
  }
  
  /* Extract user ID from subject claim */
  if (!token->payload->sub) {
    jwt_free(token);
    return 0;
  }
  
  /* Set the output parameter */
  size_t len = strlen(token->payload->sub) + 1;
  *user_id = (char*)BUFFER_ALLOC(len);
  if (!*user_id) {
    jwt_free(token);
    return 0;
  }
  memcpy(*user_id, token->payload->sub, len);
  
  /* Clean up */
  jwt_free(token);
  
  return 1;
}

/* Free JWT payload */
void jwt_payload_free(jwt_payload_t* payload) {
  if (!payload) return;
  
  if (payload->iss) BUFFER_FREE(payload->iss);
  if (payload->sub) BUFFER_FREE(payload->sub);
  if (payload->aud) BUFFER_FREE(payload->aud);
  if (payload->jti) BUFFER_FREE(payload->jti);  /* strdup allocated */
  if (payload->claims) {

      /* CHECKPOINT: json_free(payload->claims); */

  }
  
  BUFFER_FREE(payload);
}

/* Duplicate JWT payload for caching */
jwt_payload_t* jwt_payload_duplicate(const jwt_payload_t* payload) {
  if (!payload) return NULL;
  
  jwt_payload_t* dup = (jwt_payload_t*)BUFFER_ALLOC(sizeof(jwt_payload_t));
  if (!dup) return NULL;
  
  /* Copy all fields */
  dup->iss = payload->iss ? buffer_pool_strdup(payload->iss) : NULL;
  dup->sub = payload->sub ? buffer_pool_strdup(payload->sub) : NULL;
  dup->aud = payload->aud ? buffer_pool_strdup(payload->aud) : NULL;
  dup->jti = payload->jti ? buffer_pool_strdup(payload->jti) : NULL;
  dup->claims = payload->claims ? json_clone(payload->claims) : NULL;
  
  dup->exp = payload->exp;
  dup->nbf = payload->nbf;
  dup->iat = payload->iat;
  
  return dup;
}

/* Set username in JWT payload */
void jwt_payload_set_username(jwt_payload_t* payload, const char* username) {
  if (!payload || !username) return;
  
  if (!payload->claims) {
    payload->claims = json_create_object();
  }
  
  json_object_set(payload->claims, "username", json_create_string(username));
}