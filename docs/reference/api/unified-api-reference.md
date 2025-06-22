# JDBX Unified API Reference (v7.0.4)

**Complete API documentation for JDBX TRUE Unified Documents Architecture**

## 🏆 Overview

JDBX v7.0.4 implements enterprise-grade unified documents architecture where **ALL entities** (users, roles, libraries, configs, metrics) are stored as documents in a single collection. This provides unprecedented simplicity while maintaining enterprise-grade performance with 100% thread safety and cryptographic security.

### 🎯 Key Concepts

- **Single Physical Collection**: All documents stored in `default/documents`
- **Field-Based Discrimination**: Documents distinguished by `type`, `library`, `collection` fields
- **Virtual Collections**: Logical groupings based on document properties
- **Mandatory Document Fields**: Every document must include specific system fields

## 📋 Mandatory Document Fields

Every document in JDBX must include these system fields:

```json
{
  "uuid": "doc-1750019283-123456789",     // Auto-generated unique identifier
  "type": "user",                         // Document type (user, role, library, config, etc.)
  "library": "system",                    // Virtual library scope (system, default, username)
  "collection": "users",                  // Virtual collection name
  "owner": "admin",                       // Document owner for security/audit
  "created_at": 1750019283,              // Unix timestamp of creation
  "modified_at": 1750019283              // Unix timestamp of last modification
}
```

## 🔗 Core Unified Endpoints

### GET /api/documents

**Primary unified documents endpoint** - Access all documents with optional filtering.

#### Request
```http
GET /api/documents?type=user&library=system&limit=10
Authorization: Bearer <jwt_token>
```

#### Query Parameters
- `type` (optional): Filter by document type (user, role, library, config, etc.)
- `library` (optional): Filter by virtual library scope
- `collection` (optional): Filter by virtual collection name  
- `owner` (optional): Filter by document owner
- `limit` (optional): Maximum number of documents to return
- `offset` (optional): Number of documents to skip
- `sort` (optional): Sort field (default: modified_at)
- `order` (optional): Sort order (asc, desc - default: desc)

#### Response
```json
{
  "documents": [
    {
      "uuid": "doc-1750019283-123456789",
      "type": "user",
      "library": "system",
      "collection": "users", 
      "owner": "admin",
      "username": "admin",
      "email": "admin@example.com",
      "roles": ["admin"],
      "created_at": 1750019283,
      "modified_at": 1750019283
    }
  ],
  "count": 1,
  "total": 1,
  "has_more": false
}
```

### POST /api/documents

**Create new document** in unified storage.

#### Request
```http
POST /api/documents
Authorization: Bearer <jwt_token>
Content-Type: application/json

{
  "type": "config",
  "library": "default",
  "collection": "settings",
  "owner": "admin", 
  "name": "database_config",
  "settings": {
    "max_connections": 100,
    "timeout": 30
  }
}
```

#### Response
```json
{
  "uuid": "doc-1750019568-987654321",
  "type": "config", 
  "library": "default",
  "collection": "settings",
  "owner": "admin",
  "name": "database_config",
  "settings": {
    "max_connections": 100,
    "timeout": 30
  },
  "created_at": 1750019568,
  "modified_at": 1750019568
}
```

### GET /api/documents/{uuid}

**Retrieve specific document** by UUID.

#### Request
```http
GET /api/documents/doc-1750019283-123456789
Authorization: Bearer <jwt_token>
```

#### Response
```json
{
  "uuid": "doc-1750019283-123456789",
  "type": "user",
  "library": "system", 
  "collection": "users",
  "owner": "admin",
  "username": "admin",
  "email": "admin@example.com",
  "roles": ["admin"],
  "created_at": 1750019283,
  "modified_at": 1750019283
}
```

### PUT /api/documents/{uuid}

**Update existing document** by UUID.

#### Request
```http
PUT /api/documents/doc-1750019283-123456789
Authorization: Bearer <jwt_token>
Content-Type: application/json

{
  "email": "new-admin@example.com",
  "last_login": 1750019600
}
```

