# ADR-035: Static File Serving Integration Fix

## Status
**ACCEPTED** - Implemented June 20, 2025

## Context
During UI modernization testing, we discovered that JDBX v6.5.13 was failing to serve static files (HTML, CSS, JavaScript), causing authentication redirect loops:

1. **Symptom**: Users logged in successfully but were immediately kicked out
2. **Root Cause**: Static files like `/login.html`, `/js/theme.js`, `/css/styles.css` returned "No matching route" errors
3. **Impact**: Complete UI unusability despite successful backend authentication

The logs showed:
```
WARNING] api_dispatch_request.api 862: No matching route found for: /login.html
WARNING] api_dispatch_request.api 784: Authentication failed for route: /api/documents
```

## Decision
We implemented proper request routing in `handle_client.c` to check for static files BEFORE calling the API dispatcher.

## Implementation
Modified `/opt/jdbx/src/components/core/handle_client.c` to add static file routing:

```c
/* Check if this is a static file request (not API) */
if (request->method == HTTP_GET && is_admin_route(request->path)) {
    response = serve_admin_file(request->path);
    if (response && g_logger) {
        TRACE_NET("STATIC_FILE_SERVED: path=%s, status=%d", 
            request->path, response->status);
    }
}

/* If no static file response, try API dispatch */
if (!response) {
    response = api_dispatch_request(client->api_ctx, request);
    // ... existing API handling
}
```

## Consequences

### Positive
- Static files are now properly served (200 OK responses)
- UI authentication flow works correctly
- No more redirect loops
- Existing static file serving code in `static_files.c` is properly utilized
- Zero regression - API endpoints continue to work normally

### Negative
- None identified

## Technical Details
- **Files Modified**: `src/components/core/handle_client.c`
- **Functions Used**: `is_admin_route()`, `serve_admin_file()` from existing `static_files.c`
- **Routing Order**: Static files checked first, then API routes
- **Performance Impact**: Minimal - one additional function call for GET requests

## Testing
Verified static file serving:
```bash
curl -k https://localhost:5000/js/theme.js -I  # 200 OK
curl -k https://localhost:5000/index.html -I   # 200 OK
curl -k https://localhost:5000/css/styles.css -I # 200 OK
```

## Lessons Learned
1. **Request Routing Order Matters**: Static files must be checked before API routing
2. **Existing Code Utilization**: The static file serving code existed but wasn't being called
3. **UI Testing Critical**: Backend changes can break UI in subtle ways through routing issues

## Related
- ADR-034: Memory Promotion for Global Structures
- UI Modernization Guide (`/opt/jdbx/share/htdocs/UI_MIGRATION_GUIDE.md`)