# JSONdb Memory Allocation Audit Report

## Executive Summary

This comprehensive audit identifies critical mixed memory allocation patterns that are causing authentication crashes in the JSONdb codebase. The primary issue is the mixing of `malloc()` and `buffer_pool_alloc()` allocations with inconsistent free operations.

## Critical Issues Found

### 1. **JWT Module - Critical Severity**

**File**: `/opt/jsondb/src/components/rbac/jwt.c`

**Issues Identified**:

#### Base64 Functions (Lines 15-141)
- `base64_encode()` (Line 17): Uses `malloc()` but callers may use `buffer_pool_free()`
- `base64_decode()` (Line 55): Uses `malloc()` but callers may use `buffer_pool_free()`
- `base64_url_decode()` (Line 109, 122): Uses `strdup()` and `malloc()` but expects `free()`

#### JWT Token Structure (Lines 262-328)
- `jwt_create()` (Line 262): Uses `malloc()` for token structure
- `jwt_create()` (Line 268): Uses `malloc()` for header structure  
- `jwt_create()` (Line 274): Uses `malloc()` for payload structure
- `jwt_create()` (Line 282-283): Uses `strdup()` for alg/typ strings
- `jwt_free()` (Line 302): Uses `free()` but calls `json_free()` which uses `buffer_pool_free_safe()`

#### String Duplication Mixing (Lines 282-420)
- Multiple `strdup()` calls throughout JWT functions
- Mixed with `json_create_string()` which uses `buffer_pool_alloc()`
- Callers use both `free()` and `buffer_pool_free_safe()`

#### JWT Encoding Issues (Lines 442-593)
- `jwt_encode()` Line 468: `buffer_pool_free_safe(header_str)` but `header_str` from `json_stringify()`
- `jwt_encode()` Line 538: `buffer_pool_free_safe(payload_str)` but `payload_str` from `json_stringify()`
- Mixed malloc allocations for header_payload (Line 546) and token_str (Line 565)

### 2. **JSON Module - High Severity**

**File**: `/opt/jsondb/src/components/utils/json.c`

**Issues Identified**:

#### JSON Stringify Functions (Lines 678-855)
- `stringify_object()` Line 730: `free(result)` but `result` from `buffer_pool_alloc()` (Line 717)
- `stringify_object()` Line 736: `free(key)` but `key` from `escape_string()` using `buffer_pool_alloc()` (Line 633)
- `stringify_object()` Line 743: `free(result)` but `result` from `buffer_pool_alloc()` 
- `stringify_object()` Line 749: `free(value)` but `value` from `stringify_value()` using buffer pool
- `stringify_array()` Line 804: `free(result)` but `result` from `buffer_pool_alloc()` (Line 792)
- `stringify_array()` Line 810: `free(value)` but `value` from `stringify_value()` using buffer pool

#### Escape String Function (Line 614)
- `escape_string()` uses `buffer_pool_alloc()` (Line 633)
- Callers in `stringify_object()` use `free()` instead of `buffer_pool_free()`

### 3. **JSON Helpers Module - Medium Severity**

**File**: `/opt/jsondb/src/components/utils/json_helpers.c`

**Issues Identified**:

#### String Duplication (Lines 47-92)
- `json_object_keys()` Line 47: Uses `strdup()` but callers may expect buffer pool allocation
- `json_export_collection()` Line 85, 92: Uses `free()` on `json_stringify()` result (buffer pool)
- `json_import_collection()` Lines 201, 204: Uses `free()` on `json_stringify()` result
- `json_import_database()` Lines 291, 294: Uses `free()` on `json_stringify()` result

### 4. **Buffer Pool Safe Free Implementation**

**File**: `/opt/jsondb/src/components/utils/buffer_pool.c`

**Current Implementation** (Lines 350-358):
```c
void buffer_pool_free_safe(void* ptr) {
    if (!ptr) return;
    
    if (is_buffer_pool_memory(ptr)) {
        buffer_pool_free(ptr);
    } else {
        free(ptr);
    }
}
```

**Issue**: The `is_buffer_pool_memory()` function checks for a magic number that may not be reliable for all allocations.

## Root Cause Analysis

### Primary Causes:

1. **Inconsistent Allocation Strategy**: Some functions use `malloc()/strdup()` while others use `buffer_pool_alloc()/buffer_pool_strdup()`

2. **JSON Stringify Mixed Returns**: `json_stringify()` returns buffer pool allocated memory, but callers often use `free()` instead of `buffer_pool_free_safe()`

3. **JWT Module Allocation Inconsistency**: JWT structures use `malloc()` but contain JSON values that use buffer pool allocation

4. **String Handling Confusion**: Mix of `strdup()` (malloc) and `buffer_pool_strdup()` with inconsistent free operations

## Prioritized Fix List

### **Priority 1 - Critical (Fix Immediately)**

1. **JWT Base64 Functions** - Convert to buffer pool or ensure consistent free operations
   - `base64_encode()` - Line 17 in `jwt.c`
   - `base64_decode()` - Line 55 in `jwt.c` 
   - `base64_url_decode()` - Lines 109, 122 in `jwt.c`

2. **JWT Token Creation** - Standardize on buffer pool allocation
   - `jwt_create()` - Lines 262, 268, 274 in `jwt.c`
   - All `strdup()` calls in JWT module

3. **JSON Stringify Free Operations** - Fix all `free()` calls to `buffer_pool_free_safe()`
   - `stringify_object()` - Lines 730, 736, 743, 749 in `json.c`
   - `stringify_array()` - Lines 804, 810 in `json.c`

### **Priority 2 - High (Fix Soon)**

4. **JWT Encoding/Decoding** - Ensure consistent allocation strategy
   - `jwt_encode()` - Lines 468, 538 in `jwt.c`
   - `jwt_decode()` - All string allocations

5. **JSON Helpers String Operations** - Standardize string allocation
   - `json_object_keys()` - Line 47 in `json_helpers.c`
   - All `json_stringify()` result handling

### **Priority 3 - Medium (Fix When Convenient)**

6. **Buffer Pool Magic Number Validation** - Improve reliability of `is_buffer_pool_memory()`

7. **String Pool Integration** - Consider migrating all string operations to string pool

## Recommended Implementation Strategy

### Phase 1: Immediate Fixes
1. Replace all `free()` calls on `json_stringify()` results with `buffer_pool_free_safe()`
2. Convert JWT base64 functions to use buffer pool allocation
3. Standardize JWT token structure allocation to buffer pool

### Phase 2: Systematic Cleanup  
1. Create unified string allocation strategy
2. Audit all `strdup()` usage and convert to `buffer_pool_strdup()`
3. Enhance buffer pool magic number detection

### Phase 3: Testing and Validation
1. Add memory allocation tracking in debug builds
2. Create unit tests for mixed allocation scenarios
3. Implement allocation strategy enforcement

## Testing Recommendations

1. **Memory Allocation Tests**: Create tests that specifically exercise the authentication flow with memory debugging enabled
2. **Valgrind Analysis**: Run authentication scenarios under Valgrind to detect mixed allocation issues
3. **Unit Tests**: Create focused tests for each function identified above

## Conclusion

The authentication crashes are primarily caused by systematic mixing of `malloc()` and `buffer_pool_alloc()` throughout the JWT and JSON handling code. The most critical fixes are in the JWT module where base64 encoding and token creation use malloc but interact with buffer pool allocated JSON structures.

**Estimated Fix Effort**: 2-3 days for Priority 1 fixes, 1 week for complete resolution.

**Risk Level**: High - Authentication is critical functionality and these issues can cause crashes in production.