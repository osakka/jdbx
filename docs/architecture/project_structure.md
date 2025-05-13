# JSONdb Project Structure

This document provides an overview of the JSONdb project structure after the repository reorganization (v1.0.0-structure).

## Overview

The repository has been reorganized following the principles outlined in the [Repository Organization Guidelines](../guidelines/REPOSITORY_ORGANIZATION.md) to create a more maintainable and consistent project structure.

## Directory Structure

```
jsondb/
├── README.md               # Project overview and documentation
├── CONTRIBUTING.md         # Contribution guidelines
├── build/                  # Build output directory
│   ├── bin/                # Executable binaries
│   ├── lib/                # Library files
│   └── obj/                # Object files
├── src/                    # Source code
│   ├── Makefile            # Main build system
│   ├── include/            # Global header files
│   │   └── jsondb.h        # Main include file
│   └── components/         # Component-based organization
│       ├── main.c          # Main entry point
│       ├── api/            # API implementation with headers
│       │   ├── api.h       # API header files
│       │   └── *.c         # API implementation files
│       ├── core/           # Core server functionality with headers
│       │   ├── server.h    # Core header files
│       │   └── *.c         # Core implementation files
│       ├── database/       # Database engine with headers
│       │   ├── database.h  # Database header files
│       │   └── *.c         # Database implementation files
│       ├── js/             # JavaScript integration with headers
│       │   ├── js_api.h    # JavaScript header files
│       │   └── *.c         # JavaScript implementation files
│       ├── query/          # Query language implementation with headers
│       │   ├── query_language.h # Query header files
│       │   └── *.c         # Query implementation files
│       ├── rbac/           # Role-based access control with headers
│       │   ├── rbac.h      # RBAC header files
│       │   └── *.c         # RBAC implementation files
│       ├── tools/          # Server tools with headers
│       │   └── *.c         # Tools implementation files
│       ├── transaction/    # Transaction management with headers
│       │   ├── transaction.h # Transaction header files
│       │   └── *.c         # Transaction implementation files
│       └── utils/          # Utility functions with headers
│           ├── *.h         # Utility header files
│           ├── *.c         # Utility implementation files
│           └── memory/     # Memory management utilities
├── docs/                   # Documentation
│   ├── api/                # API documentation
│   ├── architecture/       # Architecture documentation
│   ├── development/        # Development documentation
│   ├── guidelines/         # Project guidelines
│   ├── guides/             # User guides
│   ├── integration/        # Integration documentation
│   ├── reference/          # Reference documentation
│   └── status/             # Project status tracking
├── tests/                  # Test suite
│   ├── unit/               # Unit tests
│   ├── integration/        # Integration tests
│   ├── performance/        # Performance tests
│   ├── security/           # Security tests
│   └── js/                 # JavaScript-specific tests
├── scripts/                # Utility scripts
├── share/                  # Shared resources
│   ├── examples/           # Example code
│   │   ├── basic/          # Basic examples
│   │   ├── advanced/       # Advanced examples
│   │   ├── js_extensions/  # JavaScript extension examples
│   │   └── client/         # Client implementation examples
│   ├── helpers/            # Helper scripts
│   ├── htdocs/             # Web interface files (admin UI)
│   ├── js/                 # JavaScript resources
│   │   ├── functions/      # JavaScript functions
│   │   ├── transforms/     # Document transformers
│   │   └── validators/     # Document validators
│   └── man/                # Manual pages
└── data/                   # Variable data (during runtime)
    ├── jsondb/             # Database files
    ├── log/                # Log files
    └── run/                # Runtime files (PIDs, sockets)
```

## Key Files

- `src/include/jsondb.h`: Main include file for the project
- `src/components/main.c`: Entry point for the server
- `src/components/js/js_engine.c`: JavaScript engine integration
- `src/Makefile`: Main build system

## Component Organization

The project uses a component-based organization where each component contains both its headers and implementation. This approach provides several benefits:

1. **Cohesion**: Headers are kept with their implementation, making it easier to understand the component.
2. **Encapsulation**: Components are self-contained, reducing unnecessary dependencies.
3. **Discoverability**: Developers can quickly find all files related to a component in one location.
4. **Maintenance**: Changes to a component can be made in one place, reducing the risk of inconsistencies.

Components are located in `src/components/` with the global header `jsondb.h` in `src/include/`.

## Build System

The project uses a hierarchical build system with the following structure:

1. Main `src/Makefile`: Coordinates the overall build
2. Component-specific Makefiles: Handle building specific components

Key make targets (executed from the src/ directory):

- `make all`: Build the entire project
- `make clean`: Clean build artifacts
- `make test`: Run the test suite
- `make install`: Install the software
- `make uninstall`: Uninstall the software

## Documentation Organization

The documentation has been reorganized into logical sections:

- `docs/api/`: API documentation
- `docs/architecture/`: Architectural documentation
- `docs/development/`: Developer documentation
- `docs/guidelines/`: Project guidelines
- `docs/guides/`: User guides
- `docs/integration/`: Integration documentation
- `docs/reference/`: Reference documentation
- `docs/status/`: Project status tracking

## Testing Organization

Tests have been reorganized to mirror the source structure:

- `tests/unit/`: Unit tests for individual components
- `tests/integration/`: Integration tests for system interactions
- `tests/performance/`: Performance and benchmarking tests
- `tests/js/`: JavaScript-specific tests

## Example Organization

Examples are now organized by complexity and purpose:

- `share/examples/basic/`: Basic usage examples
- `share/examples/advanced/`: Advanced usage examples
- `share/examples/js_extensions/`: JavaScript extension examples
- `share/examples/client/`: Client implementation examples

## Best Practices

When working with this codebase, follow these best practices:

1. Respect the component-based directory structure when adding new files
2. Add new headers to the appropriate component directory, not to a separate include hierarchy
3. Include component headers using `"components/component_name/header.h"` pattern
4. Follow the established naming conventions
5. Update documentation when making significant changes
6. Add tests for new functionality that mirror the component structure
7. Use the appropriate Makefile for building components

## Related Documentation

- [Repository Organization Guidelines](../guidelines/REPOSITORY_ORGANIZATION.md)
- [Git Guidelines](../guidelines/GIT_GUIDELINES.md)
- [Implementation Status](../status/IMPLEMENTATION_STATUS.md)