# Memory Leak Fixes in database.c - skiplist_search

## Summary
Fixed multiple memory leaks in database.c where `skiplist_search` allocates memory with `BUFFER_ALLOC` that was not being freed properly. Each call to `skiplist_search` for document lookups returns an allocated buffer that must be freed with `BUFFER_FREE`.

## Key Understanding
- When searching for **libraries** and **collections**, `skiplist_search` returns direct pointers into the skiplist structure - these should NOT be freed
- When searching for **documents**, `skiplist_search` returns an allocated copy that MUST be freed with `BUFFER_FREE`

## Fixed Memory Leaks

### 1. `db_update_document` (Line 1209)
- **Issue**: `raw_data` from first `skiplist_search` was freed only in error paths, not success path
- **Fix**: Added `BUFFER_FREE(raw_data)` at line 1301 in success path

### 2. `db_update_document` (Line 1265)
- **Issue**: `old_ptr_data` from second `skiplist_search` was never freed
- **Fix**: 
  - Added `BUFFER_FREE(old_ptr_data)` in error path at line 1277
  - Added `BUFFER_FREE(old_ptr_data)` in success path at line 1296

### 3. `db_delete_document` (Line 1327)
- **Issue**: `raw_data` was never freed in any code path
- **Fix**: 
  - Added `BUFFER_FREE(raw_data)` in all error paths (lines 1333, 1340, 1351)
  - Added `BUFFER_FREE(raw_data)` in success path at line 1360

### 4. `storage_update_document` (Line 1807)
- **Issue**: `raw_data` was never freed in any code path
- **Fix**:
  - Added `BUFFER_FREE(raw_data)` in error paths (lines 1817, 1824)
  - Added `BUFFER_FREE(raw_data)` in success path at line 1859

## Already Fixed Locations
- `storage_delete_document` (Line 1884) - Already had proper `BUFFER_FREE(raw_data)` at line 1908
- `storage_get_document` (Line 1963) - Already had proper `BUFFER_FREE(raw_data)` at lines 1972 and 1976

## Pattern for Future Development
When using `skiplist_search` for documents:
```c
size_t value_len;
void* raw_data = skiplist_search(coll->documents, uuid, strlen(uuid) + 1, &value_len);
if (!raw_data || value_len != sizeof(json_value_t*)) {
    if (raw_data) BUFFER_FREE(raw_data);  // Free on error
    return ERROR;
}

// Use the data...
json_value_t* doc = *(json_value_t**)raw_data;

// Always free before return
BUFFER_FREE(raw_data);
return SUCCESS;
```

## Testing
Created `/opt/jdbx/test_memory_leaks.py` to exercise all fixed code paths:
- Creates, updates, reads, and deletes multiple documents
- Runs multiple iterations to amplify any remaining leaks
- Previously would leak memory on every operation
- Now properly frees all allocated memory

## Impact
These fixes eliminate memory leaks that occurred on every document operation, preventing server memory exhaustion during normal operation.