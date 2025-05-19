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

- `include/`: Header files
  - Mirrors the structure of the components directory

## Building

Use the Makefile in this directory to build the project:

```bash
make
```

## Documentation

For more detailed documentation about the codebase, please refer to the `/docs` directory.