#### Response
```json
{
  "uuid": "doc-1750019283-123456789",
  "type": "user",
  "library": "system",
  "collection": "users", 
  "owner": "admin",
  "username": "admin",
  "email": "new-admin@example.com",
  "roles": ["admin"],
  "last_login": 1750019600,
  "created_at": 1750019283,
  "modified_at": 1750019600
}
```

### DELETE /api/documents/{uuid}

**Delete document** by UUID.

#### Request
```http
DELETE /api/documents/doc-1750019283-123456789
Authorization: Bearer <jwt_token>
```

#### Response
```http
HTTP/1.1 204 No Content
```

## 🗂️ Virtual Collections API

Virtual collections provide logical groupings of documents based on their `type` and `collection` fields.

### GET /api/collections

**List all virtual collections** with document counts.

#### Request
```http
GET /api/collections
Authorization: Bearer <jwt_token>
```

#### Response
```json
{
  "collections": [
    {
      "name": "users",
      "library": "default", 
      "path": "default/users",
      "document_count": 5,
      "created_at": "2025-06-15T10:30:00Z"
    },
    {
      "name": "roles",
      "library": "default",
      "path": "default/roles", 
      "document_count": 3,
      "created_at": "2025-06-15T10:30:00Z"
    }
  ]
}
```

### POST /api/collections

**Create virtual collection** metadata document.

#### Request
```http
POST /api/collections
Authorization: Bearer <jwt_token>
Content-Type: application/json

{
  "name": "products",
  "library": "default",
  "description": "Product catalog collection",
  "schema": {
    "type": "object",
    "properties": {
      "name": {"type": "string"},
      "price": {"type": "number"}
    }
  }
}
```

#### Response
```json
{
  "success": true,
  "name": "products",
  "library": "default", 
  "message": "Virtual collection created successfully"
}
```

### DELETE /api/collections/{library}/{collection}

**Delete virtual collection** metadata (documents remain).

#### Request
```http
DELETE /api/collections/default/products
Authorization: Bearer <jwt_token>
```

#### Response
```json
{
  "success": true,
  "name": "products",
  "library": "default",
  "message": "Virtual collection deleted successfully"
}
```

## 📚 Libraries API

Libraries provide virtual namespace isolation for multi-tenancy.

### GET /api/libraries

**List all libraries** (returned as documents with type="library").

#### Request
```http
GET /api/libraries
Authorization: Bearer <jwt_token>
```

#### Response
```json
{
  "libraries": [
    {
      "uuid": "doc-1750019283-555555555",
      "type": "library",
      "name": "system", 
      "template": "system",
      "owner": "admin",
      "description": "System library for internal operations",
      "created_at": 1750019283,
      "modified_at": 1750019283
    },
    {
      "uuid": "doc-1750019283-666666666", 
      "type": "library",
      "name": "default",
      "template": "standard", 
      "owner": "admin",
      "description": "Default library for general use",
      "created_at": 1750019283,
      "modified_at": 1750019283
    }
  ]
}
```

### POST /api/libraries

**Create new library** as unified document.

#### Request
```http
POST /api/libraries
Authorization: Bearer <jwt_token>
Content-Type: application/json

{
  "name": "ecommerce",
  "template": "standard",
  "description": "E-commerce data library"
}
```

#### Response
```json
{
  "success": true,
  "uuid": "doc-1750019700-777777777",
  "name": "ecommerce", 
  "template": "standard",
  "message": "Library created successfully"
}
```

## 🔐 Authentication & Sessions

Authentication creates and manages users as unified documents.

### POST /api/auth/login

**Authenticate user** and create session documents.

#### Request
```http
POST /api/auth/login
Content-Type: application/json

{
  "username": "admin",
  "password": "admin", 
  "library": "system"
}
```

#### Response
```json
{
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "refresh_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...", 
  "user_id": "doc-1750019283-123456789",
  "username": "admin",
  "expires_in": 1800,
  "library": "system"
}
```

### GET /api/auth/me

**Get current user** document information.

#### Request
```http
GET /api/auth/me
Authorization: Bearer <jwt_token>
```

