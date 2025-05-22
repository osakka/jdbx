# JSONdb Development Guidelines

## Core Principles

- Do not make minimal implementations. Delete partial concept files and ideas. Focus on:
  1. One source of truth
  2. One build (always result is bin/jsondb_server)
  3. One clear goal
  4. Build with full functionality
  5. Always fix, never regress
  6. Document thoroughly
  7. Repeat the improvement cycle
  8. One Makefile for the project src/Makefile
  9. One main.c for the project src/components/main.c
 10. Components are in src/components
 11. Headers are in src/include
 12. We have impeccable git hygiene!
 13. Guidelines are in docs/guidelines, read them
 14. Maintain zero-warning policy - always compile with -Wall -Wextra
 15. Use proper string handling to prevent buffer overflows
 16. Document all fixes thoroughly for future reference
 17. Never recreate parallel implementations, always integrate and test your fixes directly in the main code

## Build and Run Guidelines

1. ONLY BUILD USING THE MAKEFILE in src/ directory:
   ```
   cd /opt/jsondb/src && make
   ```

2. ALWAYS run the server in daemon mode, never in foreground mode:
   ```
   cd /opt/jsondb && build/jsondb_runtime.sh start
   ```

3. ALWAYS interact with the server using the runtime script:
   ```
   # Start server
   build/jsondb_runtime.sh start
   
   # Check status
   build/jsondb_runtime.sh status
   
   # Stop server
   build/jsondb_runtime.sh stop
   ```

4. NEVER run binaries from the build directory directly, only use the runtime script.

5. ALWAYS examine logs for debugging, never rely on stdout/stderr:
   ```
   cat /opt/jsondb/var/jsondb_server.log
   ```
   
6. The server runs on port 5000 by default. You can change this in the runtime script.

7. DO NOT IMPLEMENT MOCK DATA OR DEMO MODE - ALWAYS WORK WITH REAL SERVER DATA.

## Socket Binding Implementation

1. The socket binding implementation follows a specific sequence:
   - First initialize all resources (config, database, RBAC, API)
   - If in daemon mode, daemonize the process
   - Initialize the socket in the final daemon process
   - Initialize the thread pool
   - Run the server main loop

2. The socket is properly bound only in the final daemon process to ensure proper socket state.

3. Socket initialization includes:
   - Creating the socket
   - Setting socket options (SO_REUSEADDR)
   - Properly resolving the hostname
   - Binding the socket
   - Setting the socket to listen state
   - Verifying the socket is properly in listening state

## Modular Initialization System

The server now uses a modular initialization sequence with separate components:

- init_config: Initialize and parse configuration
- init_logger: Set up logging system
- init_database: Initialize database
- init_daemon: Handle daemonization if needed
- init_socket: Set up the server socket
- init_rbac: Initialize role-based access control
- init_api: Set up API routes and handlers
- init_threads: Initialize thread pool
- run_server: Run the main server loop

This modular approach ensures proper sequencing, better error handling, and clear separation of concerns.

## Binary Persistence System

The JSONdb server includes a complete binary persistence system with automatic data saves:

1. **Binary Format**: Database stored in efficient binary format (.jdb) with TLV encoding
2. **Automatic Saves**: Triggered by buffer thresholds (50 operations OR 1MB) and periodic saves (30s)
3. **Thread Safety**: Dedicated persistence thread with proper mutex/condition variable patterns
4. **Error Handling**: API errors on persistence failures with database rollback support
5. **Data Integrity**: CRC32 checksums and magic number verification
6. **Zero Deadlocks**: Fixed serialization locking to prevent thread deadlocks

### Key Implementation Files:
- `src/components/database/persistence.c` - Persistence thread and buffer management
- `src/components/binary/binary_format.c` - Binary serialization/deserialization  
- `src/components/database/simplified_db.c` - Database operations with persistence integration
- `src/include/database/database.h` - Persistence thread structure definitions

### Database File:
- Location: `/opt/jsondb/build/var/database.jdb`
- Format: Binary with magic number 0x4A534442 ("JSDB")
- Persistence: Automatic saves ensure data durability across server restarts