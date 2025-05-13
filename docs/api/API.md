# JSON Database Server API Documentation

This document provides details on the REST API endpoints available in the JSON Database Server.

## Base URL

All API endpoints are relative to the base URL of the server:

```
http://localhost:8080
```

## Authentication

The API uses JWT (JSON Web Token) for authentication. Most endpoints require a valid token to be included in the `Authorization` header.

### Authentication Endpoints

#### Register a new user

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

#### Login

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

## Collections

Collections are containers for documents.

### Collection Endpoints

#### List Collections

- **URL**: `/api/collections`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "collections": ["string"]
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `500 Internal Server Error`: Server error

#### Create Collection

- **URL**: `/api/collections`
- **Method**: `POST`
- **Auth Required**: Yes
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
  - `400 Bad Request`: Invalid request or collection name
  - `401 Unauthorized`: Missing or invalid token
  - `500 Internal Server Error`: Server error

#### Drop Collection

- **URL**: `/api/collections/:name`
- **Method**: `DELETE`
- **Auth Required**: Yes
- **URL Parameters**: `name=[string]` - Name of the collection to drop
- **Success Response**: `204 No Content`
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection not found
  - `500 Internal Server Error`: Server error

## Documents

Documents are JSON objects stored in collections.

### Document Endpoints

#### Query Documents

