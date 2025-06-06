# Session and Metrics Duplication Fix - Investigation and Resolution

## RBAC UUID Migration Fix (Updated 2025-06-05)

### Problem
The RBAC system was creating documents with the old "doc-timestamp-nanoseconds-random" format instead of UUIDs, causing inconsistency and potential duplication issues.

### Root Causes
1. **Multiple ID Generation Systems**: RBAC had its own `generate_document_id()` function creating old format IDs
2. **Query Result Limitations**: `db_query_documents` doesn't return the `_id` field, only document content
3. **Insert Result Mismatch**: `db_insert_document` returned the document without the generated `_id`

### Changes Made
1. **Removed Old ID Generation** (`rbac_database.c`):
   - Removed `generate_document_id()` function
   - Let `db_insert_document` generate UUIDs automatically
   
2. **Fixed Insert Return Value** (`database.c`):
   - Added `_id` field to returned document: `json_object_set(result_doc, "_id", json_create_string(doc_id));`
   - Now callers can get the generated UUID

3. **Updated RBAC Creation Functions**:
   - Modified to extract and use IDs from insert results
   - Properly pass admin role ID to user creation

### Current Status
- ✅ New RBAC documents use UUIDs: `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`
- ✅ Sessions created with UUIDs
- ⚠️ Query results still don't include `_id` (separate issue)
- ⚠️ Authentication may need updates for query limitations

### Verification
Test results show UUID format is now used:
- Admin user UUID: `30d5d716-f2ef-4c75-b0c9-387a0404e754`
- Admin role UUID: `71975eb2-93c0-4c58-890a-e1ccb5e8d6a2`

---

# Session and Metrics Duplication Fix - Investigation and Resolution

## Session Duplication Issue
The JSONdb server was creating new sessions on every login instead of reusing existing sessions. This led to:
- Multiple active sessions for the same user
- Session table growing unbounded
- Potential performance issues with large session counts

## Root Cause
In `/opt/jsondb/src/components/core/authentication_handler.c`, the `api_handle_login` function was creating a new session every time a user logged in without:
1. Checking for existing active sessions
2. Invalidating old sessions before creating new ones

This meant that every login (including page refreshes in the web UI) created a new session entry.

## Investigation Findings
1. **Session Creation**: Sessions were only created during login (line 118 in `authentication_handler.c`)
2. **No Reuse Logic**: The login handler didn't check for existing sessions
3. **Authentication Works**: The `api_authenticate_request_sliding` function properly validates and extends existing sessions
4. **Database Evidence**: Found 496 active sessions and 44 inactive sessions for a single user

## Fix Implementation
Modified the `api_handle_login` function to:

1. **Query existing sessions** before creating a new one:
```c
json_value_t* session_query = json_create_object();
json_object_set(session_query, "user_id", json_create_string(user_id));
json_object_set(session_query, "active", json_create_boolean(1));
```

2. **Invalidate all existing active sessions** for the user:
```c
for (size_t i = 0; i < documents->value.array.size; i++) {
    json_value_t* session = json_array_get(documents, i);
    json_value_t* session_id_val = json_object_get(session, "_id");
    if (session_id_val && session_id_val->type == JSON_STRING) {
        const char* old_session_id = session_id_val->value.string;
        LOG_DEBUG("LOGIN: Invalidating old session: %s", old_session_id);
        rbac_db_invalidate_session(ctx->db, old_session_id);
    }
}
```

3. **Create a new session** only after invalidating old ones

## Verification
After the fix:
- Login logs show old sessions being invalidated: `LOGIN: Invalidating old session: doc-xxx`
- Only one new session is created per login
- Old sessions are properly marked as inactive with `active: false`

## Recommendations
1. Consider implementing a session cleanup job to remove old inactive sessions
2. Add a configuration option for maximum sessions per user
3. Consider implementing "single session" mode where only one active session is allowed per user
4. Add session management endpoints for users to view/manage their sessions

## Files Modified
- `/opt/jsondb/src/components/core/authentication_handler.c` - Added session invalidation logic to login handler

---

## Metrics Document Duplication Issue

### Problem Identification
The JSONdb UI is showing duplicate metric documents due to a bug in the metrics persistence system. The investigation revealed:

1. **Root Cause**: The metrics persistence system loses track of existing metric document IDs on server restart
2. **Impact**: Every server restart creates new metric documents instead of updating existing ones
3. **Evidence**: 
   - 4 duplicate documents for each metric type (operations, performance, memory, cache)
   - New documents created with timestamps showing server restart times
   - Logs show `db_insert_document` being called instead of `db_update_document`

### Technical Details
The issue is in `/opt/jsondb/src/components/utils/metrics_persistence.c`:

1. Global variables store metric document IDs:
   ```c
   static char* g_metric_id_operations = NULL;
   static char* g_metric_id_performance = NULL;
   static char* g_metric_id_cache = NULL;
   static char* g_metric_id_memory = NULL;
   static char* g_metric_id_connections = NULL;
   ```

2. These IDs are cleared on shutdown but not persisted or restored on startup

3. The `update_metric_document` function checks for existing documents:
   - First checks if we have a stored ID
   - If no ID, searches by name using `find_metric_by_name`
   - If found, updates the existing document
   - If not found, creates a new document

### Solution
The fix requires modifying the metrics persistence initialization to:

1. On startup, search for existing metric documents by name
2. Store their IDs in the global variables
3. Ensure updates use the existing document IDs

### Implementation Plan
1. Modify `metrics_persistence_init()` to find and cache existing metric document IDs
2. Add a cleanup function to remove duplicate documents
3. Ensure the system properly tracks document IDs across restarts

### Temporary Workaround
To clean up existing duplicates:

```bash
# Delete duplicate metric documents (keep only the most recent)
curl -X GET "http://localhost:5000/api/collections/_system_metrics" \
  -H "Authorization: Bearer $TOKEN" | \
  jq '.documents | group_by(.name) | 
      map(sort_by(.created_at) | .[:-1][]) | 
      .[]._id' | \
  xargs -I {} curl -X DELETE "http://localhost:5000/api/collections/_system_metrics/{}" \
    -H "Authorization: Bearer $TOKEN"
```

### Prevention
1. Implement proper ID persistence across restarts
2. Add unique index on metric name field
3. Add server startup checks for existing metrics
4. Consider using a more robust document lookup strategy