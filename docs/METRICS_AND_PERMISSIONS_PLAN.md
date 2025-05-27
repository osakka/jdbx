# Metrics Storage and Permissions Enhancement Plan

## Current State Analysis

### 1. Metrics Storage Issues
- Currently creating a new document every 60 seconds
- Causing performance degradation (400ms response times)
- No historical data retention mechanism
- Document IDs are auto-generated, not predictable

### 2. Permission System
The numeric values in permissions (0,1,2,3,4,5) represent resource types:
- 0 = RBAC_DATABASE
- 1 = RBAC_COLLECTION  
- 2 = RBAC_DOCUMENT
- 3 = RBAC_USER
- 4 = RBAC_ROLE
- 5 = RBAC_PERMISSION

Format: "resource_type:resource_id" → permission_value (bitmask)
- READ = 1, WRITE = 2, DELETE = 4, ADMIN = 8
- Example: "1:*" → 15 means all permissions on all collections

## Implementation Plan

### Phase 1: Metrics Storage Redesign

#### 1.1 Collection Structure
- Collection name: `system_metrics`
- Document IDs: Fixed, predictable names
  - `metrics_operations` - Operation counts and types
  - `metrics_performance` - Response times, throughput
  - `metrics_cache` - Cache hit rates, evictions
  - `metrics_connections` - Active connections, connection stats
  - `metrics_memory` - Memory usage over time

#### 1.2 Document Structure
```json
{
  "_id": "metrics_operations",
  "type": "operations",
  "retention_minutes": 15,
  "max_entries": 15,
  "data": [
    {
      "timestamp": "2025-01-27T12:00:00Z",
      "total": 1234,
      "read": 800,
      "write": 434,
      "database": 100
    },
    // ... up to max_entries
  ],
  "current": {
    "total": 1234,
    "read": 800,
    "write": 434,
    "database": 100
  }
}
```

#### 1.3 Update Logic
- Check if document exists
- If exists, append new data point
- Trim array to max_entries (FIFO)
- Update current values
- Single atomic update operation

### Phase 2: Permissions Enhancement

#### 2.1 System Metrics Collection Permissions
- Admin role already has "1:*" (all collections) permission
- No changes needed - admin can already access system_metrics
- Add explicit permission check for system collections

#### 2.2 Permission Documentation
- Create clear mapping of numeric values to resource types
- Document permission bitmask values
- Add helper functions to translate numeric to human-readable

### Phase 3: Implementation Steps

1. **Modify metrics_persistence.c**
   - Implement new document structure
   - Add retention logic
   - Use fixed document IDs
   - Implement append-and-trim logic

2. **Create system_metrics collection handler**
   - Special handling for system collections
   - Ensure proper initialization
   - Add collection metadata

3. **Update API endpoints**
   - Modify /api/metrics to read from new structure
   - Add /api/system/metrics/:metric_type endpoint
   - Ensure backward compatibility

4. **Migration and Cleanup**
   - Delete old metrics documents
   - Initialize new metric documents
   - Preserve current metric values

5. **Testing**
   - Verify metrics update correctly
   - Check retention works
   - Confirm performance improvement
   - Test permission access

### Phase 4: Human-Readable Permissions (Future Enhancement)

Consider adding permission aliases:
```json
{
  "permissions": {
    "collections:*": ["READ", "WRITE", "DELETE", "ADMIN"],
    "system_metrics:*": ["READ"]
  }
}
```

This can coexist with numeric system for backward compatibility.

## Success Criteria

1. **Performance**: Response times back to ~1ms range
2. **Storage**: Only 5 metric documents (one per type)
3. **History**: 15 minutes of historical data per metric
4. **Permissions**: Admin can view/edit system metrics
5. **Stability**: No regression in existing functionality

## Implementation Order

1. Create new metrics storage implementation
2. Test with single metric type
3. Extend to all metric types
4. Cleanup old metrics
5. Verify performance improvement
6. Document changes