- **URL**: `/api/collections/:name/documents`
- **Method**: `GET`
- **Auth Required**: Yes
- **URL Parameters**: `name=[string]` - Name of the collection
- **Query Parameters**: `query=[json]` - Optional JSON query filter using the Query Language syntax
- **Success Response**: `200 OK`
  ```json
  {
    "documents": [
      {
        "_id": "string",
        ...
      }
    ],
    "count": 10,
    "total_count": 45,
    "pagination": {
      "page": 2,
      "page_size": 10,
      "total_pages": 5,
      "total_count": 45,
      "page_count": 10,
      "has_next_page": true,
      "has_prev_page": true,
      "next_cursor": "string",
      "prev_cursor": "string"
    }
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid query
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection not found
  - `500 Internal Server Error`: Server error

The query parameter supports a comprehensive query language for filtering documents. You can use comparison operators, logical operators, array operators, and more. For detailed documentation on the query language syntax, see [Query Language](/docs/guides/QUERY_LANGUAGE.md).

### Pagination

The query system supports two types of pagination:

1. **Offset-based pagination** (page-based): Use `page` and `page_size` parameters
   ```json
   {
     "query": { "status": "active" },
     "page": 2,
     "page_size": 10
   }
   ```

2. **Cursor-based pagination**: Use `cursor` and `limit` parameters
   ```json
   {
     "query": { "status": "active" },
     "cursor": "eyJsYXN0X2lkIjoiMTIzNDU2Nzg5MCJ9",
     "limit": 10
   }
   ```

The response includes pagination information:
- `count`: Number of documents in the current result set
- `total_count`: Total number of documents matching the query (before pagination)
- `pagination`: Detailed pagination information:
  - `page`: Current page number (offset-based pagination)
  - `page_size`: Number of documents per page
  - `total_pages`: Total number of pages
  - `has_next_page`: Whether there are more pages/results
  - `has_prev_page`: Whether there are previous pages/results
  - `next_cursor`: Cursor for the next page (cursor-based pagination)
  - `prev_cursor`: Cursor for the previous page (cursor-based pagination)

For cursor-based pagination, use the `next_cursor` value from the response to fetch the next page of results.

#### Get Document by ID

- **URL**: `/api/collections/:name/documents/:id`
- **Method**: `GET`
- **Auth Required**: Yes
- **URL Parameters**:
  - `name=[string]` - Name of the collection
  - `id=[string]` - ID of the document
- **Success Response**: `200 OK`
  ```json
  {
    "_id": "string",
    ...
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Document or collection not found
  - `500 Internal Server Error`: Server error

#### Create Document

- **URL**: `/api/collections/:name/documents`
- **Method**: `POST`
- **Auth Required**: Yes
- **URL Parameters**: `name=[string]` - Name of the collection
- **Request Body**: A JSON object
- **Success Response**: `201 Created`
  ```json
  {
    "_id": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid document
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection not found
  - `500 Internal Server Error`: Server error

#### Update Document

- **URL**: `/api/collections/:name/documents/:id`
- **Method**: `PUT`
- **Auth Required**: Yes
- **URL Parameters**:
  - `name=[string]` - Name of the collection
  - `id=[string]` - ID of the document
- **Request Body**: A JSON object (entire document)
- **Success Response**: `200 OK`
  ```json
  {
    "_id": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid document
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Document or collection not found
  - `500 Internal Server Error`: Server error

#### Delete Document

- **URL**: `/api/collections/:name/documents/:id`
- **Method**: `DELETE`
- **Auth Required**: Yes
- **URL Parameters**:
  - `name=[string]` - Name of the collection
  - `id=[string]` - ID of the document
- **Success Response**: `204 No Content`
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Document or collection not found
  - `500 Internal Server Error`: Server error

## User Management

These endpoints manage users in the system.

### User Endpoints

#### List Users

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
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `403 Forbidden`: Insufficient permissions
  - `500 Internal Server Error`: Server error

#### Get User

- **URL**: `/api/users/:id`
- **Method**: `GET`
- **Auth Required**: Yes (admin or owner)
- **URL Parameters**: `id=[string]` - User ID
- **Success Response**: `200 OK`
  ```json
  {
    "id": "string",
    "username": "string",
    "roles": ["string"]
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `403 Forbidden`: Insufficient permissions
  - `404 Not Found`: User not found
  - `500 Internal Server Error`: Server error

#### Create User

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
- **Error Responses**:
  - `400 Bad Request`: Invalid request or username already exists
  - `401 Unauthorized`: Missing or invalid token
  - `403 Forbidden`: Insufficient permissions
  - `500 Internal Server Error`: Server error

#### Update User

- **URL**: `/api/users/:id`
- **Method**: `PUT`
- **Auth Required**: Yes (admin or owner)
- **URL Parameters**: `id=[string]` - User ID
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
- **Error Responses**:
  - `400 Bad Request`: Invalid request
  - `401 Unauthorized`: Missing or invalid token
  - `403 Forbidden`: Insufficient permissions
  - `404 Not Found`: User not found
  - `500 Internal Server Error`: Server error

#### Delete User

- **URL**: `/api/users/:id`
- **Method**: `DELETE`
- **Auth Required**: Yes (admin)
- **URL Parameters**: `id=[string]` - User ID
- **Success Response**: `204 No Content`
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `403 Forbidden`: Insufficient permissions
  - `404 Not Found`: User not found
  - `500 Internal Server Error`: Server error

## Role Management

These endpoints manage roles and permissions.

### Role Endpoints

#### List Roles

- **URL**: `/api/roles`
- **Method**: `GET`
- **Auth Required**: Yes (admin)
- **Success Response**: `200 OK`
  ```json
  {
    "roles": [
      {
        "id": "string",
        "name": "string"
      }
    ]
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `403 Forbidden`: Insufficient permissions
  - `500 Internal Server Error`: Server error

#### Get Role

- **URL**: `/api/roles/:id`
- **Method**: `GET`
- **Auth Required**: Yes (admin)
- **URL Parameters**: `id=[string]` - Role ID
- **Success Response**: `200 OK`
  ```json
  {
    "id": "string",
    "name": "string",
    "permissions": {}
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `403 Forbidden`: Insufficient permissions
  - `404 Not Found`: Role not found
  - `500 Internal Server Error`: Server error

#### Create Role

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
- **Error Responses**:
  - `400 Bad Request`: Invalid request or role already exists
  - `401 Unauthorized`: Missing or invalid token
  - `403 Forbidden`: Insufficient permissions
  - `500 Internal Server Error`: Server error

#### Update Role

- **URL**: `/api/roles/:id`
- **Method**: `PUT`
- **Auth Required**: Yes (admin)
- **URL Parameters**: `id=[string]` - Role ID
- **Request Body**:
  ```json
  {
    "name": "string",
    "permissions": {}
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "id": "string",
    "name": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid request
  - `401 Unauthorized`: Missing or invalid token
  - `403 Forbidden`: Insufficient permissions
  - `404 Not Found`: Role not found
  - `500 Internal Server Error`: Server error

#### Delete Role

- **URL**: `/api/roles/:id`
- **Method**: `DELETE`
- **Auth Required**: Yes (admin)
- **URL Parameters**: `id=[string]` - Role ID
- **Success Response**: `204 No Content`
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `403 Forbidden`: Insufficient permissions
  - `404 Not Found`: Role not found
  - `500 Internal Server Error`: Server error

## Configuration

These endpoints manage server configuration.

### Configuration Endpoints

#### Get Configuration

- **URL**: `/api/config`
- **Method**: `GET`
- **Auth Required**: Yes (admin)
- **Success Response**: `200 OK`
  ```json
  {
    "port": "number",
    "ssl_enabled": "boolean",
    "db_path": "string"
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `403 Forbidden`: Insufficient permissions
  - `500 Internal Server Error`: Server error

#### Update Configuration

- **URL**: `/api/config`
- **Method**: `PUT`
- **Auth Required**: Yes (admin)
- **Request Body**:
  ```json
  {
    "port": "number",
    "ssl_enabled": "boolean",
    "db_path": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "port": "number",
    "ssl_enabled": "boolean",
    "db_path": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid configuration
  - `401 Unauthorized`: Missing or invalid token
  - `403 Forbidden`: Insufficient permissions
  - `500 Internal Server Error`: Server error

## Schema Validation

These endpoints manage JSON schema validation for collections.

### Schema Endpoints

#### Get Collection Schema

- **URL**: `/api/schemas/:collection`
- **Method**: `GET`
- **Auth Required**: Yes
- **URL Parameters**: `collection=[string]` - Collection name
- **Success Response**: `200 OK`
  ```json
  {
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
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection or schema not found
  - `500 Internal Server Error`: Server error

#### Create Schema

- **URL**: `/api/schemas`
- **Method**: `POST`
- **Auth Required**: Yes
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
  ```json
  {
    "success": true,
    "message": "Schema created successfully"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid schema definition
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection not found
  - `500 Internal Server Error`: Server error

#### Update Schema

- **URL**: `/api/schemas/:collection`
- **Method**: `PUT`
- **Auth Required**: Yes
- **URL Parameters**: `collection=[string]` - Collection name
- **Request Body**:
  ```json
  {
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
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Schema updated successfully"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid schema definition
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection or schema not found
  - `500 Internal Server Error`: Server error

#### Delete Schema

- **URL**: `/api/schemas/:collection`
- **Method**: `DELETE`
- **Auth Required**: Yes
- **URL Parameters**: `collection=[string]` - Collection name
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Schema deleted successfully"
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection or schema not found
  - `500 Internal Server Error`: Server error

#### Validate Document

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
    "error_field": "string",
    "error_message": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid request
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection or schema not found
  - `500 Internal Server Error`: Server error

## Backup and Restore

These endpoints manage database backup and restore operations.

### Backup Endpoints

#### Create Backup

- **URL**: `/api/backup`
- **Method**: `POST`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Backup created successfully",
    "filename": "string",
    "path": "string"
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `500 Internal Server Error`: Server error

#### List Backups

- **URL**: `/api/backup`
- **Method**: `GET`
- **Auth Required**: Yes
- **Success Response**: `200 OK`
  ```json
  {
    "backups": [
      {
        "filename": "string",
        "size": "number",
        "created": "number",
        "timestamp": "string"
      }
    ],
    "count": "number"
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `500 Internal Server Error`: Server error

#### Restore from Backup

- **URL**: `/api/backup/restore`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "filename": "string",
    "overwrite": "boolean"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Database restored successfully",
    "filename": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid request
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Backup file not found
  - `500 Internal Server Error`: Server error

#### Delete Backup

- **URL**: `/api/backup/:filename`
- **Method**: `DELETE`
- **Auth Required**: Yes
- **URL Parameters**: `filename=[string]` - Name of the backup file
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Backup deleted successfully",
    "filename": "string"
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Backup file not found
  - `500 Internal Server Error`: Server error

## Indexing

These endpoints manage database indexes for optimizing query performance.

### Index Endpoints

#### List Indexes

- **URL**: `/api/indexes/:collection`
- **Method**: `GET`
- **Auth Required**: Yes
- **URL Parameters**: `collection=[string]` - Collection name
- **Success Response**: `200 OK`
  ```json
  {
    "indexes": [
      {
        "name": "string",
        "field": "string",
        "type": "string",
        "entries": "number"
      }
    ],
    "count": "number"
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection not found
  - `500 Internal Server Error`: Server error

#### Create Index

- **URL**: `/api/indexes/:collection`
- **Method**: `POST`
- **Auth Required**: Yes
- **URL Parameters**: `collection=[string]` - Collection name
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
    "collection": "string",
    "name": "string",
    "field": "string",
    "type": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid index definition
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection not found
  - `500 Internal Server Error`: Server error

#### Get Index

- **URL**: `/api/indexes/:collection/:name`
- **Method**: `GET`
- **Auth Required**: Yes
- **URL Parameters**: 
  - `collection=[string]` - Collection name
  - `name=[string]` - Index name
- **Success Response**: `200 OK`
  ```json
  {
    "name": "string",
    "field": "string",
    "type": "string",
    "entries": "number",
    "buckets": "number",
    "collection": "string",
    "sample_entries": [
      {
        "document_id": "string",
        "value": "string"
      }
    ]
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection or index not found
  - `500 Internal Server Error`: Server error

#### Delete Index

- **URL**: `/api/indexes/:collection/:name`
- **Method**: `DELETE`
- **Auth Required**: Yes
- **URL Parameters**: 
  - `collection=[string]` - Collection name
  - `name=[string]` - Index name
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Index dropped successfully",
    "collection": "string",
    "name": "string"
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection or index not found
  - `500 Internal Server Error`: Server error

#### Rebuild Index

- **URL**: `/api/indexes/rebuild/:collection/:name`
- **Method**: `POST`
- **Auth Required**: Yes
- **URL Parameters**: 
  - `collection=[string]` - Collection name
  - `name=[string]` - Index name (optional - if not provided, all indexes in the collection will be rebuilt)
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Index rebuilt successfully",
    "collection": "string",
    "name": "string"
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection or index not found
  - `500 Internal Server Error`: Server error

#### Get Index Statistics

- **URL**: `/api/indexes/stats/:collection/:name`
- **Method**: `GET`
- **Auth Required**: Yes
- **URL Parameters**: 
  - `collection=[string]` - Collection name
  - `name=[string]` - Index name
- **Success Response**: `200 OK`
  ```json
  {
    "name": "string",
    "field": "string",
    "type": "string",
    "entries": "number",
    "buckets": "number",
    "empty_buckets": "number",
    "min_bucket_size": "number",
    "max_bucket_size": "number",
    "avg_bucket_size": "number",
    "collection": "string"
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection or index not found
  - `500 Internal Server Error`: Server error

#### Query Using Index

- **URL**: `/api/indexes/query/:collection`
- **Method**: `POST`
- **Auth Required**: Yes
- **URL Parameters**: `collection=[string]` - Collection name
- **Request Body**:
  ```json
  {
    "field": "string",
    "value": "any",
    "limit": "number",
    "skip": "number"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "documents": [
      {
        "id": "string",
        ...
      }
    ],
    "count": "number"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid query
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection or index not found
  - `500 Internal Server Error`: Server error

#### Compound Query Using Indexes

- **URL**: `/api/indexes/compound/:collection`
- **Method**: `POST`
- **Auth Required**: Yes
- **URL Parameters**: `collection=[string]` - Collection name
- **Request Body**:
  ```json
  {
    "operation": "string",
    "queries": [
      {
        "field": "string",
        "value": "any"
      }
    ],
    "limit": "number",
    "skip": "number"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "documents": [
      {
        "id": "string",
        ...
      }
    ],
    "count": "number"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid query
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection not found
  - `500 Internal Server Error`: Server error

### JavaScript Query Processing

These endpoints allow you to use JavaScript for custom query processing and extensions.

### JavaScript Endpoints

#### Execute JavaScript Query
- **URL**: `/api/js/query`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "collection": "string",
    "code": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "result": [
      {
        "_id": "string",
        ...
      }
    ]
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid query code
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection not found
  - `500 Internal Server Error`: Server error

#### Evaluate JavaScript Code
- **URL**: `/api/js/eval`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "code": "string"
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "result": "any"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid JavaScript code
  - `401 Unauthorized`: Missing or invalid token
  - `500 Internal Server Error`: Server error

#### Register JavaScript Function
- **URL**: `/api/js/functions`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "name": "string",
    "code": "string"
  }
  ```
- **Success Response**: `201 Created`
  ```json
  {
    "success": true,
    "name": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid function code
  - `401 Unauthorized`: Missing or invalid token
  - `500 Internal Server Error`: Server error

#### Execute JavaScript Function
- **URL**: `/api/js/functions/:name`
- **Method**: `POST`
- **Auth Required**: Yes
- **URL Parameters**:
  - `name=[string]` - Function name
- **Request Body**:
  ```json
  {
    "params": [
      "any"
    ]
  }
  ```
- **Success Response**: `200 OK`
  ```json
  {
    "result": "any"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid parameters
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Function not found
  - `500 Internal Server Error`: Server error

#### Register JavaScript Document Validator
- **URL**: `/api/js/validators`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "collection": "string",
    "code": "string"
  }
  ```
- **Success Response**: `201 Created`
  ```json
  {
    "success": true,
    "collection": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid validator code
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection not found
  - `500 Internal Server Error`: Server error

#### Register JavaScript Document Transformer
- **URL**: `/api/js/transformers`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "collection": "string",
    "code": "string"
  }
  ```
- **Success Response**: `201 Created`
  ```json
  {
    "success": true,
    "collection": "string"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid transformer code
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Collection not found
  - `500 Internal Server Error`: Server error

## Transactions

These endpoints allow you to manage transactions for atomic multi-document operations.

### Transaction Endpoints

#### Begin Transaction

- **URL**: `/api/transactions`
- **Method**: `POST`
- **Auth Required**: Yes
- **Request Body**:
  ```json
  {
    "isolation_level": "read_committed"
  }
  ```
  Valid isolation levels: `read_uncommitted`, `read_committed`, `serializable` (optional, defaults to `read_committed`)
- **Success Response**: `201 Created`
  ```json
  {
    "id": "string",
    "state": "active",
    "start_time": "number",
    "isolation_level": "read_committed",
    "user_id": "string"
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `500 Internal Server Error`: Server error

#### Commit Transaction

- **URL**: `/api/transactions/:id/commit`
- **Method**: `POST`
- **Auth Required**: Yes
- **URL Parameters**: `id=[string]` - Transaction ID
- **Success Response**: `200 OK`
  ```json
  {
    "transaction_id": "string",
    "status": "committed",
    "commit_time": "number"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid transaction ID
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Transaction not found
  - `500 Internal Server Error`: Server error

#### Rollback Transaction

- **URL**: `/api/transactions/:id/rollback`
- **Method**: `DELETE`
- **Auth Required**: Yes
- **URL Parameters**: `id=[string]` - Transaction ID
- **Success Response**: `200 OK`
  ```json
  {
    "transaction_id": "string",
    "status": "aborted",
    "abort_time": "number"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid transaction ID
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Transaction not found
  - `500 Internal Server Error`: Server error

#### Query Documents in Transaction

- **URL**: `/api/transactions/:id/collections/:collection/documents`
- **Method**: `GET`
- **Auth Required**: Yes
- **URL Parameters**:
  - `id=[string]` - Transaction ID
  - `collection=[string]` - Collection name
- **Query Parameters**: `query=[json]` - Optional JSON query filter using the Query Language syntax
- **Success Response**: `200 OK`
  ```json
  {
    "documents": [
      {
        "_id": "string",
        ...
      }
    ],
    "count": "number",
    "total_count": "number"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid query
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Transaction or collection not found
  - `500 Internal Server Error`: Server error

#### Insert Document in Transaction

- **URL**: `/api/transactions/:id/collections/:collection/documents`
- **Method**: `POST`
- **Auth Required**: Yes
- **URL Parameters**:
  - `id=[string]` - Transaction ID
  - `collection=[string]` - Collection name
- **Request Body**: A JSON object
- **Success Response**: `201 Created`
  ```json
  {
    "success": true,
    "message": "Document inserted successfully"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid document format
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Transaction or collection not found
  - `500 Internal Server Error`: Server error

#### Update Document in Transaction

- **URL**: `/api/transactions/:id/collections/:collection/documents/:document_id`
- **Method**: `PUT`
- **Auth Required**: Yes
- **URL Parameters**:
  - `id=[string]` - Transaction ID
  - `collection=[string]` - Collection name
  - `document_id=[string]` - Document ID
- **Request Body**: A JSON object
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Document updated successfully"
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid document format
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Transaction, collection, or document not found
  - `500 Internal Server Error`: Server error

#### Delete Document in Transaction

- **URL**: `/api/transactions/:id/collections/:collection/documents/:document_id`
- **Method**: `DELETE`
- **Auth Required**: Yes
- **URL Parameters**:
  - `id=[string]` - Transaction ID
  - `collection=[string]` - Collection name
  - `document_id=[string]` - Document ID
- **Success Response**: `200 OK`
  ```json
  {
    "success": true,
    "message": "Document deleted successfully"
  }
  ```
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `404 Not Found`: Transaction, collection, or document not found
  - `500 Internal Server Error`: Server error

## Cache Management

These endpoints allow you to manage the document cache system.

### Cache Endpoints

#### Get Cache Statistics
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
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `500 Internal Server Error`: Server error

#### Configure Cache
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
  ```json
  {
    "success": true,
    "message": "Cache configured successfully",
    "enabled": true,
    "size": 0,
    "capacity": 1000,
    "byte_size": 0,
    "max_memory_mb": 100,
    "type": "lru",
    "ttl": 3600,
    "hits": 0,
    "misses": 0,
    "hit_ratio": 0,
    "evictions": 0
  }
  ```
- **Error Responses**:
  - `400 Bad Request`: Invalid configuration
  - `401 Unauthorized`: Missing or invalid token
  - `500 Internal Server Error`: Server error

#### Clear Cache
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
- **Error Responses**:
  - `401 Unauthorized`: Missing or invalid token
  - `500 Internal Server Error`: Server error

## Query Language Examples

For detailed documentation on the query language syntax, see [Query Language](/docs/guides/QUERY_LANGUAGE.md).

### Basic Comparisons

```bash
# Find users with age greater than 30
curl -X GET "http://localhost:8080/api/collections/users/documents?query=%7B%22age%22%3A%7B%22%24gt%22%3A30%7D%7D" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN"
```

### Logical Operators

```bash
# Find active users or premium users
curl -X GET "http://localhost:8080/api/collections/users/documents?query=%7B%22%24or%22%3A%5B%7B%22status%22%3A%22active%22%7D%2C%7B%22type%22%3A%22premium%22%7D%5D%7D" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN"
```

### Array Operators

```bash
# Find documents that have both "database" and "json" tags
curl -X GET "http://localhost:8080/api/collections/articles/documents?query=%7B%22tags%22%3A%7B%22%24all%22%3A%5B%22database%22%2C%22json%22%5D%7D%7D" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN"
```

### Query with Projection

```bash
# Find users and return only name and email fields
curl -X GET "http://localhost:8080/api/collections/users/documents?query=%7B%22query%22%3A%7B%22status%22%3A%22active%22%7D%2C%22projection%22%3A%7B%22name%22%3A1%2C%22email%22%3A1%7D%7D" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN"
```

### Query with Sorting and Pagination

#### Page-based Pagination
```bash
# Find users sorted by lastname, with page-based pagination
curl -X GET "http://localhost:8080/api/collections/users/documents?query=%7B%22query%22%3A%7B%22status%22%3A%22active%22%7D%2C%22sort%22%3A%7B%22lastName%22%3A1%7D%2C%22page%22%3A2%2C%22page_size%22%3A10%7D" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN"
```

#### Cursor-based Pagination
```bash
# Find users sorted by lastname, with cursor-based pagination (first page)
curl -X GET "http://localhost:8080/api/collections/users/documents?query=%7B%22query%22%3A%7B%22status%22%3A%22active%22%7D%2C%22sort%22%3A%7B%22lastName%22%3A1%7D%2C%22limit%22%3A10%7D" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN"

# Find users sorted by lastname, with cursor-based pagination (next page using cursor)
curl -X GET "http://localhost:8080/api/collections/users/documents?query=%7B%22query%22%3A%7B%22status%22%3A%22active%22%7D%2C%22sort%22%3A%7B%22lastName%22%3A1%7D%2C%22cursor%22%3A%22NEXT_CURSOR_FROM_PREVIOUS_RESPONSE%22%2C%22limit%22%3A10%7D" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN"
```

#### Legacy Pagination (skip/limit)
```bash
# Find users sorted by lastname, with legacy pagination (skip/limit)
curl -X GET "http://localhost:8080/api/collections/users/documents?query=%7B%22query%22%3A%7B%22status%22%3A%22active%22%7D%2C%22sort%22%3A%7B%22lastName%22%3A1%7D%2C%22skip%22%3A10%2C%22limit%22%3A5%7D" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN"
```

## Index Examples

#### Creating an Index on a Field

To create an index on the `email` field of the `users` collection:

```bash
curl -X POST http://localhost:5000/api/indexes/users \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "email_index",
    "field": "email",
    "type": "unique"
  }'
```

#### Querying Using an Index

To find users with a specific email:

```bash
curl -X POST http://localhost:5000/api/indexes/query/users \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "field": "email",
    "value": "john.doe@example.com",
    "limit": 1
  }'
```

#### Compound Query Example

To find users with a specific role and status:

```bash
curl -X POST http://localhost:5000/api/indexes/compound/users \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
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
    "limit": 10
  }'
```

## Error Responses

All API endpoints may return these common error responses:

- `400 Bad Request`: The request was malformed or invalid
- `401 Unauthorized`: Authentication is required or token is invalid
- `403 Forbidden`: The authenticated user doesn't have sufficient permissions
- `404 Not Found`: The requested resource doesn't exist
- `405 Method Not Allowed`: The HTTP method is not supported for this endpoint
- `500 Internal Server Error`: Server error occurred

The error response body follows this format:

```json
{
  "error": "Error message"
}
```