# JDBX API Implementation Plan - v6.3.0

## Current State Analysis

### ✅ Working
- Basic authentication (`/api/auth/login`)
- Health endpoint (`/api/health`)
- Unified documents storage backend
- JWT token generation
- Some library operations

### ❌ Issues Identified
1. **Memory corruption** (partially fixed - checkpoints disabled)
2. **Legacy endpoints** not properly mapped to unified storage
3. **RBAC permissions** not enforced consistently
4. **Document queries** returning empty results via legacy paths
5. **Library context** not properly managed
6. **Virtual collections** API incomplete

## Implementation Plan

### Phase 1: Fix Critical Issues (Day 1)
**Goal**: Stabilize existing functionality

1. **Fix Document Query Issues**
   - [ ] Fix `/api/libraries/:lib/collections/:col/documents` mapping
   - [ ] Ensure proper type field mapping for collections
   - [ ] Fix query parameter handling
   - [ ] Test with tenant library scenario

2. **Re-enable Memory Checkpoints Safely**
   - [ ] Move CORS header application inside checkpoint boundary
   - [ ] Ensure response lifecycle is properly managed
   - [ ] Add comprehensive tests

3. **Fix RBAC Permission Checks**
   - [ ] Implement consistent permission checking middleware
   - [ ] Fix "Permission denied" errors for valid users
   - [ ] Add proper role checking for admin operations

### Phase 2: Complete Core APIs (Day 2-3)
**Goal**: Implement all essential endpoints

1. **Unified Documents API**
   ```c
   // In api.c - register routes
   api_register_route(ctx, "/api/documents", HTTP_GET, api_handle_documents_query);
   api_register_route(ctx, "/api/documents", HTTP_POST, api_handle_documents_create);
   api_register_route(ctx, "/api/documents/:uuid", HTTP_GET, api_handle_document_get);
   api_register_route(ctx, "/api/documents/:uuid", HTTP_PUT, api_handle_document_update);
   api_register_route(ctx, "/api/documents/:uuid", HTTP_DELETE, api_handle_document_delete);
   ```

2. **Library Context Management**
   - [ ] Add X-Library-Context header support
   - [ ] Implement library switching endpoint
   - [ ] Store current library in JWT claims
   - [ ] Apply library filter to all queries

3. **Virtual Collections Mapping**
   ```c
   // Collection to type mapping
   const char* get_document_type(const char* collection) {
       if (strcmp(collection, "users") == 0) return "user";
       if (strcmp(collection, "roles") == 0) return "role";
       if (strcmp(collection, "sessions") == 0) return "session";
       if (strcmp(collection, "libraries") == 0) return "library";
       return collection; // Default to collection name
   }
   ```

### Phase 3: RBAC Implementation (Day 4-5)
**Goal**: Proper authentication and authorization

1. **Permission Middleware**
   ```c
   typedef struct {
       const char* resource;
       const char* action;
       bool (*check_func)(rbac_user_t* user, const char* resource);
   } permission_requirement_t;
   
   // Add to route registration
   api_register_protected_route(ctx, "/api/users", HTTP_GET, 
                               api_handle_users_list, "users:read");
   ```

2. **Role-Based Checks**
   - [ ] Implement role hierarchy (admin > user > guest)
   - [ ] Add resource-specific permissions
   - [ ] Library-scoped permission checks
   - [ ] Audit logging for permission denials

3. **Session Management**
   - [ ] Proper session storage (type="session")
   - [ ] Session expiry handling
   - [ ] Concurrent session limits
   - [ ] Session activity tracking

### Phase 4: Advanced Features (Day 6-7)
**Goal**: Complete feature set

1. **Field-Level Operations**
   - [ ] GET /api/documents/:uuid/:field
   - [ ] PATCH /api/documents/:uuid/:field
   - [ ] DELETE /api/documents/:uuid/:field
   - [ ] Nested field path support (e.g., "user.profile.email")

2. **Query Enhancements**
   - [ ] Advanced filtering (operators: $gt, $lt, $in, etc.)
   - [ ] Full-text search support
   - [ ] Aggregation pipelines
   - [ ] Sorting and pagination

3. **Metrics Integration**
   - [ ] Per-endpoint metrics collection
   - [ ] Library-scoped metrics
   - [ ] Performance monitoring
   - [ ] Error rate tracking

## Implementation Guidelines

### Code Structure
```
src/
├── components/api/
│   ├── unified_documents_api.c    # New unified API
│   ├── auth_api.c                 # Authentication endpoints
│   ├── library_context_api.c      # Library management
│   └── middleware/
│       ├── auth_middleware.c      # JWT validation
│       ├── rbac_middleware.c      # Permission checks
│       └── library_middleware.c   # Library context
```

### Endpoint Implementation Template
```c
http_response_t* api_handle_endpoint(api_context_t* ctx, http_request_t* request) {
    // 1. Authentication check (if required)
    rbac_user_t* user = get_authenticated_user(ctx, request);
    if (!user) {
        return http_response_error("Unauthorized", 401);
    }
    
    // 2. Permission check
    if (!rbac_check_permission(user, "resource:action")) {
        return http_response_error("Permission denied", 403);
    }
    
    // 3. Library context
    const char* library = get_library_context(request, user);
    
    // 4. Input validation
    json_value_t* body = parse_request_body(request);
    if (!validate_input(body)) {
        return http_response_error("Invalid input", 400);
    }
    
    // 5. Business logic
    json_value_t* result = perform_operation(ctx->db, library, body);
    
    // 6. Response formatting
    return create_standard_response(result, 200);
}
```

### Testing Strategy
1. **Unit Tests**: Each endpoint handler
2. **Integration Tests**: Full request/response flow
3. **Load Tests**: Concurrent operations
4. **Security Tests**: Permission boundaries
5. **Regression Tests**: Legacy compatibility

## Success Criteria

### Phase 1 Complete When:
- [ ] No memory corruption under load
- [ ] Document queries return correct results
- [ ] Basic RBAC working for admin/user

### Phase 2 Complete When:
- [ ] All unified document endpoints working
- [ ] Library context switching functional
- [ ] Virtual collections properly mapped

### Phase 3 Complete When:
- [ ] All endpoints properly protected
- [ ] Permission system fully functional
- [ ] Session management complete

### Phase 4 Complete When:
- [ ] Field-level operations working
- [ ] Advanced queries supported
- [ ] Metrics fully integrated

## Risk Mitigation

1. **Memory Management**: Keep checkpoint system disabled until fully understood
2. **Backwards Compatibility**: Maintain legacy endpoints during transition
3. **Data Migration**: No data format changes, only access patterns
4. **Performance**: Monitor response times and optimize queries
5. **Security**: Audit all permission checks before deployment