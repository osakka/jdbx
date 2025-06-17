# API Gap Analysis: Current vs Gold Standard

## Executive Summary

This document identifies gaps between the current JDBX API implementation and our gold standard design.

## 1. Authentication & Session Management

### ✅ Implemented
- `POST /api/auth/login` - Basic login
- `POST /api/auth/logout` - Logout
- `POST /api/auth/refresh` - Token refresh
- `GET /api/sessions` - List sessions

### ❌ Missing
- `GET /api/auth/session` - Get current session info
- `DELETE /api/auth/sessions/:id` - Terminate specific session
- `POST /api/auth/library/:lib` - Switch library context
- `GET /api/auth/library` - Get current library context

### 🔧 Issues
- Multiple login endpoints (`/api/auth/login`, `/api/login`, `/api/admin/login`)
- Sessions stored as documents but not fully integrated with document API
- No library context switching mechanism

## 2. Document Operations

### ✅ Implemented
- Unified document API exists at `/api/documents`
- Library-scoped paths work: `/api/libraries/:lib/collections/:col/documents`
- Basic CRUD operations

### ❌ Missing
- `PATCH /api/documents/:uuid` - Partial updates
- Batch operations (`POST/DELETE /api/documents/batch`)
- Field projection (`?fields=name,uuid`)
- Standardized filtering (`?filter={"age":{"$gt":18}}`)

### 🔧 Issues
- Response format not standardized (no consistent envelope)
- Query parameters not standardized across endpoints

## 3. Virtual Collection Helpers

### ❌ Not Implemented
The convenient shortcuts like:
- `GET /api/users` → `/api/documents?type=user`
- `GET /api/roles` → `/api/documents?type=role`

Currently these exist as separate implementations, not as sugar over unified API.

## 4. RBAC Management

### 🔧 Major Issue: Duplicate Implementation
Currently have BOTH:
- `/api/users`, `/api/roles` - Direct endpoints
- `/api/rbac/users`, `/api/rbac/roles` - RBAC-specific endpoints

This violates single source of truth!

### ❌ Missing Unified Approach
- Users/roles should be documents with type discrimination
- Permissions should be documents too
- Membership should be relationship documents

## 5. Library Management

### ✅ Implemented
- Basic library CRUD exists
- Library metrics endpoints

### ❌ Missing
- `GET /api/libraries/:lib/quota` - Quota usage
- `GET /api/libraries/:lib/stats` - Detailed statistics
- Library templates not well integrated

## 6. Metrics & Monitoring

### ✅ Implemented
- `/api/metrics` - Prometheus format
- Various metric endpoints
- Health check at `/health`

### ❌ Missing
- `/api/health/ready` - Readiness probe
- `/api/health/live` - Liveness probe
- `/api/metrics/json` - JSON format option
- Metrics as documents (`type=metric`)

## 7. Configuration Management

### ✅ Implemented
- Basic config endpoints exist

### ❌ Missing
- Config as documents (`type=config`)
- Individual config key management
- Standardized config API

## 8. Response Format Standardization

### 🔧 Current Issues
- No consistent response envelope
- No pagination metadata in list responses
- Error responses not standardized
- No field projection support

### ❌ Need to Implement
```json
// List envelope
{
  "data": [...],
  "pagination": {
    "offset": 0,
    "limit": 100,
    "total": 250
  }
}

// Error envelope
{
  "error": {
    "code": "ERROR_CODE",
    "message": "Human readable",
    "details": {}
  }
}
```

## 9. Authentication Consistency

### 🔧 Current Issues
- Schema endpoints are public but should be authenticated
- Some endpoints missing authentication checks
- No consistent library context in auth token

## 10. JavaScript Function Management

### ✅ Implemented
- JavaScript integration exists
- Function storage and execution

### ❌ Missing
- Functions as documents (`type=function`)
- Unified API for validators/transformers
- Version management for functions

## Priority Implementation Plan

### Phase 1: Critical Foundation (Week 1)
1. **Standardize Response Format**
   - Implement response envelopes
   - Standardize error responses
   - Add pagination metadata

2. **Fix Authentication**
   - Consolidate login endpoints
   - Add session management endpoints
   - Implement library context switching

3. **Remove RBAC Duplication**
   - Remove `/api/rbac/*` endpoints
   - Make users/roles work through document API
   - Implement membership as documents

### Phase 2: Document Unification (Week 2)
1. **Virtual Collection Helpers**
   - Implement `/api/:type` pattern
   - Make them sugar over document API
   - Ensure type discrimination works

2. **Standardize Query Parameters**
   - Implement consistent filtering
   - Add field projection
   - Standardize pagination

3. **Config as Documents**
   - Migrate config to document storage
   - Implement per-key management

### Phase 3: Enhancement (Week 3)
1. **Batch Operations**
   - Implement batch create/update/delete
   - Add transaction support

2. **Metrics Improvements**
   - Add JSON format option
   - Store metrics as documents
   - Implement metric queries

3. **Function Management**
   - Migrate functions to documents
   - Add version management
   - Unify validators/transformers

## Breaking Changes

Moving to gold standard will introduce breaking changes:

1. **RBAC Endpoints** - `/api/rbac/*` will be removed
2. **Response Format** - All responses will use envelopes
3. **Authentication** - Schema endpoints will require auth
4. **Error Format** - Standardized error structure

## Migration Strategy

1. **Add Deprecation Warnings** - Add warnings to old endpoints
2. **Dual Support Period** - Support both old and new for 1 version
3. **Documentation** - Provide clear migration guide
4. **Client Libraries** - Update official clients first