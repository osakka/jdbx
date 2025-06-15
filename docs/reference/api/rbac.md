# RBAC Technical Reference

Complete technical reference for JDBX's Role-Based Access Control system.

## Table of Contents
1. [System Architecture](#system-architecture)
2. [Database Schema](#database-schema)
3. [Permission Model](#permission-model)
4. [JWT Token Structure](#jwt-token-structure)
5. [Session Management](#session-management)
6. [API Token System](#api-token-system)
7. [Security Implementation](#security-implementation)
8. [Performance Considerations](#performance-considerations)
9. [Configuration Reference](#configuration-reference)

## System Architecture

### Component Overview

```
RBAC Architecture
├── Authentication Layer
│   ├── Password validation (bcrypt with salt)
│   ├── JWT token generation and verification
│   ├── Session tracking and expiration
│   └── API token management
├── Authorization Layer
│   ├── Permission resolution
│   ├── Role-based access control
│   ├── Collection-level security
│   └── Resource-specific permissions
├── Data Layer
│   ├── User collection (_users)
│   ├── Role collection (_roles)
│   ├── Session collection (_sessions)
│   ├── Permission cache (_permission_cache)
│   └── Audit logging
└── API Layer
    ├── Authentication endpoints
    ├── User management endpoints
    ├── Role management endpoints
    └── Token management endpoints
```

### Core Components

**Authentication Handler** (`src/components/core/authentication_handler.c`)
- Validates credentials against user database
- Generates JWT tokens with appropriate claims
- Manages session lifecycle

**RBAC Core** (`src/components/rbac/rbac.c`)
- Permission checking and validation
- Role resolution and inheritance
- Access control decision engine

**JWT Implementation** (`src/components/rbac/jwt.c`)
- Token generation and verification
- Claims processing and validation
- Token expiration handling

**Session Management** (`src/components/rbac/rbac_sessions.c`)
- Active session tracking
- Session expiration and cleanup
- Concurrent session limits

## Database Schema

### Users Collection (_users)

```json
{
  "_id": "doc-timestamp-random",
  "username": "string (unique)",
  "cn": "string (common name/display name)",
  "email": "string (email address)",
  "password_hash": "string (bcrypt hash)",
  "roles": ["array of role IDs"],
  "active": "boolean (account status)",
  "created_at": "ISO8601 timestamp",
  "updated_at": "ISO8601 timestamp",
  "last_login": "ISO8601 timestamp (optional)",
  "failed_login_attempts": "number (security tracking)"
}
```

**Field Specifications:**
- `username`: 3-50 characters, alphanumeric + underscore
- `password_hash`: bcrypt with cost factor 10
- `roles`: Array of role document IDs
- `active`: Disabled users cannot authenticate

### Roles Collection (_roles)

```json
{
  "_id": "doc-timestamp-random",
  "name": "string (unique identifier)",
  "cn": "string (display name)",
  "description": "string (role purpose)",
  "permissions": {
    "collections": {
      "collection_name": ["CREATE", "READ", "UPDATE", "DELETE", "ADMIN"],
      "*": ["permission_array"],
      "pattern_*": ["permission_array"]
    },
    "system": ["*"] | ["specific_permissions"],
    "0:*": 15,  // Database permissions (bitfield)
    "1:*": 15,  // Collection permissions (bitfield)
    "2:*": 15,  // Document permissions (bitfield)
    "3:*": 15,  // Role permissions (bitfield)
    "4:*": 15,  // User permissions (bitfield)
    "5:*": 15   // Permission permissions (bitfield)
  },
  "created_at": "ISO8601 timestamp",
  "updated_at": "ISO8601 timestamp"
}
```

**Permission Bitfield Values:**
- `1` = CREATE
- `2` = READ  
- `4` = UPDATE
- `8` = DELETE
- `15` = ALL (CREATE + READ + UPDATE + DELETE)

### Sessions Collection (_sessions)

```json
{
  "_id": "doc-timestamp-random",
  "user_id": "string (user document ID)",
  "token": "string (JWT token)",
  "username": "string (denormalized for performance)",
  "created_at": "ISO8601 timestamp",
  "last_seen": "ISO8601 timestamp",
  "expires_at": "ISO8601 timestamp",
  "active": "boolean",
  "ip_address": "string (optional)",
  "user_agent": "string (optional)"
}
```

**Session Lifecycle:**
- Created on successful authentication
- Updated on each API request (`last_seen`)
- Expired after 30 minutes of inactivity
- Cleaned up by background process

### Permission Cache (_permission_cache)

```json
{
  "_id": "user_id:collection_name",
  "user_id": "string",
  "collection_name": "string",
  "permissions": ["array of permissions"],
  "resolved_at": "ISO8601 timestamp",
  "expires_at": "ISO8601 timestamp"
}
```

**Cache Behavior:**
- TTL: 300 seconds (5 minutes)
- Invalidated on role changes
- Reduces permission resolution overhead

## Permission Model

### Permission Types

| Permission | Value | Description |
|------------|-------|-------------|
| CREATE     | 1     | Create new documents |
| READ       | 2     | Read existing documents |
| UPDATE     | 4     | Modify existing documents |
| DELETE     | 8     | Remove documents |
| ADMIN      | 15    | Full administrative access |

### Permission Resolution Algorithm

```
1. Load user's assigned roles
2. For each role, resolve permissions for target collection
3. Apply permission inheritance rules
4. Check collection-specific overrides
5. Apply wildcard patterns
6. Cache resolved permissions
7. Return final permission set
```

### Collection Pattern Matching

**Exact Match:**
```json
{
  "collections": {
    "users": ["READ", "UPDATE"]
  }
}
```

**Wildcard Patterns:**
```json
{
  "collections": {
    "*": ["READ"],                    // All collections
    "public_*": ["CREATE", "READ"],   // Prefix match
    "*_logs": ["READ"]                // Suffix match
  }
}
```

**Pattern Priority (highest to lowest):**
1. Exact collection name
2. Specific patterns (prefix/suffix)
3. Wildcard (`*`)
4. Default deny

### System Permissions

**Full System Access:**
```json
{
  "system": ["*"]
}
```

**Specific System Permissions:**
```json
{
  "system": ["metrics", "admin", "backup"]
}
```

## JWT Token Structure

### Token Claims

**Standard Claims:**
```json
{
  "iss": "jdbx",
  "sub": "user_document_id",
  "exp": 1234567890,
  "iat": 1234567890,
  "jti": "unique_token_id"
}
```

**Custom Claims:**
```json
{
  "username": "user_login_name",
  "type": "access" | "refresh" | "api",
  "session_id": "session_document_id",
  "permissions": {
    "collections": {
      "cached_permissions": "for_performance"
    }
  }
}
```

### Token Types

**Access Tokens:**
- Expiration: 30 minutes
- Used for API authentication
- Contains cached permissions
- Automatically refreshed

**API Tokens:**
- Expiration: Configurable (default: 1 year)
- Used for programmatic access
- No automatic refresh
- Can be revoked manually

### Token Validation

```c
// Token validation process
1. Verify JWT signature
2. Check expiration time
3. Validate issuer and format
4. Look up session in database
5. Verify user account status
6. Cache validation result
```

## Session Management

### Session Lifecycle

**Creation:**
```
1. User authenticates successfully
2. Generate JWT access token
3. Create session record in database
4. Set session expiration (30 minutes)
5. Return token to client
```

**Validation:**
```
1. Extract token from Authorization header
2. Verify JWT signature and claims
3. Look up session in database
4. Check session expiration
5. Update last_seen timestamp
6. Return user context
```

**Expiration:**
```
1. Background process runs every 5 minutes
2. Find sessions with expires_at < now
3. Mark sessions as inactive
4. Clean up old session records
```

### Session Security

**Concurrent Session Limits:**
- Default: 5 active sessions per user
- Configurable via environment variable
- Oldest sessions automatically terminated

**Session Invalidation:**
- Password changes invalidate all sessions
- Role changes invalidate affected sessions
- Manual logout invalidates specific session
- Admin can terminate any session

## API Token System

### Token Generation

```c
// API token generation process
1. User requests API token
2. Verify user permissions for token creation
3. Generate long-lived JWT
4. Store token metadata in database
5. Return token (displayed once only)
```

### Token Management

**Token Metadata:**
```json
{
  "_id": "token_document_id",
  "user_id": "associated_user_id",
  "name": "token_description",
  "token_hash": "sha256_hash_of_token",
  "created_at": "ISO8601_timestamp",
  "expires_at": "ISO8601_timestamp",
  "last_used": "ISO8601_timestamp",
  "active": "boolean",
  "permissions": "inherited_from_user_roles"
}
```

**Token Revocation:**
- Immediate: Mark token as inactive
- Batch: Clean up expired tokens
- Security: Emergency revocation of all tokens

## Security Implementation

### Password Security

**Hashing:**
- Algorithm: bcrypt
- Cost factor: 10 (adjustable)
- Salt: Automatically generated per password
- Minimum length: 8 characters

**Password Validation:**
```c
int validate_password(const char* password, const char* hash) {
    return bcrypt_checkpw(password, hash) == 0;
}
```

### Token Security

**JWT Signature:**
- Algorithm: HS256 (HMAC-SHA256)
- Secret: 256-bit random key
- Key rotation: Manual process

**Token Storage:**
- Server: Only metadata stored
- Client: Full token in memory/localStorage
- Database: Hash of token for revocation

### Access Control

**Permission Checking:**
```c
bool check_permission(user_t* user, const char* collection, permission_t perm) {
    // 1. Load user roles
    // 2. Resolve collection permissions
    // 3. Check against required permission
    // 4. Return authorization decision
}
```

**Rate Limiting:**
- Failed logins: 5 attempts per 15 minutes
- API requests: 1000 requests per hour
- Token generation: 10 tokens per day

## Performance Considerations

### Caching Strategy

**Permission Cache:**
- TTL: 5 minutes
- Invalidation: On role changes
- Memory usage: ~1KB per user per collection

**Session Cache:**
- In-memory session lookup
- Periodic database sync
- LRU eviction policy

### Database Optimization

**Indexes:**
```sql
-- Users collection
CREATE INDEX idx_users_username ON _users(username)
CREATE INDEX idx_users_active ON _users(active)

-- Sessions collection  
CREATE INDEX idx_sessions_token ON _sessions(token)
CREATE INDEX idx_sessions_expires ON _sessions(expires_at)
CREATE INDEX idx_sessions_user ON _sessions(user_id)

-- Roles collection
CREATE INDEX idx_roles_name ON _roles(name)
```

**Query Optimization:**
- User lookup by username (O(1) with index)
- Session validation by token (O(1) with index)
- Permission resolution (cached)

### Memory Usage

**Per User:**
- User object: ~500 bytes
- Active session: ~200 bytes
- Permission cache: ~1KB per collection

**Total System:**
- 1000 users ≈ 1.7MB base memory
- Plus permission cache expansion
- Plus active session storage

## Configuration Reference

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `JDBX_JWT_SECRET` | (random) | JWT signing secret |
| `JDBX_SESSION_TIMEOUT` | 1800 | Session timeout (seconds) |
| `JDBX_MAX_SESSIONS` | 5 | Max sessions per user |
| `JDBX_BCRYPT_COST` | 10 | Password hashing cost |
| `JDBX_TOKEN_CLEANUP_INTERVAL` | 300 | Token cleanup interval (seconds) |

### Database Configuration

**RBAC Collections:**
```json
{
  "_users": {
    "validation": "strict",
    "indexes": ["username", "active"]
  },
  "_roles": {
    "validation": "strict", 
    "indexes": ["name"]
  },
  "_sessions": {
    "validation": "relaxed",
    "indexes": ["token", "expires_at", "user_id"],
    "ttl": 3600
  }
}
```

### Security Settings

**Password Policy:**
```json
{
  "min_length": 8,
  "require_uppercase": false,
  "require_lowercase": false,
  "require_numbers": false,
  "require_symbols": false,
  "max_age_days": 0,
  "history_count": 0
}
```

**Rate Limiting:**
```json
{
  "login_attempts": {
    "max": 5,
    "window_seconds": 900,
    "lockout_seconds": 900
  },
  "api_requests": {
    "max": 1000,
    "window_seconds": 3600
  }
}
```

## Related Documentation

- [RBAC Setup Guide](../guides/rbac-setup.md)
- [RBAC API Reference](../api/rbac-api.md)
- [Authentication Guide](../guides/authentication-guide.md)
- [Security Guidelines](../security/security-guidelines.md)