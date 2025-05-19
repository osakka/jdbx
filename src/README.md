# JSONdb Source Code

This directory contains the source code for the JSONdb project.

## Structure

- `components/`: Contains the main components of the system
  - `api/`: API endpoints and request handling
  - `core/`: Core server functionality including the thread pool
  - `database/`: Database operations and storage
  - `js/`: JavaScript engine integration
  - `query/`: Query language implementation
  - `rbac/`: Role-based access control
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
make
```

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