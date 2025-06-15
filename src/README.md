# JDBX Source Code

**Version**: 5.0.0  
**Last Updated**: June 15, 2025

This directory contains the source code for the JDBX project with enterprise-grade security, zero-warning build quality, and comprehensive architectural excellence.

## Structure

- `components/`: Contains the main components of the system
  - `api/`: API endpoints and request handling
  - `binary/`: Binary serialization format for persistence
  - `core/`: Core server functionality including the thread pool
  - `database/`: Database operations, storage, adaptive indexing (v3.1.0), and performance tracking
  - `index/`: B+tree and hash index implementations
  - `js/`: JavaScript engine integration
  - `lockfree/`: Lock-free data structures (skiplist, hazard pointers)
  - `query/`: Query language implementation
  - `rbac/`: Role-based access control with database-backed storage and UUID support
  - `storage/`: Memory-mapped storage implementation
  - `tools/`: Command-line tools and utilities
  - `transaction/`: Transaction management
  - `utils/`: Utility functions and helpers

- `initialize/`: Contains component initialization modules
  - Each file corresponds to initializing a specific subsystem
  - Provides clear separation between component implementation and initialization
  - Follows the initialization sequence in main.c (config → logger → database → etc.)

- `include/`: Header files
  - Mirrors the structure of the components directory

## Building

Use the Makefile in this directory to build the project:

```bash
# Standard build with JavaScript support
make

# Build without JavaScript support
make js-disabled

# Build with debug symbols
make debug

# Build optimized release version
make release

# Clean build artifacts
make clean
```

The build system ensures:
- Zero warnings with -Wall -Wextra
- Proper dependency tracking
- Automatic directory creation
- Support for different build configurations

## Documentation

For more detailed documentation about the codebase, please refer to the `/docs` directory.

## Initialization Sequence

The server follows a specific initialization sequence, implemented through the modules in the `initialize/` directory:

1. **Config** (`config.c`): Parses command-line arguments and loads configuration
2. **Logger** (`logger.c`): Sets up the logging system
3. **Database** (`database.c`): Initializes the database and indices
4. **RBAC** (`rbac.c`): Sets up role-based access control
5. **API** (`api.c`): Initializes API context and registers routes
6. **Daemon** (`daemon.c`): Handles daemonization (if in daemon mode)
7. **Socket** (`socket.c`): Creates and binds the server socket
8. **Threads** (`threads.c`): Initializes the thread pool

This sequence ensures proper dependency handling and resolves issues like socket binding in daemon mode.

## Key Features

1. **Binary Persistence**: Automatic saves to .jdb format with CRC32 integrity
2. **Session Management**: Comprehensive tracking with IP/UA and termination support
3. **Time-Series Metrics**: Fixed document pattern with O(1) updates
4. **Thread Pool**: Efficient request handling with configurable workers
5. **RBAC System**: Database-backed with multiple implementation layers
6. **Adaptive Indexing**: Automatic index creation based on query patterns
7. **Index Maintenance**: Automatic index updates on data modifications
8. **Index Metrics**: Performance tracking with ROI and effectiveness analysis
9. **Index Cleanup**: Automatic removal of underperforming indexes with configurable thresholds
10. **Connection Management**: Loop-based keep-alive handling with zero memory leaks
11. **Query Pattern Tracking**: Field path extraction and performance monitoring

## Code Standards

- All code must compile with zero warnings
- Use consistent logging format (no redundant prefixes)
- Follow single source of truth principle
- Document all changes thoroughly
- Test all modifications before committing