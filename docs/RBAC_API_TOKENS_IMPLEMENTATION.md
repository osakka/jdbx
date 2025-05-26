# RBAC API Tokens Implementation Plan

## Overview
This document outlines the implementation plan for long-lived API tokens in JSONdb's RBAC system, as requested by the user who noted that "users will want to generate long lived tokens instead of logging in using their username and passwords."

## Current State
- JWT tokens expire after 30 minutes
- Users must re-authenticate with username/password
- No support for long-lived API tokens

## Proposed Implementation

### 1. Database Schema
Add a new collection `_api_tokens`:
```json
{
  "_id": "token-<timestamp>-<random>",
  "token": "<hashed-token>",
  "user_id": "admin",
  "name": "Production API Key",
  "permissions": ["READ", "WRITE"],
  "created_at": "2025-05-26T12:00:00Z",
  "expires_at": null,  // null for no expiry
  "last_used": "2025-05-26T12:00:00Z",
  "active": true
}
```

### 2. API Endpoints

#### Create Token
```
POST /api/rbac/tokens
Authorization: Bearer <jwt-token>
{
  "name": "Production API Key",
  "permissions": ["READ", "WRITE"],
  "expires_in_days": 365  // optional, null for no expiry
}

Response:
{
  "token": "jsondb_pat_xyzabc123...",  // shown only once
  "id": "token-1748261234-5678",
  "name": "Production API Key",
  "expires_at": "2026-05-26T12:00:00Z"
}
```

#### List Tokens
```
GET /api/rbac/tokens
Authorization: Bearer <jwt-token>

Response:
{
  "tokens": [
    {
      "id": "token-1748261234-5678",
      "name": "Production API Key",
      "created_at": "2025-05-26T12:00:00Z",
      "last_used": "2025-05-26T12:00:00Z",
      "active": true
    }
  ]
}
```

#### Revoke Token
```
DELETE /api/rbac/tokens/{token-id}
Authorization: Bearer <jwt-token>
```

### 3. Authentication Flow
Update authentication to accept API tokens:
```
Authorization: Bearer jsondb_pat_xyzabc123...
```

### 4. Implementation Files

#### `/opt/jsondb/src/components/api/api_tokens.c`
```c
#include "api/api_tokens.h"
#include "rbac/rbac.h"
#include "utils/json_helpers.h"
#include <openssl/rand.h>
#include <openssl/evp.h>

#define TOKEN_PREFIX "jsondb_pat_"
#define TOKEN_LENGTH 32

static char* generate_token() {
    unsigned char random_bytes[TOKEN_LENGTH];
    RAND_bytes(random_bytes, TOKEN_LENGTH);
    
    // Convert to base64
    char* token = malloc(TOKEN_LENGTH * 2 + strlen(TOKEN_PREFIX) + 1);
    strcpy(token, TOKEN_PREFIX);
    
    // Base64 encode
    EVP_ENCODE_CTX *ctx = EVP_ENCODE_CTX_new();
    EVP_EncodeInit(ctx);
    int out_len;
    EVP_EncodeUpdate(ctx, (unsigned char*)(token + strlen(TOKEN_PREFIX)), 
                     &out_len, random_bytes, TOKEN_LENGTH);
    EVP_EncodeFinal(ctx, (unsigned char*)(token + strlen(TOKEN_PREFIX) + out_len), &out_len);
    EVP_ENCODE_CTX_free(ctx);
    
    // Remove newlines
    char* p = token;
    char* q = token;
    while (*p) {
        if (*p != '\n' && *p != '\r') {
            *q++ = *p;
        }
        p++;
    }
    *q = '\0';
    
    return token;
}

void handle_create_api_token(struct http_request* req, struct http_response* resp) {
    // Verify JWT authentication
    const char* auth_header = get_header(req, "Authorization");
    if (!auth_header || !verify_jwt_token(auth_header + 7)) {
        set_error_response(resp, 401, "Unauthorized");
        return;
    }
    
    // Parse request body
    json_t* body = parse_json_body(req);
    if (!body) {
        set_error_response(resp, 400, "Invalid JSON");
        return;
    }
    
    // Generate token
    char* token = generate_token();
    char* token_hash = hash_token(token);
    
    // Create token document
    json_t* token_doc = json_object();
    json_object_set_new(token_doc, "_id", 
        json_sprintf("token-%ld-%d", time(NULL), rand()));
    json_object_set_new(token_doc, "token", json_string(token_hash));
    json_object_set_new(token_doc, "user_id", 
        json_string(get_user_from_jwt(auth_header + 7)));
    json_object_set_new(token_doc, "name", 
        json_object_get(body, "name"));
    json_object_set_new(token_doc, "created_at", 
        json_string(get_iso_timestamp()));
    
    // Save to database
    db_insert("_api_tokens", token_doc);
    
    // Return token (only shown once)
    json_t* response = json_object();
    json_object_set_new(response, "token", json_string(token));
    json_object_set_new(response, "id", 
        json_object_get(token_doc, "_id"));
    
    set_json_response(resp, 201, response);
    
    free(token);
    free(token_hash);
    json_decref(body);
    json_decref(token_doc);
    json_decref(response);
}
```