#### Response
```json
{
  "uuid": "doc-1750019283-123456789",
  "type": "user",
  "library": "system",
  "collection": "users",
  "username": "admin", 
  "email": "admin@example.com",
  "roles": ["admin"],
  "created_at": 1750019283,
  "modified_at": 1750019283
}
```

## 🔍 Advanced Querying

### Complex Filters

Query documents with complex JSON filters:

```http
GET /api/documents
Authorization: Bearer <jwt_token>
Content-Type: application/json

{
  "type": "user",
  "library": "system", 
  "$and": [
    {"roles": {"$contains": "admin"}},
    {"created_at": {"$gte": 1750000000}}
  ]
}
```

### Field Projection

Request specific fields only:

```http
GET /api/documents?type=user&fields=username,email,roles
Authorization: Bearer <jwt_token>
```

Response:
```json
{
  "documents": [
    {
      "uuid": "doc-1750019283-123456789",
      "username": "admin",
      "email": "admin@example.com", 
      "roles": ["admin"]
    }
  ],
  "count": 1
}
```

## 📊 Document Types Reference

### User Documents
```json
{
  "type": "user",
  "library": "system",
  "collection": "users",
  "username": "john",
  "email": "john@example.com",
  "password_hash": "...",
  "roles": ["user"],
  "profile": {
    "first_name": "John",
    "last_name": "Doe"
  }
}
```

### Role Documents
```json
{
  "type": "role", 
  "library": "system",
  "collection": "roles",
  "name": "editor",
  "permissions": {
    "documents": ["read", "create", "update"],
    "collections": ["read"]
  },
  "description": "Content editor role"
}
```

### Library Documents
```json
{
  "type": "library",
  "library": "default", 
  "collection": "documents",
  "name": "customer_data",
  "template": "standard",
  "description": "Customer information library",
  "settings": {
    "max_collections": 100,
    "retention_days": 365
  }
}
```

### Config Documents
```json
{
  "type": "config",
  "library": "system",
  "collection": "configs", 
  "name": "server_settings",
  "settings": {
    "max_connections": 100,
    "timeout": 30,
    "ssl_enabled": true
  }
}
```

### Session Documents  
```json
{
  "type": "session",
  "library": "system",
  "collection": "sessions",
  "user_id": "doc-1750019283-123456789",
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "expires_at": 1750021083,
  "ip_address": "192.168.1.100",
  "user_agent": "Mozilla/5.0..."
}
```

## ⚠️ Error Responses

### 400 Bad Request
```json
{
  "error": "Document missing required fields (type, owner)",
  "details": {
    "missing_fields": ["type", "owner"]
  }
}
```

### 401 Unauthorized
```json
{
  "error": "Invalid or expired JWT token"
}
```

### 403 Forbidden
```json
{
  "error": "Insufficient permissions for system collection access"
}
```

### 404 Not Found
```json
{
  "error": "Document not found",
  "uuid": "doc-1750019283-123456789"
}
```

### 409 Conflict
```json
{
  "error": "Document with same identifier already exists"
}
```

## 🚀 Migration from v5.x

### API Endpoint Changes

**Old (v5.x hierarchical):**
```http
GET /api/collections/system/users
POST /api/collections/system/users
```

**New (v6.0 unified):**
```http
GET /api/documents?type=user&library=system&collection=users
POST /api/documents
```

### Response Format Changes

**Old response:**
```json
{
  "users": [...]
}
```

**New response:**
```json
{
  "documents": [...],
  "count": 5
}
```

## 🔧 Best Practices

1. **Always include mandatory fields** when creating documents
2. **Use type filtering** for entity-specific queries  
3. **Leverage virtual collections** for logical organization
4. **Include proper error handling** for field validation
5. **Use UUID-based references** for document relationships
6. **Implement proper authentication** for all operations

---

*For more information, see:*
- `CLAUDE.md` - Development guidelines
- `docs/TRUE_UNIFIED_DOCUMENTS_ARCHITECTURE.md` - Architecture guide
- `docs/MIGRATION_GUIDE_v6.md` - Migration guide