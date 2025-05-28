# JSON Database Server API Documentation

**Version**: 2.0.6  
**Last Updated**: January 28, 2025

This document provides accurate details on the REST API endpoints available in the JSON Database Server, verified against the actual codebase implementation.

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

**IMPORTANT**: Some collection endpoints currently have authentication disabled. This is marked in the endpoint descriptions.

## Authentication Endpoints

### Register User

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

### Login

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

### Refresh Token

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

### Logout

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

- **URL**: `/api/collections`
- **Method**: `GET`
- **Auth Required**: No (temporarily disabled)
- **Success Response**: `200 OK`
  ```json
  {
    "collections": ["string"]
  }
  ```

### Create Collection

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

### Drop Collection

- **URL**: `/api/collections/`
- **Method**: `DELETE`
- **Auth Required**: No (temporarily disabled)
- **URL Pattern**: `/api/collections/{collection_name}`
- **Success Response**: `204 No Content`

## Documents

**Note**: Document endpoints currently have authentication disabled for testing purposes.

### Query Documents

- **URL**: `/api/collections/`
- **Method**: `GET`
- **Auth Required**: No (temporarily disabled)
- **URL Pattern**: `/api/collections/{collection_name}/documents`
- **Query Parameters**: 
  - `query` - JSON query object
  - `page` - Page number (for offset pagination)
  - `page_size` - Items per page
  - `cursor` - Cursor for cursor-based pagination
  - `limit` - Limit for cursor-based pagination
- **Success Response**: `200 OK`
  ```json
  {
    "documents": [...],
    "count": 10,
    "total_count": 100,
    "pagination": {
      "page": 1,
      "page_size": 10,
      "total_pages": 10,
      "has_next_page": true,
      "has_prev_page": false,
      "next_cursor": "string",
      "prev_cursor": null
    }
  }
  ```

### Get Document

- **URL**: `/api/collections/`
- **Method**: `GET`
- **Auth Required**: No (temporarily disabled)
- **URL Pattern**: `/api/collections/{collection_name}/documents/{document_id}`
- **Success Response**: `200 OK`
  ```json
  {
    "_id": "string",
    ...
  }
  ```

### Create Document

- **URL**: `/api/collections/`
- **Method**: `POST`
- **Auth Required**: No (temporarily disabled)
- **URL Pattern**: `/api/collections/{collection_name}/documents`
- **Request Body**: JSON document
- **Success Response**: `201 Created`
  ```json
  {
    "_id": "string"
  }
  ```

### Update Document

- **URL**: `/api/collections/`
- **Method**: `PUT`
- **Auth Required**: No (temporarily disabled)
- **URL Pattern**: `/api/collections/{collection_name}/documents/{document_id}`
- **Request Body**: Complete JSON document
- **Success Response**: `200 OK`
  ```json
  {
    "_id": "string"
  }
  ```

### Delete Document

- **URL**: `/api/collections/`
- **Method**: `DELETE`
- **Auth Required**: No (temporarily disabled)
- **URL Pattern**: `/api/collections/{collection_name}/documents/{document_id}`
- **Success Response**: `204 No Content`

## User Management

### List Users

- **URL**: `/api/users`
- **Method**: `GET`
- **Auth Required**: Yes
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

- **URL**: `/api/users/`
- **Method**: `GET`
- **Auth Required**: Yes
- **URL Pattern**: `/api/users/{user_id}`
- **Success Response**: `200 OK`
  ```json
  {
    "id": "string",
    "username": "string",
    "roles": ["string"]
  }
  ```

### Create User

- **URL**: `/api/users`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "username": "string",
    "password": "string"
  }
  ```
- **Success Response**: `201 Created`

### Update User

- **URL**: `/api/users/`
- **Method**: `PUT`
- **Auth Required**: Yes
- **URL Pattern**: `/api/users/{user_id}`
- **Request Body**:
  ```json
  {
    "username": "string",
    "password": "string"
  }
  ```
- **Success Response**: `200 OK`

### Delete User

- **URL**: `/api/users/`
- **Method**: `DELETE`
- **Auth Required**: Yes
- **URL Pattern**: `/api/users/{user_id}`
- **Success Response**: `204 No Content`

## Role Management

### List Roles

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

- **URL**: `/api/roles/`
- **Method**: `GET`
- **Auth Required**: Yes
- **URL Pattern**: `/api/roles/{role_id}`
- **Success Response**: `200 OK`

### Create Role

- **URL**: `/api/roles`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "name": "string"
  }
  ```
