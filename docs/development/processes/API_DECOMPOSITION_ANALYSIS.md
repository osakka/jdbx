# API Decomposition Analysis

**File**: `src/components/core/api.c`  
**Current Size**: 5,057 lines  
**Total Routes**: 89 endpoints  
**Includes**: 25 headers  
**Status**: Phase 2.1 - Route Analysis Complete  

## Current Route Structure Analysis

### Routes by Category (89 total endpoints)

#### 1. Authentication & Session Management (13 routes)
```c
// Authentication routes (8)
{"/api/auth/login", HTTP_POST, api_handle_login, 0},
{"/api/auth/register", HTTP_POST, api_handle_register, 0},
{"/api/auth/refresh", HTTP_POST, api_handle_token_refresh, 0},
{"/api/auth/logout", HTTP_POST, api_handle_logout, 1},
{"/api/auth/session", HTTP_GET, api_handle_get_current_session, 1},
{"/api/auth/library", HTTP_GET, api_handle_get_library_context, 1},
{"/api/auth/library/", HTTP_POST, api_handle_switch_library, 1},
{"/api/auth/password", HTTP_PUT, api_handle_change_password, 1},

// Session management routes (4)
{"/api/sessions", HTTP_GET, api_handle_get_sessions, 1},
{"/api/sessions/active", HTTP_GET, api_handle_get_active_sessions, 1},
{"/api/sessions/", HTTP_POST, api_handle_session_terminate, 1},
{"/api/sessions/", HTTP_DELETE, api_handle_terminate_session, 1},

// Legacy auth (1)
{"/api/login", HTTP_POST, api_handle_login, 0},
```

**Target Module**: `api_auth.c`
- **Dependencies**: rbac/, jwt, sessions
- **Size Estimate**: ~800 lines
- **Complexity**: Medium (JWT, RBAC integration)

#### 2. Document Operations (14 routes)
```c
// Unified documents (6)
{"/api/documents", HTTP_GET, api_handle_unified_documents_query, 1},
{"/api/documents", HTTP_POST, api_handle_unified_documents_create, 1},
{"/api/documents/query", HTTP_POST, api_handle_unified_documents_query, 1},
{"/api/documents/", HTTP_GET, api_handle_unified_document_get, 1},
{"/api/documents/", HTTP_PUT, api_handle_unified_document_update, 1},
{"/api/documents/", HTTP_DELETE, api_handle_unified_document_delete, 1},

// Collection-scoped documents (7)
{"/api/collections/", HTTP_GET, api_handle_documents_query, 1},
{"/api/collections/", HTTP_GET, api_handle_document_field_access, 1},
{"/api/collections/", HTTP_GET, api_handle_document_get, 1},
{"/api/collections/", HTTP_POST, api_handle_document_create, 1},
{"/api/collections/", HTTP_PUT, api_handle_document_update, 1},
{"/api/collections/", HTTP_DELETE, api_handle_document_delete, 1},
{"/api/collections/", HTTP_DELETE, api_handle_virtual_collection_drop, 1},

// Library-scoped documents (2)
{"/api/libraries/", HTTP_GET, api_handle_documents_query, 1},
{"/api/libraries/", HTTP_POST, api_handle_document_create, 1},
```

**Target Module**: `api_documents.c`
- **Dependencies**: database/, document_storage, virtual_layer
- **Size Estimate**: ~1200 lines
- **Complexity**: High (unified architecture, field operations)

#### 3. Library Management (9 routes)
```c
// Library operations (3)
{"/api/libraries", HTTP_GET, api_handle_get_libraries, 1},
{"/api/libraries", HTTP_POST, api_handle_create_library, 1},
{"/api/library-templates", HTTP_GET, api_handle_get_library_templates, 1},

// Library-specific operations (6)
{"/api/libraries/", HTTP_DELETE, api_handle_delete_library, 1},
{"/api/libraries/", HTTP_GET, api_handle_library_document_field_access, 1},
{"/api/libraries/", HTTP_PUT, api_handle_update_library, 1},
{"/api/libraries/", HTTP_POST, api_handle_copy_library, 1},
{"/api/libraries/:library/metrics/query", HTTP_GET, api_handle_query_library_metrics, 1},
{"/api/libraries/:library/metrics", HTTP_GET, api_handle_library_metrics, 1},
{"/api/libraries/:library/metrics", HTTP_POST, api_handle_record_library_metric, 1},
```

**Target Module**: `api_libraries.c`
- **Dependencies**: library_api.h, library_metrics_api.h
- **Size Estimate**: ~600 lines
- **Complexity**: Medium (existing modules can be integrated)

#### 4. Collections & Virtual Collections (2 routes)
```c
{"/api/collections", HTTP_GET, api_handle_virtual_collections_list, 1},
{"/api/collections", HTTP_POST, api_handle_virtual_collection_create, 1},
```

**Target Module**: `api_collections.c`
- **Dependencies**: virtual_collections_api.h
- **Size Estimate**: ~300 lines
- **Complexity**: Low (simple wrapper)

