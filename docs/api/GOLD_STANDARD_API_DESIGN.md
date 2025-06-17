# JDBX Gold Standard API Design

## Core Principles

1. **Everything is a Document** - Users, roles, sessions, metrics, configs are all documents
2. **Library Context** - All operations happen within a library context (default: "default")
3. **Virtual Collections** - Collections are logical groupings via type discrimination
4. **Unified Authentication** - All endpoints except health/login require JWT authentication
5. **RESTful Design** - Consistent REST patterns with proper HTTP methods

## API Structure

### 1. Authentication & Session Management

#### Public Endpoints (No Auth Required)
```
POST   /api/auth/login              # Login with username/password
GET    /api/health                  # Basic health check
GET    /api/health/ready            # Readiness probe
GET    /api/health/live             # Liveness probe
```

#### Session Management (Auth Required)
```
POST   /api/auth/logout             # Logout current session
POST   /api/auth/refresh            # Refresh JWT token
GET    /api/auth/session            # Get current session info
GET    /api/auth/sessions           # List all user sessions
DELETE /api/auth/sessions/:id       # Terminate specific session
POST   /api/auth/library/:lib       # Switch library context
GET    /api/auth/library            # Get current library context
```

### 2. Document Operations (All Auth Required)

#### Unified Document API
```
# Query with type discrimination
GET    /api/documents?type=user&library=system    # Query by type
POST   /api/documents                              # Create document
GET    /api/documents/:uuid                       # Get by UUID
PUT    /api/documents/:uuid                       # Update document
PATCH  /api/documents/:uuid                       # Partial update
DELETE /api/documents/:uuid                       # Delete document

# Batch operations
POST   /api/documents/batch                        # Batch create/update
DELETE /api/documents/batch                        # Batch delete
```

#### Virtual Collection Helpers (Sugar over unified API)
```
# These are convenience endpoints that internally use type discrimination
GET    /api/:type                   # List all of type (users, roles, etc)
POST   /api/:type                   # Create new of type
GET    /api/:type/:id               # Get specific item
PUT    /api/:type/:id               # Update item
DELETE /api/:type/:id               # Delete item

# Examples:
GET    /api/users                   # Internally: /api/documents?type=user
GET    /api/roles                   # Internally: /api/documents?type=role
GET    /api/sessions                # Internally: /api/documents?type=session
```

### 3. Library Management (All Auth Required)

```
GET    /api/libraries               # List all libraries
POST   /api/libraries               # Create new library
GET    /api/libraries/:lib          # Get library details
PUT    /api/libraries/:lib          # Update library
DELETE /api/libraries/:lib          # Delete library

# Library metrics
GET    /api/libraries/:lib/metrics  # Get library metrics
GET    /api/libraries/:lib/stats    # Get library statistics
GET    /api/libraries/:lib/quota    # Get quota usage
```

### 4. Virtual Collections Management (All Auth Required)

```
GET    /api/collections             # List virtual collections (types)
GET    /api/collections/:type       # Get collection metadata
PUT    /api/collections/:type       # Update collection settings
GET    /api/collections/:type/count # Get document count for type
GET    /api/collections/:type/schema# Get schema for type
```

### 5. User & Role Management (All Auth Required)

Since everything is a document, these are just convenience wrappers:

```
# Users (type=user documents)
GET    /api/users                   # List users in current library
POST   /api/users                   # Create user
GET    /api/users/:username         # Get user details
PUT    /api/users/:username         # Update user
DELETE /api/users/:username         # Delete user
POST   /api/users/:username/password# Change password

# Roles (type=role documents)
GET    /api/roles                   # List roles in current library
POST   /api/roles                   # Create role
GET    /api/roles/:name             # Get role details
PUT    /api/roles/:name             # Update role
DELETE /api/roles/:name             # Delete role

# Membership (type=membership documents)
POST   /api/roles/:role/members     # Add user to role
DELETE /api/roles/:role/members/:user # Remove user from role
GET    /api/roles/:role/members     # List role members
GET    /api/users/:user/roles       # List user's roles

# Permissions (type=permission documents)
GET    /api/permissions             # List all permissions
POST   /api/roles/:role/permissions # Grant permission
DELETE /api/roles/:role/permissions/:perm # Revoke permission
GET    /api/roles/:role/permissions # List role permissions
```

### 6. Metrics & Monitoring

#### Public Endpoints
```
GET    /api/metrics                 # Prometheus format metrics
GET    /api/metrics/json            # JSON format metrics
```