- **Success Response**: `201 Created`

### Update Role

- **URL**: `/api/roles/`
- **Method**: `PUT`
- **Auth Required**: Yes
- **URL Pattern**: `/api/roles/{role_id}`
- **Success Response**: `200 OK`

### Delete Role

- **URL**: `/api/roles/`
- **Method**: `DELETE`
- **Auth Required**: Yes
- **URL Pattern**: `/api/roles/{role_id}`
- **Success Response**: `204 No Content`

## Metrics

### Get Metrics

- **URL**: `/api/metrics`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "operations": {...},
    "performance": {...},
    "cache": {...},
    "memory": {...},
    "connections": {...}
  }
  ```

### Get Metrics History

- **URL**: `/api/metrics/history`
- **Method**: `GET`
- **Auth Required**: Yes
- **Query Parameters**:
  - `type` - Metric type (operations, performance, cache, memory, connections)
  - `limit` - Number of data points
- **Success Response**: `200 OK`

### Get Aggregated Metrics

- **URL**: `/api/metrics/aggregate`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`

### Export Metrics

- **URL**: `/api/metrics/export`
- **Method**: `POST`
- **Auth Required**: Yes
- **Success Response**: `200 OK`

## System Information

### Get System Info

- **URL**: `/api/system/info`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "version": "2.0.6",
    "uptime": 3600,
    "memory_usage": {...},
    "database_stats": {...}
  }
  ```

## Health Check

### Health Check

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

- **URL**: `/metrics/available`
- **Method**: `GET`
- **Auth Required**: No
- **Success Response**: `200 OK`

## Configuration

### Get Configuration

- **URL**: `/api/config`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`

### Update Configuration

- **URL**: `/api/config`
- **Method**: `PUT`
- **Auth Required**: Yes
- **Request Body**: Configuration object
- **Success Response**: `200 OK`

## Schemas

### List Schemas

- **URL**: `/api/schemas`
- **Method**: `GET`
- **Auth Required**: No
- **Success Response**: `200 OK`

### Create Schema

- **URL**: `/api/schemas`
- **Method**: `POST`
- **Auth Required**: No
- **Request Body**: Schema definition
- **Success Response**: `201 Created`

### Get Schema

- **URL**: `/api/schemas/`
- **Method**: `GET`
- **Auth Required**: No
- **URL Pattern**: `/api/schemas/{schema_id}`
- **Success Response**: `200 OK`

### Update Schema

- **URL**: `/api/schemas/`
- **Method**: `PUT`
- **Auth Required**: No
- **URL Pattern**: `/api/schemas/{schema_id}`
- **Success Response**: `200 OK`

### Delete Schema

- **URL**: `/api/schemas/`
- **Method**: `DELETE`
- **Auth Required**: No
- **URL Pattern**: `/api/schemas/{schema_id}`
- **Success Response**: `204 No Content`

### Validate Document

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

## Import/Export

### Export Data

- **URL**: `/api/export`
- **Method**: `POST`
- **Auth Required**: Yes
- **Success Response**: `200 OK`

### Import Data

- **URL**: `/api/import`
- **Method**: `POST`
- **Auth Required**: Yes
- **Success Response**: `200 OK`

## Indexes

Full index API documentation available - includes create, list, get, delete, rebuild, stats, and query operations.

## JavaScript Extensions

Full JavaScript API documentation available - includes query execution, code evaluation, function registration, validators, and transformers.

## Transactions

Full transaction API documentation available - includes begin, commit, rollback, isolation levels, savepoints, and transaction-scoped operations.

## Cache Management

Full cache API documentation available - includes stats, configuration, clearing, and invalidation.

## OpenAPI Specification

### Get OpenAPI Spec

- **URL**: `/api/openapi.json`
- **Method**: `GET`
- **Auth Required**: No
- **Success Response**: `200 OK`
- **Content-Type**: `application/json`

## Notes

1. **Backup/Restore endpoints are NOT implemented** - Documentation showing these endpoints is incorrect
2. **Binary persistence** is automatic and requires no API interaction
3. **Metrics are stored in time-series format** with automatic retention management
4. Some endpoints use path prefix matching - actual paths may include additional segments

## Error Response Format

All errors follow this format:

```json
{
  "error": "Error message description"
}
```

Common HTTP status codes:
- `400` - Bad Request
- `401` - Unauthorized
- `403` - Forbidden
- `404` - Not Found
- `500` - Internal Server Error