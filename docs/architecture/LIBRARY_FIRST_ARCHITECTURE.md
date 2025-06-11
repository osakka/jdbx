# Library-First Architecture Implementation Plan

## Overview
Transform JSONdb to treat libraries as first-class citizens with session-based library context, removing the need for path prefixes in API calls.

## Core Concept
- Libraries become a session context (like SQL's `USE database`)
- All API calls operate within the current library context
- Clean, simple URLs without library prefixes
- Per-library metrics and isolation

## Implementation Tasks

### 1. Library Context Switching (Priority: HIGH)
**Endpoint**: `POST /api/session/library`
```json
{
  "library": "system"
}
```
- Store library context in session
- Update JWT token with current library
- All subsequent API calls use this context
- Default to user's home library on login

### 2. Library Management API (Priority: HIGH)

#### Basic CRUD Operations
- `GET /api/libraries` - List all libraries
- `POST /api/libraries` - Create new library
- `DELETE /api/libraries/{name}` - Delete library  
- `GET /api/libraries/{name}` - Get library metadata

#### Extended Operations
- `PUT /api/libraries/{name}` - Update library settings
- `GET /api/libraries/{name}/stats` - Library statistics
- `POST /api/libraries/{name}/copy` - Clone library structure

### 3. Library Templates System (Priority: MEDIUM)
**Collection**: `system/library_templates`
```json
{
  "name": "standard",
  "features": {
    "auth": ["users", "roles", "permissions", "sessions"],
    "metrics": ["metrics", "audit"],
    "javascript": ["validators", "transformers", "functions"]
  }
}
```
- Templates define which collections to auto-create
- Immutable system templates (copy on create)
- Each library gets configured features

### 4. Per-Library Metrics (Priority: MEDIUM)
- Each library has its own `metrics` collection
- Metrics are written to `{library}/metrics`
- System aggregation job (run by `system-metrics` user):
  - Reads all `*/metrics` collections
  - Aggregates into `system/global_metrics`
  - Runs periodically (every 60 seconds)

### 5. UI Improvements

#### Document Viewing Fix (Priority: HIGH)
- Investigate why document clicking fails
- Likely issue with collection path format
- Fix API endpoint expectations

#### Smart Library Switcher (Priority: MEDIUM)
- Show on: Browser, Operations, API pages
- Hide on: Dashboard (unless in library view), Settings
- Context-aware visibility

#### Library Dropdown Enhancement (Priority: MEDIUM)
```
[📚 System        ▼]
   Default
   System
   MyApp
   ───────────────
   + Create New
   ⚙ Manage Libraries
```

#### Collection Button Spacing (Priority: LOW)
- Add 4px spacing between collection buttons
- Maintains compact layout

#### Dashboard Enhancements (Priority: MEDIUM)

##### Statistics Panel
```
Libraries    Collections    Documents
    5            24          1,337
   (+1)         (+3)         (+125)
```

##### View Toggle
- "Global View" - Shows system-wide metrics from `system/global_metrics`
- "Library View" - Shows current library metrics from `{library}/metrics`

##### Fix All Charts (Priority: MEDIUM)
- Operations over time
- Storage usage
- Response times  
- Connection activity

### 6. Field-Level Access API (Priority: HIGH)
**Pattern**: `/api/collections/{collection}/documents/{id}/{field}`
- Works with current library context
- Returns single field value
- Supports nested paths with dot notation
- Natural URL structure

## Benefits

1. **Cleaner APIs**: `/api/collections/users` instead of `/api/collections/system/users`
2. **True Multi-tenancy**: Complete isolation between libraries
3. **Natural Metrics**: Each library tracks its own metrics
4. **Scalability**: Easy to shard by library
5. **Flexibility**: Switch context without changing code

## Migration Path

1. Implement library context switching
2. Update API endpoints to use context
3. Migrate existing library/collection paths
4. Update UI to use new endpoints
5. Implement per-library metrics
6. Add aggregation job

## Next Steps

Ready to implement these changes one by one, starting with library context switching.