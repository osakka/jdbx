# JSONdb REST API Documentation

**Version**: 2.0.6  
**Last Updated**: January 30, 2025

This document provides comprehensive details on the REST API endpoints available in the JSON Database Server.

## Table of Contents

- [Base URL](#base-url)
- [Authentication](#authentication)
- [Authentication Endpoints](#authentication-endpoints)
- [Session Management](#session-management)
- [Collections](#collections)
- [Documents](#documents)
- [User Management](#user-management)
- [Role Management](#role-management)
- [RBAC API](#rbac-api)
- [Metrics](#metrics)
- [System Information](#system-information)
- [Health Check](#health-check)
- [Configuration](#configuration)
- [Schema Validation](#schema-validation)
- [Import/Export](#importexport)
- [Indexes](#indexes)
- [JavaScript Extensions](#javascript-extensions)
- [Transactions](#transactions)
- [Cache Management](#cache-management)
- [OpenAPI Specification](#openapi-specification)
- [Error Responses](#error-responses)

## Base URL

All API endpoints are relative to the base URL of the server:

```
http://localhost:5000
```

## Authentication

The API uses JWT (JSON Web Token) for authentication. Most endpoints require a valid token to be included in the `Authorization` header:

```
Authorization: Bearer <your-jwt-token>
```

**Note**: Some collection and document endpoints currently have authentication disabled for testing purposes. These are marked in the endpoint descriptions.

## Authentication Endpoints

### Register User

Creates a new user account.

- **URL**: `/api/auth/register`
- **Method**: `POST`
- **Auth Required**: No
- **Request Body**:
  ```json
  {
    "username": "string",
    "password": "string"
  }
  ```
- **Success Response**: `201 Created`
  ```json
  {
    "user_id": "string",
    "username": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid request or username already exists
  - `500 Internal Server Error`: Server error

### Login

Authenticates a user and returns a JWT token.

- **URL**: `/api/auth/login`
- **Method**: `POST`
- **Auth Required**: No
- **Request Body**:
  ```json
  {
    "username": "string",
    "password": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "token": "string",
    "user_id": "string",
    "username": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid request
  - `401 Unauthorized`: Invalid credentials
  - `500 Internal Server Error`: Server error

### Refresh Token

Refreshes an expired or expiring JWT token.

- **URL**: `/api/auth/refresh`
- **Method**: `POST`
- **Auth Required**: No
- **Request Body**:
  ```json
  {
    "token": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "token": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid token
  - `401 Unauthorized`: Token expired or invalid
  - `500 Internal Server Error`: Server error

### Logout

Invalidates the current session.

- **URL**: `/api/auth/logout`
- **Method**: `POST`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "message": "Logged out successfully"
  }
  ```

### Admin Login

Special login endpoint for administrative access.

- **URL**: `/api/admin/login`
- **Method**: `POST`
- **Auth Required**: No
- **Request Body**:
  ```json
  {
    "username": "string",
    "password": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "token": "string",
    "user_id": "string",
    "username": "string"
  }
  ```

## Session Management

### List All Sessions

Returns all active sessions (admin only).

- **URL**: `/api/sessions`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "sessions": [
      {
        "id": "string",
        "user_id": "string",
        "username": "string",
        "created_at": "number",
        "last_accessed": "number",
        "expires_at": "number",
        "ip_address": "string",
        "user_agent": "string"
      }
    ]
  }
  ```

### Get Active Sessions

Returns currently active sessions.

- **URL**: `/api/sessions/active`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "sessions": [
      {
        "id": "string",
        "user_id": "string",
        "username": "string",
        "created_at": "number",
        "last_accessed": "number",
        "expires_at": "number",
        "ip_address": "string",
        "user_agent": "string"
      }
    ]
  }
  ```

### Terminate Session

Terminates a specific session.

- **URL**: `/api/sessions/`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "session_id": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Session terminated successfully"
  }
  ```

## Collections

**Note**: Collection endpoints currently have authentication disabled for testing purposes.

### List Collections

Returns all collections in the database.

- **URL**: `/api/collections`
- **Method**: `GET`
- **Auth Required**: No (temporarily disabled)
- **Success Response**: `200 OK`
  ```json
  {
    "collections": ["collection1", "collection2", "..."]
  }
  ```

### Create Collection

Creates a new collection.

- **URL**: `/api/collections`
- **Method**: `POST`
- **Auth Required**: No (temporarily disabled)
- **Request Body**:
  ```json
  {
    "name": "string"
  }
  ```
- **Success Response**: `201 Created`
  ```json
  {
    "name": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid collection name or collection already exists
  - `500 Internal Server Error`: Server error

### Drop Collection

Deletes a collection and all its documents.

- **URL**: `/api/collections/{collection_name}`
- **Method**: `DELETE`
- **Auth Required**: No (temporarily disabled)
- **Success Response**: `204 No Content`
- **Error Responses**:
  - `404 Not Found`: Collection not found
  - `500 Internal Server Error`: Server error

## Documents

**Note**: Document endpoints currently have authentication disabled for testing purposes.

### Query Documents

Queries documents in a collection with optional filters and pagination.

- **URL**: `/api/collections/{collection_name}/documents`
- **Method**: `GET`
- **Auth Required**: No (temporarily disabled)
- **Query Parameters**: 
  - `query` - JSON query object (see Query Language documentation)
  - `page` - Page number (for offset pagination)
  - `page_size` - Items per page (default: 10)
  - `cursor` - Cursor for cursor-based pagination
  - `limit` - Limit for cursor-based pagination
- **Success Response**: `200 OK`
  ```json
  {
    "documents": [
      {
        "_id": "string",
        "...": "document fields"
      }
    ],
    "count": 10,
    "total_count": 100,
    "pagination": {
      "page": 1,
      "page_size": 10,
      "total_pages": 10,
      "total_count": 100,
      "page_count": 10,
      "has_next_page": true,
      "has_prev_page": false,
      "next_cursor": "string",
      "prev_cursor": null
    }
  }
  ```

### Get Document

Retrieves a specific document by ID.

- **URL**: `/api/collections/{collection_name}/documents/{document_id}`
- **Method**: `GET`
- **Auth Required**: No (temporarily disabled)
- **Success Response**: `200 OK`
  ```json
  {
    "_id": "string",
    "...": "document fields"
  }
  ```
- **Error Responses**:
  - `404 Not Found`: Document or collection not found
  - `500 Internal Server Error`: Server error

### Create Document

Creates a new document in a collection.

- **URL**: `/api/collections/{collection_name}/documents`
- **Method**: `POST`
- **Auth Required**: No (temporarily disabled)
- **Request Body**: JSON document
- **Success Response**: `201 Created`
  ```json
  {
    "_id": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid document
  - `404 Not Found`: Collection not found
  - `500 Internal Server Error`: Server error

### Update Document

Updates an existing document (complete replacement).

- **URL**: `/api/collections/{collection_name}/documents/{document_id}`
- **Method**: `PUT`
- **Auth Required**: No (temporarily disabled)
- **Request Body**: Complete JSON document
- **Success Response**: `200 OK`
  ```json
  {
    "_id": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid document
  - `404 Not Found`: Document or collection not found
  - `500 Internal Server Error`: Server error

### Delete Document

Deletes a document from a collection.

- **URL**: `/api/collections/{collection_name}/documents/{document_id}`
- **Method**: `DELETE`
- **Auth Required**: No (temporarily disabled)
- **Success Response**: `204 No Content`
- **Error Responses**:
  - `404 Not Found`: Document or collection not found
  - `500 Internal Server Error`: Server error

## User Management

### List Users

Returns all users (admin only).

- **URL**: `/api/users`
- **Method**: `GET`
- **Auth Required**: Yes (admin)
- **Success Response**: `200 OK`
  ```json
  {
    "users": [
      {
        "id": "string",
        "username": "string"
      }
    ]
  }
  ```

### Get User

Returns details for a specific user.

- **URL**: `/api/users/{user_id}`
- **Method**: `GET`
- **Auth Required**: Yes (admin or owner)
- **Success Response**: `200 OK`
  ```json
  {
    "id": "string",
    "username": "string",
    "roles": ["string"]
  }
  ```

### Create User

Creates a new user (admin only).

- **URL**: `/api/users`
- **Method**: `POST`
- **Auth Required**: Yes (admin)
- **Request Body**:
  ```json
  {
    "username": "string",
    "password": "string"
  }
  ```
- **Success Response**: `201 Created`
  ```json
  {
    "id": "string",
    "username": "string"
  }
  ```

### Update User

Updates user information.

- **URL**: `/api/users/{user_id}`
- **Method**: `PUT`
- **Auth Required**: Yes (admin or owner)
- **Request Body**:
  ```json
  {
    "username": "string",
    "password": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "id": "string",
    "username": "string"
  }
  ```

### Delete User

Deletes a user (admin only).

- **URL**: `/api/users/{user_id}`
- **Method**: `DELETE`
- **Auth Required**: Yes (admin)
- **Success Response**: `204 No Content`

## Role Management

### List Roles

Returns all roles.

- **URL**: `/api/roles`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "roles": [
      {
        "id": "string",
        "name": "string",
        "permissions": {}
      }
    ]
  }
  ```

### Get Role

Returns details for a specific role.

- **URL**: `/api/roles/{role_id}`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "id": "string",
    "name": "string",
    "permissions": {}
  }
  ```

### Create Role

Creates a new role (admin only).

- **URL**: `/api/roles`
- **Method**: `POST`
- **Auth Required**: Yes (admin)
- **Request Body**:
  ```json
  {
    "name": "string"
  }
  ```
- **Success Response**: `201 Created`
  ```json
  {
    "id": "string",
    "name": "string"
  }
  ```

### Update Role

Updates role information (admin only).

- **URL**: `/api/roles/{role_id}`
- **Method**: `PUT`
- **Auth Required**: Yes (admin)
- **Request Body**:
  ```json
  {
    "name": "string",
    "permissions": {}
  }
  ```
- **Success Response**: `200 OK`

### Delete Role

Deletes a role (admin only).

- **URL**: `/api/roles/{role_id}`
- **Method**: `DELETE`
- **Auth Required**: Yes (admin)
- **Success Response**: `204 No Content`

## RBAC API

The RBAC system includes additional endpoints registered through the RBAC API module. See the [RBAC API documentation](RBAC_API.md) for complete details on:

- `/api/rbac/roles` - Enhanced role management
- `/api/rbac/users` - Enhanced user management  
- `/api/rbac/permissions` - Permission management
- `/api/rbac/roles/{id}/permissions` - Role permission assignment
- `/api/rbac/roles/{id}/users` - Role user assignment

## Metrics

### Get Current Metrics

Returns current system metrics.

- **URL**: `/api/metrics`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "operations": {
      "total": 123,
      "database": 45,
      "read": 78,
      "write": 45
    },
    "performance": {
      "avg_response_time_ms": 12.5,
      "min_response_time_ms": 0.8,
      "max_response_time_ms": 245.3,
      "active_connections": 5
    },
    "cache": {
      "hit_rate": 0.85,
      "hits": 1024,
      "misses": 180,
      "evictions": 25,
      "size_bytes": 2048576
    },
    "memory": {
      "total_kb": 8388608,
      "free_kb": 4194304,
      "used_kb": 4194304,
      "process_kb": 65536
    },
    "connections": {
      "active": 5,
      "total": 150
    }
  }
  ```

### Get Metrics History

Returns historical metrics data.

- **URL**: `/api/metrics/history`
- **Method**: `GET`
- **Auth Required**: Yes
- **Query Parameters**:
  - `type` - Metric type (operations, performance, cache, memory, connections)
  - `limit` - Number of data points (default: 15)
- **Success Response**: `200 OK`
  ```json
  {
    "type": "operations",
    "data": [
      {
        "timestamp": "2025-01-30T10:00:00Z",
        "total": 100,
        "database": 40,
        "read": 60,
        "write": 40
      }
    ]
  }
  ```

### Get Aggregated Metrics

Returns aggregated metrics over a time period.

- **URL**: `/api/metrics/aggregate`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`

### Export Metrics

Exports metrics data.

- **URL**: `/api/metrics/export`
- **Method**: `POST`
- **Auth Required**: Yes
- **Success Response**: `200 OK`

## System Information

### Get System Info

Returns system information and statistics.

- **URL**: `/api/system/info`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "version": "2.0.6",
    "uptime": 3600,
    "memory_usage": {
      "total": 8388608,
      "used": 4194304,
      "free": 4194304
    },
    "database_stats": {
      "collections": 10,
      "documents": 1500,
      "size_bytes": 10485760
    }
  }
  ```

## Health Check

### Basic Health Check

Simple endpoint to verify server is running.

- **URL**: `/health`
- **Method**: `GET`
- **Auth Required**: No
- **Success Response**: `200 OK`
  ```json
  {
    "status": "healthy",
    "timestamp": 1706478000
  }
  ```

### Available Metrics

Lists available metric types.

- **URL**: `/metrics/available`
- **Method**: `GET`
- **Auth Required**: No
- **Success Response**: `200 OK`

## Configuration

### Get Configuration

Returns current server configuration.

- **URL**: `/api/config`
- **Method**: `GET`
- **Auth Required**: Yes (admin)
- **Success Response**: `200 OK`
  ```json
  {
    "port": 5000,
    "ssl_enabled": false,
    "db_path": "/opt/jsondb/build/var/database.jdb",
    "cache_enabled": true,
    "cache_size": 1000,
    "log_level": "INFO"
  }
  ```

### Update Configuration

Updates server configuration.

- **URL**: `/api/config`
- **Method**: `PUT`
- **Auth Required**: Yes (admin)
- **Request Body**: Configuration object
- **Success Response**: `200 OK`

## Schema Validation

### List Schemas

Returns all registered schemas.

- **URL**: `/api/schemas`
- **Method**: `GET`
- **Auth Required**: No
- **Success Response**: `200 OK`
  ```json
  {
    "schemas": [
      {
        "collection": "users",
        "name": "user_schema",
        "description": "User document validation"
      }
    ]
  }
  ```

### Create Schema

Creates a new validation schema.

- **URL**: `/api/schemas`
- **Method**: `POST`
- **Auth Required**: No
- **Request Body**:
  ```json
  {
    "collection": "string",
    "name": "string",
    "description": "string",
    "rules": [
      {
        "type": "string",
        "field_path": "string",
        "params": {}
      }
    ]
  }
  ```
- **Success Response**: `201 Created`

### Get Schema

Returns a specific schema.

- **URL**: `/api/schemas/{collection}`
- **Method**: `GET`
- **Auth Required**: No
- **Success Response**: `200 OK`

### Update Schema

Updates an existing schema.

- **URL**: `/api/schemas/{collection}`
- **Method**: `PUT`
- **Auth Required**: No
- **Success Response**: `200 OK`

### Delete Schema

Deletes a schema.

- **URL**: `/api/schemas/{collection}`
- **Method**: `DELETE`
- **Auth Required**: No
- **Success Response**: `204 No Content`

### Validate Document

Validates a document against its schema.

- **URL**: `/api/validate`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "collection": "string",
    "document": {}
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "is_valid": true
  }
  ```
- **Error Response**: `200 OK` (with validation errors)
  ```json
  {
    "is_valid": false,
    "error_field": "email",
    "error_message": "Invalid email format"
  }
  ```

## Import/Export

### Export Data

Exports database data.

- **URL**: `/api/export`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "collections": ["collection1", "collection2"],
    "format": "json"
  }
  ```
- **Success Response**: `200 OK`
  Returns exported data as JSON

### Import Data

Imports data into the database.

- **URL**: `/api/import`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**: JSON data to import
- **Success Response**: `200 OK`
  ```json
  {
    "imported": 150,
    "errors": 0
  }
  ```

## Indexes

### List Indexes

Returns all indexes for a collection.

- **URL**: `/api/indexes/{collection}`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "indexes": [
      {
        "name": "email_index",
        "field": "email",
        "type": "unique",
        "entries": 150
      }
    ],
    "count": 1
  }
  ```

### Create Index

Creates a new index on a field.

- **URL**: `/api/indexes/{collection}`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "name": "string",
    "field": "string",
    "type": "string"
  }
  ```
- **Success Response**: `201 Created`
  ```json
  {
    "success": true,
    "message": "Index created successfully",
    "collection": "users",
    "name": "email_index",
    "field": "email",
    "type": "unique"
  }
  ```

### Get Index

Returns details about a specific index.

- **URL**: `/api/indexes/{collection}/{index_name}`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "name": "email_index",
    "field": "email",
    "type": "unique",
    "entries": 150,
    "buckets": 100,
    "collection": "users",
    "sample_entries": [
      {
        "document_id": "doc-123",
        "value": "user@example.com"
      }
    ]
  }
  ```

### Delete Index

Deletes an index.

- **URL**: `/api/indexes/{collection}/{index_name}`
- **Method**: `DELETE`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Index dropped successfully",
    "collection": "users",
    "name": "email_index"
  }
  ```

### Rebuild Index

Rebuilds an index or all indexes in a collection.

- **URL**: `/api/indexes/rebuild/{collection}/{index_name}`
- **Method**: `POST`
- **Auth Required**: Yes
- **Note**: If `index_name` is omitted, all indexes in the collection are rebuilt
- **Success Response**: `200 OK`

### Get Index Statistics

Returns detailed statistics for an index.

- **URL**: `/api/indexes/stats/{collection}/{index_name}`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "name": "email_index",
    "field": "email",
    "type": "unique",
    "entries": 150,
    "buckets": 100,
    "empty_buckets": 20,
    "min_bucket_size": 0,
    "max_bucket_size": 5,
    "avg_bucket_size": 1.5,
    "collection": "users"
  }
  ```

### Query Using Index

Queries documents using an index.

- **URL**: `/api/indexes/query/{collection}`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "field": "email",
    "value": "user@example.com",
    "limit": 10,
    "skip": 0
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "documents": [
      {
        "id": "doc-123",
        "_id": "doc-123",
        "email": "user@example.com",
        "...": "other fields"
      }
    ],
    "count": 1
  }
  ```

### Compound Query Using Indexes

Performs compound queries using multiple indexes.

- **URL**: `/api/indexes/compound/{collection}`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "operation": "AND",
    "queries": [
      {
        "field": "role",
        "value": "admin"
      },
      {
        "field": "status",
        "value": "active"
      }
    ],
    "limit": 10,
    "skip": 0
  }
  ```
- **Success Response**: `200 OK`

## JavaScript Extensions

### Execute JavaScript Query

Executes a JavaScript query on a collection.

- **URL**: `/api/js/query`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "collection": "users",
    "code": "db.filter(doc => doc.age > 30)"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "result": [
      {
        "_id": "doc-123",
        "...": "document fields"
      }
    ]
  }
  ```

### Evaluate JavaScript Code

Evaluates arbitrary JavaScript code.

- **URL**: `/api/js/eval`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "code": "2 + 2"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "result": 4
  }
  ```

### Register JavaScript Function

Registers a reusable JavaScript function.

- **URL**: `/api/js/functions`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "name": "calculateAge",
    "code": "function(birthYear) { return new Date().getFullYear() - birthYear; }"
  }
  ```
- **Success Response**: `201 Created`

### Execute JavaScript Function

Executes a registered JavaScript function.

- **URL**: `/api/js/functions/{function_name}`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "params": [1990]
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "result": 35
  }
  ```

### Register Document Validator

Registers a JavaScript validator for a collection.

- **URL**: `/api/js/validators`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "collection": "users",
    "code": "function(doc) { return doc.email && doc.email.includes('@'); }"
  }
  ```
- **Success Response**: `201 Created`

### Register Document Transformer

Registers a JavaScript transformer for a collection.

- **URL**: `/api/js/transformers`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "collection": "users",
    "code": "function(doc) { doc.fullName = doc.firstName + ' ' + doc.lastName; return doc; }"
  }
  ```
- **Success Response**: `201 Created`

## Transactions

### Begin Transaction

Starts a new transaction.

- **URL**: `/api/transactions`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "isolation_level": "read_committed"
  }
  ```
  Valid isolation levels: `read_uncommitted`, `read_committed`, `serializable`
- **Success Response**: `201 Created`
  ```json
  {
    "id": "txn-123",
    "state": "active",
    "start_time": 1706478000,
    "isolation_level": "read_committed",
    "user_id": "user-123"
  }
  ```

### Commit Transaction

Commits a transaction.

- **URL**: `/api/transactions/{transaction_id}/commit`
- **Method**: `POST`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "transaction_id": "txn-123",
    "status": "committed",
    "commit_time": 1706478100
  }
  ```

### Rollback Transaction

Rolls back a transaction.

- **URL**: `/api/transactions/{transaction_id}/rollback`
- **Method**: `DELETE`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "transaction_id": "txn-123",
    "status": "aborted",
    "abort_time": 1706478100
  }
  ```

### Transaction Operations

Transactions support all document operations (query, create, update, delete) within the transaction context:

- **Query**: `GET /api/transactions/{txn_id}/collections/{collection}/documents`
- **Create**: `POST /api/transactions/{txn_id}/collections/{collection}/documents`
- **Update**: `PUT /api/transactions/{txn_id}/collections/{collection}/documents/{doc_id}`
- **Delete**: `DELETE /api/transactions/{txn_id}/collections/{collection}/documents/{doc_id}`

### Transaction Metrics

Returns transaction performance metrics.

- **URL**: `/api/transactions/metrics`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`

### Transaction Status

Returns the status of a specific transaction.

- **URL**: `/api/transactions/{transaction_id}`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`

### Transaction Logs

Various endpoints for transaction logging and audit trails:

- **Get Logs**: `GET /api/transactions/logs`
- **Configure Logging**: `POST /api/transactions/logs/configure`
- **Archive Logs**: `POST /api/transactions/logs/archive`
- **Generate Report**: `GET /api/transactions/logs/report`
- **Document History**: `GET /api/transactions/logs/document-history`

## Cache Management

### Get Cache Statistics

Returns cache performance statistics.

- **URL**: `/api/cache/stats`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "enabled": true,
    "size": 120,
    "capacity": 1000,
    "byte_size": 35840,
    "max_memory_mb": 100,
    "type": "lru",
    "ttl": 3600,
    "hits": 450,
    "misses": 120,
    "hit_ratio": 0.789,
    "evictions": 25
  }
  ```

### Configure Cache

Updates cache configuration.

- **URL**: `/api/cache/configure`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "enabled": true,
    "capacity": 1000,
    "ttl": 3600,
    "type": "lru",
    "max_memory_mb": 100
  }
  ```
- **Success Response**: `200 OK`

### Clear Cache

Clears all cached entries.

- **URL**: `/api/cache/clear`
- **Method**: `POST`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Cache cleared successfully"
  }
  ```

### Invalidate Cache

Invalidates specific cache entries.

- **URL**: `/api/cache/invalidate`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "collection": "users",
    "document_id": "doc-123"
  }
  ```
- **Success Response**: `200 OK`

## OpenAPI Specification

### Get OpenAPI Spec

Returns the OpenAPI 3.0 specification for the API.

- **URL**: `/api/openapi.json`
- **Method**: `GET`
- **Auth Required**: No
- **Success Response**: `200 OK`
- **Content-Type**: `application/json`

## Error Responses

All API endpoints follow a consistent error response format:

```json
{
  "error": "Error message description"
}
```

Common HTTP status codes:
- `400 Bad Request` - The request was malformed or invalid
- `401 Unauthorized` - Authentication is required or token is invalid
- `403 Forbidden` - The authenticated user doesn't have sufficient permissions
- `404 Not Found` - The requested resource doesn't exist
- `405 Method Not Allowed` - The HTTP method is not supported for this endpoint
- `500 Internal Server Error` - Server error occurred

## Examples

### Authentication Flow

```bash
# Register a new user
curl -X POST http://localhost:5000/api/auth/register \
  -H "Content-Type: application/json" \
  -d '{"username": "john", "password": "secret123"}'

# Login to get token
curl -X POST http://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "john", "password": "secret123"}'

# Use the token for authenticated requests
curl -X GET http://localhost:5000/api/users \
  -H "Authorization: Bearer <your-jwt-token>"
```

### Working with Collections and Documents

```bash
# Create a collection
curl -X POST http://localhost:5000/api/collections \
  -H "Content-Type: application/json" \
  -d '{"name": "products"}'

# Insert a document
curl -X POST http://localhost:5000/api/collections/products/documents \
  -H "Content-Type: application/json" \
  -d '{"name": "Widget", "price": 19.99, "category": "tools"}'

# Query documents
curl -X GET 'http://localhost:5000/api/collections/products/documents?query={"category":"tools"}'

# Update a document
curl -X PUT http://localhost:5000/api/collections/products/documents/doc-123 \
  -H "Content-Type: application/json" \
  -d '{"name": "Super Widget", "price": 24.99, "category": "tools"}'

# Delete a document
curl -X DELETE http://localhost:5000/api/collections/products/documents/doc-123
```

### Using Indexes

```bash
# Create an index
curl -X POST http://localhost:5000/api/indexes/products \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"name": "price_index", "field": "price", "type": "btree"}'

# Query using index
curl -X POST http://localhost:5000/api/indexes/query/products \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"field": "price", "value": 19.99}'
```

### Transaction Example

```bash
# Begin transaction
curl -X POST http://localhost:5000/api/transactions \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"isolation_level": "read_committed"}'

# Perform operations within transaction
curl -X POST http://localhost:5000/api/transactions/txn-123/collections/orders/documents \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"customer": "john", "total": 99.99}'

# Commit transaction
curl -X POST http://localhost:5000/api/transactions/txn-123/commit \
  -H "Authorization: Bearer <token>"
```

## Notes

1. **Binary persistence** is automatic - the database is saved to disk periodically and on shutdown
2. **Metrics** are stored in the `_system_metrics` collection with automatic retention management
3. **Sessions** have sliding timeouts that extend on activity
4. **Backup/Restore endpoints** shown in some documentation are not yet implemented
5. Collection and document endpoints currently have authentication disabled for testing - this will be re-enabled in production