#### Update `/opt/jsondb/src/components/rbac/jwt.c`
Add function to verify API tokens:
```c
bool verify_api_token(const char* token) {
    if (strncmp(token, TOKEN_PREFIX, strlen(TOKEN_PREFIX)) != 0) {
        return false;
    }
    
    char* token_hash = hash_token(token);
    
    // Look up token in database
    json_t* query = json_object();
    json_object_set_new(query, "token", json_string(token_hash));
    json_object_set_new(query, "active", json_true());
    
    json_t* token_doc = db_find_one("_api_tokens", query);
    if (!token_doc) {
        free(token_hash);
        json_decref(query);
        return false;
    }
    
    // Update last_used
    json_object_set_new(token_doc, "last_used", 
        json_string(get_iso_timestamp()));
    db_update("_api_tokens", json_object_get(token_doc, "_id"), token_doc);
    
    free(token_hash);
    json_decref(query);
    json_decref(token_doc);
    return true;
}
```

### 5. Frontend Implementation
Add API token management to the RBAC UI:

```javascript
// In app.js
async function createAPIToken() {
    const name = prompt('Enter a name for this API token:');
    if (!name) return;
    
    try {
        const response = await apiRequest('/api/rbac/tokens', {
            method: 'POST',
            body: JSON.stringify({ name })
        });
        
        // Show token in modal (only shown once)
        showTokenModal(response.token);
        
        // Refresh token list
        await loadAPITokens();
    } catch (error) {
        showNotification('Failed to create API token', 'error');
    }
}

function showTokenModal(token) {
    const modalHtml = `
        <div class="modal fade" id="tokenModal">
            <div class="modal-dialog">
                <div class="modal-content">
                    <div class="modal-header">
                        <h5 class="modal-title">API Token Created</h5>
                    </div>
                    <div class="modal-body">
                        <div class="alert alert-warning">
                            <i class="bi bi-exclamation-triangle"></i>
                            This token will only be shown once. Please copy it now.
                        </div>
                        <div class="input-group">
                            <input type="text" class="form-control" value="${token}" readonly>
                            <button class="btn btn-outline-secondary" onclick="copyToken('${token}')">
                                <i class="bi bi-clipboard"></i> Copy
                            </button>
                        </div>
                    </div>
                    <div class="modal-footer">
                        <button type="button" class="btn btn-primary" data-bs-dismiss="modal">Done</button>
                    </div>
                </div>
            </div>
        </div>
    `;
    
    document.body.insertAdjacentHTML('beforeend', modalHtml);
    const modal = new bootstrap.Modal(document.getElementById('tokenModal'));
    modal.show();
}
```

### 6. Usage Example
```bash
# Create API token
curl -X POST http://localhost:5000/api/rbac/tokens \
  -H "Authorization: Bearer <jwt-token>" \
  -H "Content-Type: application/json" \
  -d '{"name": "CI/CD Pipeline"}'

# Use API token
curl http://localhost:5000/api/collections \
  -H "Authorization: Bearer jsondb_pat_xyzabc123..."
```

## Security Considerations
1. Tokens are hashed before storage (like passwords)
2. Token shown only once during creation
3. Tokens can be revoked immediately
4. Optional expiration dates
5. Usage tracking for audit trails
6. Tokens inherit user's permissions

## Migration Path
1. Existing JWT authentication continues to work
2. API tokens are opt-in
3. Both authentication methods can coexist
4. Gradual migration possible

## Benefits
1. No need to store credentials in applications
2. Fine-grained permission control per token
3. Easy revocation without changing passwords
4. Better security for automated systems
5. Audit trail of token usage