#### Authenticated Endpoints
```
GET    /api/metrics/history         # Historical metrics
GET    /api/metrics/aggregate       # Aggregated metrics
POST   /api/metrics/query           # Query metrics
GET    /api/metrics/documents       # Metrics as documents (type=metric)

# System metrics
GET    /api/system/stats            # System statistics
GET    /api/system/info             # System information
GET    /api/system/config           # Current configuration
```

### 7. JavaScript Functions (All Auth Required)

```
# Functions are documents too (type=function)
GET    /api/functions               # List functions
POST   /api/functions               # Create function
GET    /api/functions/:name         # Get function
PUT    /api/functions/:name         # Update function
DELETE /api/functions/:name         # Delete function
POST   /api/functions/:name/execute # Execute function

# Specialized function types
GET    /api/validators              # List validators (type=validator)
POST   /api/validators              # Create validator
GET    /api/transformers            # List transformers (type=transformer)
POST   /api/transformers            # Create transformer
```

### 8. Index Management (All Auth Required)

```
# Indexes are documents (type=index)
GET    /api/indexes                 # List all indexes
POST   /api/indexes                 # Create index
GET    /api/indexes/:name           # Get index details
DELETE /api/indexes/:name           # Delete index
POST   /api/indexes/:name/rebuild   # Rebuild index

# Index operations
GET    /api/indexes/stats           # Index statistics
POST   /api/indexes/analyze         # Analyze query for index usage
```

### 9. Configuration (All Auth Required)

```
# Config entries are documents (type=config)
GET    /api/config                  # Get all config
GET    /api/config/:key             # Get specific config
PUT    /api/config/:key             # Update config
DELETE /api/config/:key             # Delete config
POST   /api/config/reload           # Reload from disk
```

### 10. Import/Export (All Auth Required)

```
POST   /api/export                  # Export data
POST   /api/import                  # Import data
GET    /api/export/formats          # List supported formats
```

## Authentication Flow

1. **Login**: `POST /api/auth/login` with credentials → receive JWT token
2. **Use Token**: Include `Authorization: Bearer <token>` in all requests
3. **Library Context**: Token includes default library, can switch with `/api/auth/library/:lib`
4. **Refresh**: Before expiry, call `/api/auth/refresh` for new token
5. **Logout**: `POST /api/auth/logout` to invalidate session

## Library Context Behavior

1. **Default Library**: "default" if not specified
2. **User Libraries**: Users can have personal libraries (username as library name)
3. **System Library**: "system" for system objects (users, roles, etc.)
4. **Context Switching**: Change active library without re-login
5. **Cross-Library Access**: Requires explicit permissions

## Error Responses

Consistent error format:
```json
{
  "error": {
    "code": "UNIQUE_ERROR_CODE",
    "message": "Human readable message",
    "details": {
      "field": "additional context"
    }
  }
}
```

## Standard HTTP Status Codes

- `200 OK` - Successful GET/PUT/PATCH
- `201 Created` - Successful POST creating resource
- `204 No Content` - Successful DELETE
- `400 Bad Request` - Invalid request data
- `401 Unauthorized` - Missing/invalid authentication
- `403 Forbidden` - Authenticated but not authorized
- `404 Not Found` - Resource doesn't exist
- `409 Conflict` - Resource already exists
- `422 Unprocessable Entity` - Validation failed
- `500 Internal Server Error` - Server error

## Query Parameters

Standard query parameters across all endpoints:

```
?limit=100              # Pagination limit
?offset=0               # Pagination offset
?sort=created_at        # Sort field
?order=asc|desc         # Sort order
?fields=name,uuid       # Field projection
?q=search               # Full-text search
?filter={"age":{"$gt":18}} # JSON filter
```

## Response Envelopes

List responses:
```json
{
  "data": [...],
  "pagination": {
    "offset": 0,
    "limit": 100,
    "total": 250
  }
}
```

Single item responses:
```json
{
  "data": { ... }
}
```

## Implementation Priority

1. **Critical** - Auth endpoints, document CRUD, health
2. **High** - Library management, user/role management, metrics
3. **Medium** - JavaScript functions, configuration, indexes
4. **Low** - Import/export, advanced metrics queries

## Migration Notes

Current implementation gaps to address:
1. Consolidate duplicate user/role endpoints (both /api/users and /api/rbac/users exist)
2. Implement proper library context switching
3. Make all RBAC operations use document interface
4. Add missing session management endpoints
5. Standardize response formats with envelopes
6. Implement batch operations
7. Add field projection support
8. Standardize error responses