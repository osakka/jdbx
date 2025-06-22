# REST API Reference

**Version**: 7.0.1  
**Last Updated**: June 22, 2025  
**Base URL**: `https://localhost:5000/api` (SSL enabled by default)

Complete REST API documentation for JDBX v7.0.1 with memory checkpoint safety enhancements, integrated WAL architecture, HTTP protocol compliance, enterprise-grade authentication security excellence, collection ownership protection, and revolutionary memory management.

## Authentication

JDBX uses JWT (JSON Web Tokens) for authentication with database-backed RBAC.

### Login
```http
POST /api/login
Content-Type: application/json

{
  "username": "admin", 
  "password": "secure_password"
}
```

**Response:**
```json
{
  "success": true,
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "expires_at": "2025-06-13T10:00:00Z",
  "user": {
    "id": "user-123-456",
    "username": "admin",
    "roles": ["admin"],
    "library": "default"
  }
}
```

### Token Usage
Include JWT token in all authenticated requests:
```http
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

## 🔒 Security Model

JDBX implements enterprise-grade collection and document ownership security:

### Collection Access Control
- **System Collections**: Admin-only access for `system/*` collections (users, roles, sessions, etc.)
- **User Namespaces**: Users restricted to `default` library or their own `username` library
- **Cross-Library Access**: Requires explicit RBAC permissions

### Permission Hierarchy
```
Admin Users:
  ✅ Full access to all libraries and collections
  ✅ Can create/modify/delete system collections
  ✅ Can access any user's data

Regular Users:
  ✅ Can access default library collections
  ✅ Can create collections in default or username library
  ❌ Cannot access system collections for write operations
  ❌ Cannot access other users' namespace libraries
  ❌ Cannot create system_* or _system* named collections
```

### Error Responses
- `401 Unauthorized`: Missing or invalid JWT token
- `403 Forbidden`: Valid token but insufficient permissions
- `404 Not Found`: Collection/document doesn't exist or no access

### Token Refresh
```http
POST /api/refresh
Authorization: Bearer <current_token>
```

### Change Password
```http
PUT /api/auth/password
Authorization: Bearer <current_token>
Content-Type: application/json

{
  "current_password": "old_password",
  "new_password": "new_secure_password"
}
```

**Response:**
```json
{
  "success": true,
  "message": "Password changed successfully"
}
```

## Document Operations

### Create Document
```http
POST /api/collections/{collection}/documents
Authorization: Bearer <token>
Content-Type: application/json

{
  "name": "John Doe",
  "email": "john@example.com",
  "age": 30,
  "profile": {
    "bio": "Software developer",
    "skills": ["JavaScript", "Python", "C"]
  }
}
```

**Response:**
```json
{
  "success": true,
  "document": {
    "_id": "doc-1733472000-abc123",
    "name": "John Doe",
    "email": "john@example.com",
    "age": 30,
    "profile": {
      "bio": "Software developer", 
      "skills": ["JavaScript", "Python", "C"]
    },
    "_created_at": 1718234400,
    "_modified_at": 1718234400
  }
}
```

### Query Documents
```http
GET /api/collections/{collection}/documents
Authorization: Bearer <token>

# Query parameters
?query={"age": {"$gte": 18}}
&limit=50
&skip=0
&sort={"name": 1}
&projection={"name": 1, "email": 1}
```

**Advanced Query Examples:**
```bash
# Simple equality
GET /api/collections/users/documents?query={"name": "John Doe"}

# Range query
GET /api/collections/users/documents?query={"age": {"$gte": 18, "$lt": 65}}

# Array contains
GET /api/collections/users/documents?query={"skills": {"$in": ["JavaScript"]}}

# Nested field query
GET /api/collections/users/documents?query={"profile.bio": {"$regex": "developer"}}

# Multiple conditions
GET /api/collections/users/documents?query={"$and": [{"age": {"$gte": 18}}, {"email": {"$regex": "@company.com"}}]}
```

### Get Document by ID
```http
GET /api/collections/{collection}/documents/{id}
Authorization: Bearer <token>
```

### Update Document
```http
PUT /api/collections/{collection}/documents/{id}
Authorization: Bearer <token>
Content-Type: application/json

{
  "$set": {
    "email": "newemail@example.com",
    "profile.bio": "Senior Software Developer"
  },
  "$unset": {
    "temporary_field": 1
  },
  "$push": {
    "profile.skills": "Go"
  }
}
```

**Update Operators:**
- `$set`: Set field values
- `$unset`: Remove fields
- `$push`: Add to array
- `$pull`: Remove from array
- `$inc`: Increment numeric value
- `$mul`: Multiply numeric value

### Delete Document
```http
DELETE /api/collections/{collection}/documents/{id}
Authorization: Bearer <token>
```

## Field-Level Operations (v3.3.0)

JDBX v3.3.0 introduces granular field-level operations for efficient document manipulation.

### Read Specific Field
```http
GET /api/collections/{collection}/documents/{id}/fields/{field_path}
Authorization: Bearer <token>
```

**Examples:**
```bash
# Get email field
GET /api/collections/users/documents/doc-123/fields/email

# Get nested field
GET /api/collections/users/documents/doc-123/fields/profile.bio

# Get array element
GET /api/collections/users/documents/doc-123/fields/profile.skills[0]
```

### Update Specific Field
```http
PUT /api/collections/{collection}/documents/{id}/fields/{field_path}
Authorization: Bearer <token>
Content-Type: application/json

{
  "value": "new_field_value"
}
```

### Delete Specific Field
```http
DELETE /api/collections/{collection}/documents/{id}/fields/{field_path}
Authorization: Bearer <token>
```

## Library Management (Multi-Tenant)

JDBX v3.3.0 supports multi-library architecture for tenant isolation.

### List Libraries
```http
GET /api/libraries
Authorization: Bearer <token>
```

### Create Library
```http
POST /api/libraries
Authorization: Bearer <token>
Content-Type: application/json

{
  "name": "tenant1",
  "display_name": "Tenant One",
  "description": "Primary tenant library",
  "settings": {
    "max_collections": 100,
    "max_documents_per_collection": 1000000
  }
}
```

### Switch Session Library
```http
POST /api/session/library
Authorization: Bearer <token>
Content-Type: application/json

{
  "library": "tenant1"
}
```

## Collection Management

### List Collections
```http
GET /api/collections
Authorization: Bearer <token>
```

### Create Collection
```http
POST /api/collections
Authorization: Bearer <token>
Content-Type: application/json

{
  "name": "products",
  "schema": {
    "type": "object",
    "properties": {
      "name": {"type": "string", "required": true},
      "price": {"type": "number", "minimum": 0}
    }
  }
}
```

### Drop Collection
```http
DELETE /api/collections/{collection}
Authorization: Bearer <token>
```

## Index Management

### Create Index
```http
POST /api/indexes/{collection}
Authorization: Bearer <token>
Content-Type: application/json

{
  "field": "email",
  "type": "hash",
  "name": "user_email_idx",
  "unique": true
}
```

**Index Types:**
- `hash`: O(1) exact match lookups
- `btree`: O(log n) range queries
- `compound`: Multi-field indexes

### List Indexes
```http
GET /api/indexes/{collection}
Authorization: Bearer <token>
```

### Drop Index
```http
DELETE /api/indexes/{collection}/{index_name}
Authorization: Bearer <token>
```

## JavaScript Integration

### Execute Custom Function
```http
POST /api/js/functions/{function_name}
Authorization: Bearer <token>
Content-Type: application/json

{
  "args": {
    "param1": "value1",
    "param2": 42,
    "data": [1, 2, 3, 4, 5]
  }
}
```

### Register Validator
```http
POST /api/js/validators
Authorization: Bearer <token>
Content-Type: application/json

{
  "name": "user_validator",
  "collection": "users",
  "code": "function validateUser(doc) { if (!doc.email || !doc.email.includes('@')) { addError('email', 'Invalid email'); } return isValid; }"
}
```

### Register Transformer
```http
POST /api/js/transformers
Authorization: Bearer <token>
Content-Type: application/json

{
  "name": "user_transformer",
  "collection": "users", 
  "code": "function transformUser(doc, operation) { if (operation === 'insert') { doc.created_at = new Date().toISOString(); } return doc; }"
}
```

## User & Role Management (RBAC)

### List Users
```http
GET /api/users
Authorization: Bearer <token>
```

### Create User
```http
POST /api/users
Authorization: Bearer <token>
Content-Type: application/json

{
  "username": "newuser",
  "password": "secure_password",
  "email": "user@example.com",
  "roles": ["user"],
  "library": "default"
}
```

### List Roles
```http
GET /api/roles
Authorization: Bearer <token>
```

### Create Role
```http
POST /api/roles
Authorization: Bearer <token>
Content-Type: application/json

{
  "name": "editor",
  "display_name": "Content Editor",
  "permissions": {
    "collections.read": true,
    "collections.write": true,
    "collections.delete": false,
    "users.read": false
  },
  "field_permissions": {
    "users": {
      "allow": ["name", "email", "profile"],
      "deny": ["password_hash", "admin_notes"]
    }
  }
}
```

## System & Monitoring

### System Information
```http
GET /api/system/info
Authorization: Bearer <token>
```

**Response:**
```json
{
  "version": "6.5.0",
  "architecture": "JDBX Lock-Free",
  "uptime": 3600,
  "memory_usage": "45.2MB",
  "database_size": "128.5MB",
  "thread_pool": {
    "active_threads": 8,
    "max_threads": 16,
    "queue_size": 12
  }
}
```

### Health Check
```http
GET /health
```

### Metrics
```http
GET /api/metrics
Authorization: Bearer <token>
```

**Response:**
```json
{
  "operations": {
    "total_requests": 15420,
    "reads": 12336,
    "writes": 3084,
    "average_response_time": "1.2ms"
  },
  "performance": {
    "lock_free_library_access": "0.1ms",
    "document_insert": "1.2ms",
    "document_query": "0.8ms"
  },
  "cache": {
    "hit_rate": 94.5,
    "size": "25.6MB",
    "entries": 8542
  }
}
```

## Error Responses

All API endpoints return consistent error responses:

```json
{
  "success": false,
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "Document validation failed",
    "details": {
      "field": "email",
      "reason": "Invalid email format"
    }
  }
}
```

**Common Error Codes:**
- `AUTHENTICATION_REQUIRED`: Missing or invalid JWT token
- `PERMISSION_DENIED`: Insufficient permissions for operation
- `VALIDATION_ERROR`: Document validation failed
- `NOT_FOUND`: Document, collection, or resource not found
- `CONFLICT`: Resource already exists or conflict occurred
- `RATE_LIMIT_EXCEEDED`: Too many requests
- `INTERNAL_ERROR`: Server internal error

## Rate Limiting

JDBX implements rate limiting to protect against abuse:

**Default Limits:**
- Authenticated users: 1000 requests/minute
- Anonymous requests: 100 requests/minute
- Admin users: 5000 requests/minute

**Headers:**
```http
X-RateLimit-Limit: 1000
X-RateLimit-Remaining: 987
X-RateLimit-Reset: 1718234460
```

## WebSocket API (Real-time)

For real-time updates, JDBX supports WebSocket connections:

```javascript
const ws = new WebSocket('ws://localhost:5000/api/ws');

// Subscribe to collection changes
ws.send(JSON.stringify({
  action: 'subscribe',
  collection: 'users',
  filter: { age: { $gte: 18 } }
}));

// Receive real-time updates
ws.onmessage = (event) => {
  const update = JSON.parse(event.data);
  console.log('Document updated:', update);
};
```

## OpenAPI Specification

Complete OpenAPI 3.0 specification available at:
```http
GET /api/openapi.json
```

Import this into tools like Postman, Insomnia, or Swagger UI for interactive API exploration.

---

**Need more details?** See the [JavaScript API Reference](javascript-api.md) or [Authentication Guide](../guides/authentication.md).