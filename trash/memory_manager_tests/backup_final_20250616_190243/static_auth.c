#include "core/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Generate a token for admin authentication */
char* generate_static_auth_token(const char* username) {
  if (!username) {
    return NULL;
  }
  
  /* Create token with format: username:timestamp:random */
  char token[256];
  snprintf(token, sizeof(token), "%s:%lu:%d", 
       username, (unsigned long)time(NULL), rand() % 10000);
  
  /* Encode token (simple base64 would be better, but we'll use a basic encoding for now) */
  char* encoded = (char*)malloc(strlen(token) * 2 + 1);
  if (!encoded) {
    return NULL;
  }
  
  /* Simple encoding (not secure, but sufficient for demo) */
  size_t token_len = strlen(token);
  for (size_t i = 0; i < token_len; i++) {
    sprintf(encoded + (i * 2), "%02x", (unsigned char)token[i]);
  }
  
  return encoded;
}

/* Check if request has valid admin authentication */
int check_static_auth(http_request_t* request) {
  if (!request) {
    return 0;
  }
  
  /* Check for admin auth cookie */
  char* token = get_cookie_value(request, ADMIN_AUTH_COOKIE_NAME);
  if (!token) {
    return 0;
  }
  
  /* In a real implementation, you would validate the token here */
  /* For demo purposes, just check if the token exists and is not empty */
  return *token != '\0';
}