#### 5. RBAC Management (10 routes)
```c
// User management (5)
{"/api/users", HTTP_GET, api_handle_users_list, 1},
{"/api/users", HTTP_POST, api_handle_user_create, 1},
{"/api/users/", HTTP_GET, api_handle_user_get, 1},
{"/api/users/", HTTP_PUT, api_handle_user_update, 1},
{"/api/users/", HTTP_DELETE, api_handle_user_delete, 1},

// Role management (5)
{"/api/roles", HTTP_GET, api_handle_roles_list, 1},
{"/api/roles", HTTP_POST, api_handle_role_create, 1},
{"/api/roles/", HTTP_GET, api_handle_role_get, 1},
{"/api/roles/", HTTP_PUT, api_handle_role_update, 1},
{"/api/roles/", HTTP_DELETE, api_handle_role_delete, 1},
```

**Target Module**: `api_rbac.c` (already exists, needs integration)
- **Dependencies**: rbac/, rbac_db.h
- **Size Estimate**: ~800 lines
- **Complexity**: Medium (existing implementation available)

#### 6. Metrics & Monitoring (8 routes)
```c
{"/api/metrics", HTTP_GET, health_api_handle_metrics, 1},
{"/api/metrics/stats", HTTP_GET, health_api_handle_metrics, 1},
{"/api/metrics/activity", HTTP_GET, health_api_handle_metrics, 1},
{"/api/metrics/export", HTTP_POST, health_api_handle_metrics_export, 1},
{"/api/metrics/history", HTTP_GET, api_handle_metrics_history, 1},
{"/api/metrics/aggregate", HTTP_GET, api_handle_metrics_aggregate, 1},
{"/api/metrics/adaptive-indexing", HTTP_GET, api_handle_adaptive_indexing_metrics, 1},
{"/health", HTTP_GET, api_handle_health_check, 0},
```

**Target Module**: `api_metrics.c`
- **Dependencies**: utils/metrics.h, health_api.h
- **Size Estimate**: ~500 lines
- **Complexity**: Low (mostly existing handlers)

#### 7. System Administration (4 routes)
```c
{"/api/system/info", HTTP_GET, api_handle_system_info, 1},
{"/api/system/log-control", HTTP_GET, api_handle_log_control, 1},
{"/api/system/log-control", HTTP_POST, api_handle_log_control, 1},
{"/api/openapi.json", HTTP_GET, api_handle_openapi_spec, 0},
```

**Target Module**: `api_system.c`
- **Dependencies**: logger.h, config
- **Size Estimate**: ~400 lines
- **Complexity**: Low (simple system operations)

#### 8. Data Visualization (6 routes)
```c
{"/api/visualization/collection-stats", HTTP_GET, api_handle_visualization_collection_stats, 1},
{"/api/visualization/document-types", HTTP_GET, api_handle_visualization_document_types, 1},
{"/api/visualization/field-distribution", HTTP_GET, api_handle_visualization_field_distribution, 1},
{"/api/visualization/transaction-history", HTTP_GET, api_handle_visualization_transaction_history, 1},
{"/api/visualization/transaction-metrics", HTTP_GET, api_handle_visualization_transaction_metrics, 1},
{"/api/visualization/transaction-relationships", HTTP_GET, api_handle_visualization_transaction_relationships, 1},
```

**Target Module**: `api_visualization.c`
- **Dependencies**: database/, transaction/
- **Size Estimate**: ~600 lines
- **Complexity**: Medium (analytics and visualization)

#### 9. Import/Export & Backup (3 routes)
```c
{"/api/export", HTTP_POST, api_handle_export, 1},
{"/api/import", HTTP_POST, api_handle_import, 1},
// Backup routes commented out - ready for implementation
```

**Target Module**: `api_backup.c`
- **Dependencies**: import_export, backup (to be implemented)
- **Size Estimate**: ~500 lines
- **Complexity**: Medium (data operations)

#### 10. Schema Validation (6 routes)
```c
{"/api/schemas", HTTP_GET, api_handle_schema_get, 0},
{"/api/schemas", HTTP_POST, api_handle_schema_create, 0},
{"/api/schemas/", HTTP_GET, api_handle_schema_get, 0},
{"/api/schemas/", HTTP_PUT, api_handle_schema_update, 0},
{"/api/schemas/", HTTP_DELETE, api_handle_schema_delete, 0},
{"/api/validate", HTTP_POST, api_handle_schema_validate, 1},
```

**Target Module**: `api_schemas.c`
- **Dependencies**: database/json_schema_manager.h
- **Size Estimate**: ~500 lines
- **Complexity**: Medium (validation logic)

#### 11. Index Management (8 routes)
```c
{"/api/indexes/", HTTP_GET, api_handle_index_list, 1},
{"/api/indexes/", HTTP_POST, api_handle_index_create, 1},
{"/api/indexes/", HTTP_GET, api_handle_index_get, 1},
{"/api/indexes/", HTTP_DELETE, api_handle_index_delete, 1},
{"/api/indexes/rebuild/", HTTP_POST, api_handle_index_rebuild, 1},
{"/api/indexes/stats/", HTTP_GET, api_handle_index_stats, 1},
{"/api/indexes/query/", HTTP_POST, api_handle_index_query, 1},
{"/api/indexes/compound/", HTTP_POST, api_handle_index_compound_query, 1},
```

