# Log Format String Fix

## Issue
Server was hanging during startup with the following error:
```
[DEBUG] [simplified_db.c:1035:db_query_documents] Collection: (null)
```

The metrics persistence thread was crashing because log statements were missing their format string arguments.

## Root Cause
Multiple log statements throughout the codebase had format specifiers (`%s`) but were missing the actual parameter values. For example:
```c
LOG_DEBUG("Collection: %s");              // Missing collection_name parameter
LOG_ERROR("Collection not found: %s");    // Missing collection_name parameter
```

## Files Fixed

### simplified_db.c
- Fixed 5 instances of `LOG_DEBUG("Collection: %s")` → `LOG_DEBUG("Collection: %s", collection_name)`
- Fixed 5 instances of `LOG_ERROR("Collection not found: %s")` → `LOG_ERROR("Collection not found: %s", collection_name)`

### indexed_document_operations.c
- Fixed `LOG_ERROR("create document index for collection %s")` → Added collection_name parameter
- Fixed `LOG_INFO("Created new document index for collection %s")` → Added collection_name parameter
- Fixed 2 instances of `LOG_ERROR("get or create index for collection %s")` → Added collection_name parameter
- Fixed 3 instances of `LOG_ERROR("Collection not found: %s")` → Added collection_name parameter

### simplified_operations.c
- Fixed 5 instances of `LOG_DEBUG("Collection: %s")` → Added collection_name parameter
- Fixed 5 instances of `LOG_ERROR("Collection not found: %s")` → Added collection_name parameter

### optimized_db_operations.c
- Fixed 3 instances of `LOG_ERROR("Collection not found: %s")` → Added collection_name parameter

## Impact
- Server now starts correctly without hanging
- Log messages now properly display the actual collection names
- Metrics persistence thread no longer crashes on null collection names
- Debugging is significantly improved with accurate log output

## Prevention
When using format specifiers in log statements, always ensure the corresponding parameters are provided:
```c
// Wrong
LOG_ERROR("Failed to process %s");

// Correct
LOG_ERROR("Failed to process %s", item_name);
```

## Verification
After the fix:
- Server starts successfully
- Log messages show actual collection names
- Metrics persistence thread functions correctly
- No null pointer issues in log output