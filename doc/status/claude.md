# JSON Database Server Project Status

> **IMPORTANT**: This document provides a high-level overview of the project status. The definitive source of truth is the `project-status.json` file in the project root. Always refer to and keep `project-status.json` updated to maintain an accurate representation of the project state.

## Project Overview

The JSON Database Server is a lightweight, multithreaded database server that stores and manages JSON documents with collections. It provides a RESTful API for data management and includes features such as role-based access control, JWT authentication, SSL/TLS support, and ACID transactions.

## Core Features

| Feature | Status | Description |
|---------|--------|-------------|
| JSON Document Database | Stable | Stores and retrieves JSON documents in collections |
| RESTful API | Stable | Comprehensive API for all database operations |
| Role-based Access Control | Stable | User, role, and permission management system |
| JWT Authentication | Stable | Secure token-based authentication |
| SSL/TLS Support | Stable | Secure communication between clients and server |
| Schema Validation | Stable | JSON schema validation for documents |
| Web-based Admin Interface | Stable | Interface for managing collections, documents, and users |
| Document Caching | Stable | Document caching with multiple eviction policies |
| JavaScript Extensions | Stable | Integration with QuickJS for custom logic |
| Query Language | Stable | MongoDB-like query syntax for filtering documents |
| Indexing | Stable | Optimized queries with custom index support |

## Transaction System Components

| Component | Status | Description |
|-----------|--------|-------------|
| ACID Transactions | Stable | Atomicity, Consistency, Isolation, Durability guarantees |
| Transaction Logging | Stable | Logging system for crash recovery and auditing |
| Transaction Visualization | Alpha | Visualization tools for transaction monitoring |
| Deadlock Resolution | Beta | Automatic detection and resolution of deadlocks |
| Transaction Isolation | Stable | Multiple isolation levels (read uncommitted, read committed, serializable) |
| Transaction Audit Trail | Beta | Detailed audit logs for all transaction operations |
| Transaction Retry | Stable | Automatic retry mechanism for failed transactions |
| Transaction Savepoints | Beta | Support for partial rollbacks within transactions |
| Transaction Monitoring | Beta | Metrics and monitoring for transaction performance |

## Project Structure