**Target Module**: `api_indexes.c`
- **Dependencies**: database/index.h, adaptive_indexer.h
- **Size Estimate**: ~700 lines
- **Complexity**: Medium (index operations)

#### 12. JavaScript Integration (6 routes)
```c
{"/api/js/query", HTTP_POST, api_handle_js_query, 1},
{"/api/js/eval", HTTP_POST, api_handle_js_eval, 1},
{"/api/js/functions", HTTP_POST, api_handle_js_function_register, 1},
{"/api/js/functions/", HTTP_POST, api_handle_js_function_execute, 1},
{"/api/js/validators", HTTP_POST, api_handle_js_validator_register, 1},
{"/api/js/transformers", HTTP_POST, api_handle_js_transformer_register, 1},
```

**Target Module**: `api_javascript.c`
- **Dependencies**: js/, js_function_resolver.h
- **Size Estimate**: ~600 lines
- **Complexity**: Medium (JavaScript engine integration)

---

## Decomposition Strategy

### Phase 2.1: Core Module Separation (Priority 1)

1. **Document Operations** → `api_documents.c` (~1200 lines)
   - Highest complexity, most critical functionality
   - Unified documents architecture implementation
   - Field-level operations and collection management

2. **Authentication & Sessions** → `api_auth.c` (~800 lines)
   - Security-critical module
   - JWT, RBAC, and session management
   - Clean separation of auth logic

3. **RBAC Management** → Enhanced `api_rbac.c` (~800 lines)
   - Integrate existing api/rbac_api.c functionality
   - User and role management consolidation

### Phase 2.2: Feature Module Separation (Priority 2)

4. **Library Management** → `api_libraries.c` (~600 lines)
   - Integrate existing library_api.h functionality
   - Multi-tenancy and library operations

5. **Metrics & Monitoring** → `api_metrics.c` (~500 lines)
   - System health and performance monitoring
   - Analytics and reporting

6. **Index Management** → `api_indexes.c` (~700 lines)
   - Database indexing and optimization
   - Query performance management

### Phase 2.3: Advanced Features (Priority 3)

7. **JavaScript Integration** → `api_javascript.c` (~600 lines)
   - JavaScript engine operations
   - Custom functions and validation

8. **Data Visualization** → `api_visualization.c` (~600 lines)
   - Analytics and reporting endpoints
   - Chart and graph data generation

9. **System Administration** → `api_system.c` (~400 lines)
   - System configuration and management
   - Logging and administrative tasks

### Phase 2.4: Utility Modules (Priority 4)

10. **Schema Validation** → `api_schemas.c` (~500 lines)
    - Document schema management
    - Validation rules and enforcement

11. **Import/Export & Backup** → `api_backup.c` (~500 lines)
    - Data import/export functionality
    - Backup and restore operations

12. **Collections Management** → `api_collections.c` (~300 lines)
    - Virtual collections wrapper
    - Simple collection operations

---

## Implementation Plan

### Dependencies to Eliminate/Reduce

**Current 25 includes → Target <15 per module**

#### High-Priority Eliminations:
1. **Duplicate includes**: `database/document_storage.h` (appears twice)
2. **Circular dependencies**: Clean separation between modules
3. **Oversized includes**: Split large headers into focused interfaces

#### Module-Specific Dependencies:

**api_documents.c** (Core Database):
- `database/document_storage.h`
- `database/virtual_layer.h`
- `database/database.h`
- `utils/logger.h`
- `utils/buffer_pool.h`

**api_auth.c** (Authentication):
- `rbac/rbac.h`
- `rbac/jwt.h`
- `rbac/jwt_cache.h`
- `rbac/rbac_db.h`
- `api/auth_session_api.h`

**api_libraries.c** (Library Management):
- `api/library_api.h`
- `api/library_metrics_api.h`
- `database/document_storage.h`

### Quality Gates

1. **Build Verification**: Zero warnings with -Wall -Wextra
2. **API Compatibility**: Identical HTTP interface (zero regressions)
3. **Performance Testing**: No response time degradation
4. **Security Review**: Authentication and authorization preserved
5. **Integration Testing**: Full end-to-end validation

### Validation Criteria

1. **Route Preservation**: All 89 routes function identically
2. **Include Reduction**: <15 includes per module achieved
3. **File Size**: No module >1,200 lines
4. **Single Source of Truth**: No duplicate functionality
5. **Documentation**: Complete API documentation for each module

---

## Next Steps (Phase 2.1 Implementation)

### Immediate Actions:
1. Create `api_documents.c` with unified documents functionality
2. Extract document operation handlers from main `api.c`
3. Create focused header `include/api/api_documents.h`
4. Comprehensive testing of document operations
5. Validate zero regressions in document functionality

This decomposition will reduce the main `api.c` from 5,057 lines to ~1,000 lines of core routing logic, while creating 12 focused modules averaging ~600 lines each. Each module will have clear responsibilities and minimal dependencies, achieving the single source of truth principle while dramatically improving maintainability.