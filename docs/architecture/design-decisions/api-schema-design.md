# JDBX API Schema Design - Gold Standard v6.3.0

## Core Principles

1. **Everything is a Document**: All entities (users, roles, libraries, sessions, configs) are documents
2. **Unified Storage**: Single physical collection with type-based discrimination
3. **RBAC Protection**: All endpoints except health and login require authentication
4. **Library Context**: All operations are library-scoped (default, system, or user libraries)
5. **RESTful Design**: Consistent HTTP methods and URL patterns

## API Endpoint Categories

### 1. **Public Endpoints** (No Authentication)
```
GET  /health                          # Server health check
GET  /api/health                      # Detailed health with metrics
POST /api/auth/login                  # User authentication
GET  /api/openapi.json               # API documentation
```

### 2. **Authentication & Session Management** (Mixed)
```
POST /api/auth/login                  # Login (public)
POST /api/auth/logout                 # Logout (authenticated)
POST /api/auth/refresh                # Refresh token (authenticated)
GET  /api/auth/session                # Current session info (authenticated)
POST /api/auth/session/library        # Switch library context (authenticated)
GET  /api/sessions                    # List all sessions (admin only)
DELETE /api/sessions/:id              # Terminate session (admin only)
```

### 3. **Unified Documents API** (Authenticated)
Primary interface for ALL document operations:
```
# Query documents with filters
GET  /api/documents?type=user&library=system&field=value

# CRUD operations
POST   /api/documents                 # Create (auto-generates UUID)
GET    /api/documents/:uuid           # Read by UUID
PUT    /api/documents/:uuid           # Update (full replacement)
PATCH  /api/documents/:uuid           # Update (partial)
DELETE /api/documents/:uuid           # Delete

# Field-level operations
GET    /api/documents/:uuid/:field    # Get specific field
PATCH  /api/documents/:uuid/:field    # Update specific field
DELETE /api/documents/:uuid/:field    # Remove field
```

### 4. **Virtual Collections API** (Authenticated)
For backwards compatibility and convenience:
```
# Collection management (virtual views)
GET  /api/collections                 # List available collections
GET  /api/collections/:name/schema    # Get collection schema

# Document operations via collection
GET    /api/collections/:name/documents
POST   /api/collections/:name/documents  
GET    /api/collections/:name/documents/:id
PUT    /api/collections/:name/documents/:id
DELETE /api/collections/:name/documents/:id

# These map to unified storage:
# - Collection "users" → type="user"
# - Collection "roles" → type="role"
# - Collection "libraries" → type="library"
```

### 5. **Library Management** (Authenticated)
Libraries are documents with type="library":
```
GET    /api/libraries                 # List libraries (query unified storage)
POST   /api/libraries                 # Create library
GET    /api/libraries/:name           # Get library details
PUT    /api/libraries/:name           # Update library
DELETE /api/libraries/:name           # Delete library (admin only)

# Library-scoped operations
GET  /api/libraries/:name/collections # Virtual collections in library
GET  /api/libraries/:name/users       # Users with access to library
GET  /api/libraries/:name/metrics     # Library-specific metrics
```

### 6. **RBAC Management** (Authenticated)
Users and roles are documents:
```
# User management (type="user")
GET    /api/users                     # List users
POST   /api/users                     # Create user
GET    /api/users/:username           # Get user
PUT    /api/users/:username           # Update user
DELETE /api/users/:username           # Delete user

# Role management (type="role")  
GET    /api/roles                     # List roles
POST   /api/roles                     # Create role
GET    /api/roles/:name               # Get role
PUT    /api/roles/:name               # Update role
DELETE /api/roles/:name               # Delete role

# Permission assignment
POST   /api/users/:username/roles     # Assign role to user
DELETE /api/users/:username/roles/:role # Remove role from user
GET    /api/permissions               # List all permissions
GET    /api/permissions/check         # Check current user permissions
```

### 7. **Metrics & Monitoring** (Mixed)
```
# Public metrics
GET  /metrics                         # Prometheus format (public)
GET  /api/metrics/health              # Basic health metrics (public)

# Authenticated metrics
GET  /api/metrics                     # Comprehensive metrics
GET  /api/metrics/history             # Time-series data
GET  /api/metrics/libraries/:name     # Library-specific metrics
POST /api/metrics/query               # Custom metric queries
GET  /api/metrics/indexes             # Index performance metrics
```

### 8. **System Management** (Admin Only)
```
GET  /api/system/info                 # System information
GET  /api/system/config               # Current configuration
PUT  /api/system/config               # Update configuration
GET  /api/system/logs                 # System logs
POST /api/system/maintenance          # Maintenance operations
```

## Authentication & Authorization

### JWT Token Structure
```json
{
  "iss": "jdbx",
  "sub": "user-uuid",
  "exp": 1234567890,
  "iat": 1234567890,
  "username": "john",
  "library": "default",      // Current library context
  "libraries": ["default", "johndoe"],  // Accessible libraries
  "roles": ["user", "developer"],
  "type": "access"
}
```

### Authorization Rules
1. **Public Endpoints**: No token required
2. **Authenticated Endpoints**: Valid JWT required
3. **Library Scoping**: Operations limited to current library context
4. **Admin Operations**: Require admin role
5. **Cross-Library Access**: Requires explicit permissions

## Request/Response Standards

### Request Headers
```
Authorization: Bearer <jwt-token>
Content-Type: application/json
X-Library-Context: <library-name>    # Optional library override
```

### Standard Query Parameters
```
?library=default                     # Filter by library
?type=user                          # Filter by document type
?owner=john                         # Filter by owner
?fields=name,email                  # Field projection
?sort=created_at:desc               # Sorting
?limit=20&offset=0                  # Pagination
?q=search+term                      # Full-text search
```

### Standard Response Format
```json
{
  "success": true,
  "data": { /* response data */ },
  "meta": {
    "count": 100,
    "offset": 0,
    "limit": 20,
    "total": 500
  },
  "library": "default",
  "timestamp": 1234567890
}
```

### Error Response Format
```json
{
  "success": false,
  "error": {
    "code": "PERMISSION_DENIED",
    "message": "You don't have access to this resource",
    "details": { /* additional context */ }
  },
  "timestamp": 1234567890
}
```

## Implementation Priority

### Phase 1: Core Infrastructure ✅
- [x] Unified documents storage
- [x] Basic authentication
- [x] Health endpoints
- [ ] Proper error handling

### Phase 2: Document Operations
- [ ] Full unified documents API
- [ ] Field-level operations
- [ ] Query optimization
- [ ] Virtual collections mapping

### Phase 3: RBAC & Security
- [ ] Complete RBAC implementation
- [ ] Permission checking middleware
- [ ] Library context switching
- [ ] Audit logging

### Phase 4: Advanced Features
- [ ] Metrics aggregation
- [ ] Transaction support
- [ ] JavaScript integration
- [ ] Real-time subscriptions

## Migration Notes

### From Legacy to Unified
- `/api/libraries/X/collections/Y/documents` → `/api/documents?library=X&type=Y`
- Physical collections → Virtual collections (type field)
- Hierarchical paths → Flat document storage
- Collection-specific logic → Type-based handlers

### Backwards Compatibility
- Keep legacy endpoints during transition
- Map legacy routes to unified handlers
- Deprecation warnings in responses
- Migration guide for clients