- **src/**: Core implementation files
  - **transaction.c**: Main transaction system implementation
  - **transaction_log.c**: Transaction logging for recovery and auditing
  - **transaction_visualization.c**: Transaction visualization tools
  - **lock_manager.c**: Document-level locking for concurrency control
  - **transaction_retry.c**: Logic for retrying failed transactions
  - **database.c**: Core database functionality
  - **server.c**: HTTP server implementation
  - **api.c**: API endpoints
  - **tools/**: Utility tools
    - **jsondb_metrics.c**: Metrics collection
    - **jsondb_tools.c**: Database administration tools

- **include/**: Header files
  - **transaction.h**: Transaction system interface
  - **database.h**: Database interface
  - **server.h**: Server configuration and setup
  - **api.h**: API endpoints and handlers
  - **utils/**: Utility headers
    - **metrics.h**: Metrics collection interfaces
    - **cache.h**: Document caching interfaces

- **admin/**: Web administration interface
  - **index.html**: Main admin interface
  - **login.html**: Authentication page
  - **js/app.js**: Front-end logic
  - **css/styles.css**: Interface styling

## Recent Development

Recent commits have focused on:
1. Refactoring JSON string to HTTP response conversion and centralizing helper functions
2. Implementing a comprehensive memory management system with reference counting
3. Reorganizing source code for better maintainability
4. Adding a comprehensive transaction management system
5. Implementing transaction visualization and enhanced audit trail capabilities
6. Building a transaction logging system for crash recovery
7. Improving workflow documentation in CONTRIBUTING.md
8. Integrating QuickJS for JavaScript extensions

## QuickJS Integration

The database now integrates with QuickJS, a small and embeddable JavaScript engine, to provide JavaScript extension capabilities. This integration allows for:

1. **Custom JavaScript Functions**: Define and execute custom JavaScript functions for data validation, transformation, and business logic
2. **JavaScript Extension API**: A C API that exposes database functionality to JavaScript code
3. **Memory-Safe Execution**: Proper memory management to prevent leaks and crashes
4. **Error Handling**: Robust error handling for JavaScript execution failures
5. **Automatic Setup**: Automatic download and installation of QuickJS if not present

Implementation details:
- QuickJS library is automatically downloaded and installed if needed via `scripts/setup_quickjs.sh`
- The system first checks for QuickJS in `/opt/qjs` and uses it if available
- If QuickJS is not found, the script downloads, compiles, and installs it
- JavaScript engine initialization and cleanup in `js_engine.c`
- JavaScript API functions exposed in `js_api.c`
- Global JavaScript engine instance managed in `main.c`
- Fallback mock implementation when QuickJS is unavailable using `-DUSE_QUICKJS_MOCK` flag

## Development Status

The core database functionality is stable and production-ready. The transaction system is fully implemented with varying levels of maturity for different components:

- **Stable Components**: Core transaction functionality, transaction logging, transaction isolation, and transaction retry mechanisms
- **Beta Components**: Deadlock resolution, transaction audit trail, transaction savepoints, and transaction monitoring
- **Alpha Components**: Transaction visualization tools

## Next Steps

1. ~~Fix build system file duplication issue to resolve duplicate function definitions~~ ✅ DONE
2. Continue improving JSON string handling throughout the codebase for consistency
3. Enhance transaction visualization capabilities
4. Improve deadlock detection and resolution
5. Optimize performance for high-concurrency operations
6. Add distributed transaction support
7. Expand monitoring and metrics for transactions
8. Extend JavaScript API with additional database functions

## Dependencies

- TCC (Tiny C Compiler) >= 0.9.27
- libuuid >= 2.36
- libssl >= 1.1.1
- libcrypto >= 1.1.1
- QuickJS >= 2021-03-27

## Memory Management System

The project now includes a comprehensive memory management system to prevent memory leaks and double-free errors:

1. **Reference Counting**: Core technique for tracking object ownership
   - Implemented in `ref_counter.h/c` to manage shared object lifetimes
   - JSON-specific reference counting in `ref_json.h/c`
   - Debug utilities in `debug.h` for conditional debugging output

2. **HTTP Response Helpers**: Utilities for JSON string handling
   - Created `json_helpers.h` for consistent JSON string to HTTP response conversion
   - Implemented `http_response_json_string` and `http_response_from_json_string` helper functions
   - Centralized helper functions by moving them from transaction.c to json_helpers.h for better code reuse
   - Fixed type mismatch issues in transaction components

This memory management system ensures consistent resource handling and prevents common memory-related issues in C programming.

## Build System and Server Initialization

The build system has been improved to handle the ongoing source code reorganization:

1. **Fixed File Duplication**: Resolved the issue where files in both `src/` and subdirectories were being compiled, causing duplicate function definitions
   - Updated Makefile to filter out duplicate files using `filter-out` function
   - Added special handling for main.c variants to ensure only one is used
   - Created `main_fixed.c` to fix server initialization issues

2. **Server Initialization Fix**: Resolved the server initialization and startup issues
   - Fixed return value checking in `server_init()` and `server_start()` to correctly handle status codes
   - Added proper initialization of the max_connections field
   - Ensured error field is properly initialized
   - Corrected enum handling where SERVER_OK (0) was being checked incorrectly

3. **QuickJS Integration**: Enhanced build system with automatic QuickJS detection and setup
   - Added script to download and install QuickJS when not available
   - Implemented conditional compilation for QuickJS mock implementation

The server now builds successfully and runs without errors, listening on port 5000 for incoming connections.

## Server Usage

To build and run the JSON Database Server:

```bash
# Clean and build the project
make clean
make

# Run the server
./bin/jsondb
```

The server will:
- Start and listen on port 5000 (default)
- Initialize the database from `db.json` (creates if not exists)
- Load RBAC configuration from `rbac.json` (creates if not exists)
- Skip JavaScript engine initialization (for now)
- Log any errors during startup

Access the server through:
- REST API: http://localhost:5000/api/...
- Admin interface: http://localhost:5000/admin

For advanced usage, check the command-line options:
- `-js <file.js>`: Run a JavaScript file and exit (useful for maintenance scripts)

## Project Status Management

When making changes to the project, please follow these guidelines:

1. **Always update `project-status.json`**: This file is the definitive source of truth for the project status. Any changes to feature status, new components, or roadmap items should be reflected here first.

2. **Keep this overview document (`claude.md`) in sync**: After updating `project-status.json`, make sure this document reflects the high-level changes. This document serves as a quick reference, while `project-status.json` contains the comprehensive details.

3. **Update for AI assistants**: Both this document and `project-status.json` are designed to help AI assistants (like Claude) understand the project structure and status. Keeping them updated helps ensure accurate assistance.

4. **Documentation-code consistency**: When implementing new features or modifying existing ones, ensure that both the code and documentation (including status files) remain consistent.