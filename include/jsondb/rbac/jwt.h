#ifndef JWT_H
#define JWT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/json.h"

/* JWT header */
typedef struct {
    char* alg;  /* Algorithm */
    char* typ;  /* Type */
} jwt_header_t;

/* JWT payload */
typedef struct {
    char* iss;      /* Issuer */
    char* sub;      /* Subject */
    char* aud;      /* Audience */
    time_t exp;     /* Expiration time */
    time_t nbf;     /* Not before */
    time_t iat;     /* Issued at */
    char* jti;      /* JWT ID */
    json_value_t* claims; /* Custom claims */
} jwt_payload_t;

/* JWT token */
typedef struct {
    jwt_header_t* header;
    jwt_payload_t* payload;
    char* signature;
    char* token_str;
} jwt_token_t;

/* JWT function prototypes */
jwt_token_t* jwt_create(const char* secret);
void jwt_free(jwt_token_t* token);
int jwt_verify(const char* token_str, const char* secret);
jwt_token_t* jwt_decode(const char* token_str);
char* jwt_encode(jwt_token_t* token, const char* secret);

/* JWT header operations */
void jwt_set_algorithm(jwt_token_t* token, const char* alg);

/* JWT payload operations */
void jwt_set_issuer(jwt_token_t* token, const char* iss);
void jwt_set_subject(jwt_token_t* token, const char* sub);
void jwt_set_audience(jwt_token_t* token, const char* aud);
void jwt_set_expiration(jwt_token_t* token, time_t exp);
void jwt_set_not_before(jwt_token_t* token, time_t nbf);
void jwt_set_issued_at(jwt_token_t* token, time_t iat);
void jwt_set_jwt_id(jwt_token_t* token, const char* jti);
void jwt_add_claim(jwt_token_t* token, const char* key, json_value_t* value);
json_value_t* jwt_get_claim(jwt_token_t* token, const char* key);

#endif /* JWT_H */