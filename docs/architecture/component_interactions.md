# JDBX Component Interactions

This document describes the key interactions between components in the JDBX system. Understanding these interactions is crucial for maintaining and extending the codebase.

## Core Component Overview

JDBX consists of the following primary components that interact with each other:

- **Core Server**: HTTP server, request handling, and lifecycle management
- **Database Engine**: JSON storage, indexing, and query processing
- **JavaScript Engine**: Script execution environment (optional)
- **RBAC System**: Role-based access control for security
- **API Layer**: REST API endpoints for client interaction
- **Transaction System**: ACID compliance and transaction management
- **Utility Layer**: Common functions used across components

## Key Component Interactions

### 1. Server and Database Initialization

```
main.c
  ├─ init_paths() -> Resolves paths relative to binary
  ├─ db_init() -> Creates database instance
  ├─ rbac_refcount_init() -> Initializes RBAC system
  ├─ metrics_registry_create() -> Creates metrics registry
  ├─ api_create_context() -> Creates API context
  └─ server_init() -> Initializes HTTP server
```

The initialization process carefully sets up components in dependency order, ensuring each has access to its required dependencies.

### 2. JavaScript Integration

```
main.c
  └─ [Optional: Only if JavaScript support is enabled]
      └─ js_api_init() -> Initializes JavaScript engine
          ├─ Registers database API with JavaScript context
          └─ Sets up file resolution system

js_engine.c
  ├─ Manages QuickJS runtime
  ├─ Provides functions to execute JS code
  └─ Handles memory management for JS objects
```

JavaScript integration is conditionally compiled using `#ifndef DISABLE_JS` directives, allowing the system to be built with or without JavaScript support.

### 3. API Request Handling

```
server.c
  ├─ server_handle_request() -> Entry point for HTTP requests
  │   └─ route_to_handler() -> Routes to appropriate API handler
  │
api/<api_type>.c
  ├─ handle_<action>_request() -> Processes specific API request
  │   ├─ Validates inputs
  │   ├─ Performs database operations
  │   └─ Returns JSON response
  │
database.c
  └─ db_* functions -> Core database operations
```

API requests flow through the server to specific handlers, which interact with the database and return JSON responses.

### 4. Transaction Management

```
transaction.c
  ├─ transaction_begin() -> Starts a new transaction
  ├─ transaction_commit() -> Commits changes
  └─ transaction_rollback() -> Reverts changes

transaction_retry.c
  └─ Handles transaction retry logic for deadlock avoidance

transaction_log.c
  └─ Maintains audit trail of transactions
```

Transactions provide ACID guarantees and integrate with the database operations for consistency.

### 5. Role-Based Access Control

```
rbac.c
  ├─ rbac_check_permission() -> Checks if action is allowed
  └─ rbac_authenticate() -> Validates credentials

jwt.c
  ├─ jwt_create() -> Creates authentication tokens
  └─ jwt_verify() -> Validates tokens

rbac_refcount.c
  └─ Reference counting for efficient RBAC management
```

RBAC integrates with API handlers to enforce access control on all operations.

## Conditional Compilation

The system supports building with or without JavaScript support using conditional compilation:

```c
#ifndef DISABLE_JS
    // JavaScript-dependent code
#else
    // Stub implementations when JavaScript is disabled
#endif
```

This allows deploying a smaller, more focused build in environments where JavaScript extensions are not needed.

## Data Flow Diagram

```
Client Request
    │
    ▼
HTTP Server (server.c)
    │
    ▼
API Handler (api/*.c)
    │
    ├───────┬───────┬───────┐
    ▼       ▼       ▼       ▼
 Database   RBAC   Metrics  JS Engine
  Engine   System  Registry (optional)
```

## Common Pitfalls and Solutions

1. **JavaScript Initialization Order**: Always initialize the JavaScript engine after database initialization, as JavaScript functions depend on database access.

2. **Memory Management**: Use reference counting for shared objects that cross component boundaries to prevent memory leaks.

3. **Path Resolution**: Always use the path resolution utilities when dealing with file paths to ensure correct behavior across different deployment environments.

4. **Conditional Compilation**: When adding new JavaScript-dependent features, ensure they are properly wrapped in `#ifndef DISABLE_JS` directives and provide appropriate stubs for non-JavaScript builds.

5. **Transaction Scope**: Be careful about transaction boundaries to avoid deadlocks or inconsistent states. Use the transaction retry mechanism for operations that might conflict.

## Build System Integration

Components are organized in the build system according to their dependencies:

```
Core Components (no dependencies)
    │
    ▼
Utility Layer (depends on Core)
    │
    ▼
Database Engine (depends on Utility)
    │
    ▼
API Layer (depends on Database)
    │
    ▼
Optional Features (JavaScript, etc.)
```

The Makefile respects these dependencies and allows conditional compilation of optional features.