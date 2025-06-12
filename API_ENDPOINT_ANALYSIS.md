# JSONdb API Endpoint Analysis Report

## Summary

The JSONdb server has a critical issue where almost all API endpoints hang indefinitely when they try to interact with the unified documents architecture. Only a few endpoints that don't require database access are working properly.

## Working Endpoints

These endpoints are functioning correctly:

1. **GET /api/health** - Returns health status (200 OK)
   - Simple health check without database interaction
   - Returns: `{"status": "ok", ...}`

2. **GET /api/openapi.json** - Returns OpenAPI specification (200 OK)
   - Static API documentation
   - Returns OpenAPI 3.0.0 spec

3. **GET /api/system/info** - Returns 401 Unauthorized (expected behavior)
   - Requires authentication but the endpoint itself responds correctly

## Non-Working Endpoints (Hanging)

All endpoints that interact with the database hang indefinitely:

### Authentication Endpoints
- POST /api/auth/login
- POST /api/auth/register
- POST /api/login
- POST /api/admin/login

### Session Management
- GET /api/sessions
- GET /api/sessions/active
- POST /api/auth/refresh
- POST /api/auth/logout

### Library Management
- GET /api/libraries
- POST /api/libraries
- GET /api/library-templates
- GET /api/libraries/:library

### Collection Management
- GET /api/collections
- POST /api/collections

### Document Operations
- GET /api/collections/:library/:collection/documents
- POST /api/collections/:library/:collection
- GET /api/collections/:library/:collection/:id
- PUT /api/collections/:library/:collection/:id
- DELETE /api/collections/:library/:collection/:id

### Unified Documents API
- GET /api/documents
- POST /api/documents
- GET /api/documents/:id
- PUT /api/documents/:id
- DELETE /api/documents/:id

### RBAC Management
- GET /api/users
- GET /api/roles
- POST /api/users
- POST /api/roles

### Metrics & Configuration
- GET /api/metrics
- GET /api/config
- GET /api/system/log-control

### Visualization & Import/Export
- GET /api/visualization/*
- POST /api/export
- POST /api/import

## Root Cause Analysis

### 1. Authentication Flow Issue
When attempting login at `/api/auth/login`, the server:
1. Receives and parses the request correctly
2. Extracts credentials successfully
3. Attempts to query for user in unified documents: `{"type":"user","username":"admin","library":"system"}`
4. Opens JDBX B-tree pages (root pages 34 and 35)
5. **HANGS** - Never returns from the query operation

### 2. JDBX Implementation Problem
The hanging occurs in the JDBX (JSONdb Binary format) storage backend:
- The `jdbx_find_documents` function appears to enter a deadlock or infinite loop
- Thread locks (pthread_rwlock) may be causing deadlock conditions
- The `find_collection` function tries to load the "documents" collection but may not be properly initialized

### 3. Server Crash Pattern
- After several hanging requests, the server becomes completely unresponsive
- Eventually crashes with no specific error message in logs
- Process terminates leaving a stale PID file

## Technical Details

### Request Flow for Authentication
```
1. HTTP Request → handle_client()
2. Route dispatch → api_handle_login()
3. Parse credentials → Successful
4. Query unified documents → db_query_documents()
5. JDBX backend → jdbx_find_documents()
6. Collection lookup → find_collection()
7. B-tree operations → HANG
```

### JDBX Storage Architecture
- Single-file database with B-tree indexes
- Uses pthread read/write locks for concurrency
- Collections stored with metadata pages
- Document storage with primary and secondary indexes

## Critical Finding

The storage backend configuration is **hardcoded** in the source code:
```c
/* JDBX is the only storage backend - ignore storage_backend argument */
if (storage_backend) {
    INIT_LOG_PROGRESS("CONFIG", "Ignoring storage backend argument - JDBX is the only supported backend");
}
```

This means:
- The JSONDB_STORAGE_BACKEND environment variable is ignored
- The --storage-backend command line argument is ignored
- JDBX is forced as the only storage backend
- The mmap backend has been removed or disabled

## Recommendations

1. **Immediate Fix**: The JDBX implementation needs debugging to identify the deadlock/hang condition
2. **Collection Initialization**: Ensure the "documents" collection is properly created before queries
3. **Lock Management**: Review pthread lock usage for potential deadlock scenarios
4. **Restore mmap Support**: Re-enable the mmap storage backend as a fallback option
5. **Storage Selection**: Implement proper storage backend selection based on configuration
6. **Testing**: Add unit tests for JDBX operations to identify the specific failure point

## Conclusion

The unified documents architecture is completely broken because:
1. JDBX is hardcoded as the only storage backend
2. JDBX implementation has critical bugs causing indefinite hangs
3. There is no fallback to the previous working mmap implementation
4. The server is effectively unusable for any database operations

The v3.2.0 release with "JDBX storage backend and unified documents architecture" has introduced a complete regression, making the JSONdb server non-functional for its core purpose.