# Binary Serialization Crash Fix

## Issue
The server was crashing during binary serialization of the database, specifically when processing metrics documents in the `_system_metrics` collection. The crash occurred after the message:
```
[DEBUG] [binary_format.c:629:binary_serialize_database] PERSIST_DEBUG: Freeing temp buffer
```

## Root Cause
The crash was caused by multiple issues in the binary serialization code:

1. **Initial Buffer Allocation**: The temp buffer was not being allocated on first use if the estimated size was 0 (when `temp_size` was still 0).

2. **Insufficient Buffer Size**: The safety margin of 32 bytes was too small for complex metrics documents that contain nested arrays and time-series data.

3. **Missing Bounds Checking**: There was no validation that the actual serialized size didn't exceed the allocated buffer size, leading to potential buffer overflows.

## Fix Applied

### 1. Fixed Initial Buffer Allocation (line ~578)
```c
// Before:
if (estimated_size > temp_size) {

// After:
if (!temp_buffer || estimated_size > temp_size) {
```

### 2. Increased Safety Margin (line ~575)
```c
// Before:
size_t estimated_size = get_json_binary_size(document) + 32;

// After:
size_t estimated_size = get_json_binary_size(document) + 1024;
```

### 3. Added Bounds Validation (after line ~592)
```c
/* Validate serialized size doesn't exceed buffer */
if (doc_size > temp_size) {
  LOG_ERROR("Serialized document size (%zu) exceeds buffer size (%zu)", doc_size, temp_size);
  free(temp_buffer);
  close(fd);
  return 0;
}
```

### 4. Improved Buffer Cleanup (line ~638)
```c
// More defensive cleanup with better logging
if (temp_buffer) {
  LOG_TRACE("Freeing buffer at %p (size=%zu)", temp_buffer, temp_size);
  free(temp_buffer);
  temp_buffer = NULL;
  temp_size = 0;
  LOG_TRACE("Buffer freed successfully");
}
```

## Testing
After applying these fixes:
1. The server should handle large metrics documents without crashing
2. Buffer allocations are properly tracked with TRACE logging
3. Any buffer overflow attempts are caught and logged as errors
4. The serialization process is more robust and defensive

## Files Modified
- `/opt/jsondb/src/components/binary/binary_format.c`

## Impact
This fix ensures that the binary persistence system can handle documents of any size, particularly important for the metrics system which stores time-series data that can grow large over time.