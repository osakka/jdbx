# JSONdb Project Structure

This document provides an overview of the JSONdb project structure after the repository reorganization (v1.0.0-structure).

## Overview

The repository has been reorganized following the principles outlined in the [Repository Organization Guidelines](../guidelines/REPOSITORY_ORGANIZATION.md) to create a more maintainable and consistent project structure.

## Directory Structure

```
jsondb/
├── Makefile                # Main build system
├── README.md               # Project overview and documentation
├── CONTRIBUTING.md         # Contribution guidelines
├── bin/                    # Binary output directory
├── src/                    # Source code
│   ├── api/                # API implementation
│   ├── components/         # Component implementations
│   ├── core/               # Core server functionality
│   ├── database/           # Database engine
│   ├── js/                 # JavaScript integration
│   ├── query/              # Query language implementation
│   ├── rbac/               # Role-based access control
│   ├── tools/              # Server tools
│   ├── transaction/        # Transaction management
│   └── utils/              # Utility functions
├── include/                # Header files
│   ├── jsondb.h            # Main include file
│   └── jsondb/             # Sub-component headers
│       ├── api/            # API headers
│       ├── core/           # Core headers
│       ├── database/       # Database headers
│       ├── js/             # JavaScript headers
│       ├── query/          # Query language headers
│       ├── rbac/           # RBAC headers
│       ├── transaction/    # Transaction headers
│       └── utils/          # Utility headers
├── doc/                    # Documentation
│   ├── api/                # API documentation
│   ├── architecture/       # Architecture documentation
│   ├── development/        # Development documentation
│   ├── guidelines/         # Project guidelines
│   └── reference/          # Reference documentation
├── examples/               # Example code
│   ├── basic/              # Basic examples
│   ├── advanced/           # Advanced examples
│   ├── js-extensions/      # JavaScript extension examples
│   └── client/             # Client implementation examples
├── tests/                  # Test suite
│   ├── unit/               # Unit tests
│   ├── integration/        # Integration tests
│   ├── performance/        # Performance tests
│   └── js/                 # JavaScript-specific tests
├── scripts/                # Utility scripts
├── share/                  # Shared resources
│   ├── helpers/            # Helper scripts
│   ├── htdocs/             # Web interface files
│   └── man/                # Manual pages
└── var/                    # Variable data (during runtime)
    ├── data/               # Database files
    ├── log/                # Log files
    └── run/                # Runtime files (PIDs, sockets)
```

## Key Files

- `include/jsondb.h`: Main include file for the project
- `src/core/main.c`: Entry point for the server
- `src/js/js_engine.c`: JavaScript engine integration
- `Makefile`: Main build system

## Build System

The project uses a hierarchical build system with the following structure:

1. Top-level `Makefile`: Coordinates the overall build
2. Component-specific Makefiles: Handle building specific components

Key make targets:

- `make all`: Build the entire project
- `make clean`: Clean build artifacts
- `make test`: Run the test suite
- `make install`: Install the software
- `make uninstall`: Uninstall the software

## Documentation Organization

The documentation has been reorganized into logical sections:

- `doc/api/`: API documentation
- `doc/architecture/`: Architectural documentation
- `doc/development/`: Developer documentation
- `doc/guidelines/`: Project guidelines
- `doc/reference/`: Reference documentation

## Testing Organization

Tests have been reorganized to mirror the source structure:

- `tests/unit/`: Unit tests for individual components
- `tests/integration/`: Integration tests for system interactions
- `tests/performance/`: Performance and benchmarking tests
- `tests/js/`: JavaScript-specific tests

## Example Organization

Examples are now organized by complexity and purpose:

- `examples/basic/`: Basic usage examples
- `examples/advanced/`: Advanced usage examples
- `examples/js-extensions/`: JavaScript extension examples
- `examples/client/`: Client implementation examples

## Best Practices

When working with this codebase, follow these best practices:

1. Respect the directory structure when adding new files
2. Follow the established naming conventions
3. Update documentation when making significant changes
4. Add tests for new functionality
5. Use the appropriate Makefile for building components

## Related Documentation

- [Repository Organization Guidelines](../guidelines/REPOSITORY_ORGANIZATION.md)
- [Git Guidelines](../guidelines/GIT_GUIDELINES.md)
- [Implementation Status](../IMPLEMENTATION_STATUS.md)