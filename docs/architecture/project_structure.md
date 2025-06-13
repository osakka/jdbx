# JDBX Project Structure

This document provides an overview of the JDBX project structure after the repository reorganization and consolidation (v1.1.0-structure).

## Overview

The repository has been reorganized following the principles outlined in the [Repository Organization Guidelines](../guidelines/REPOSITORY_ORGANIZATION.md) to create a more maintainable and consistent project structure. Additionally, all source files have been consolidated to eliminate redundancy and provide a single source of truth.

## Directory Structure

```
jdbx/
├── README.md               # Project overview and documentation
├── CONTRIBUTING.md         # Contribution guidelines
├── build/                  # Build output directory
│   ├── bin/                # Executable binaries
│   ├── lib/                # Library files
│   └── obj/                # Object files
├── src/                    # Source code
│   ├── Makefile            # Main build system
│   ├── include/            # Global header files - include path for all components
│   │   ├── jdbx.h        # Main include file
│   │   ├── api/            # API header files
│   │   │   └── *.h         # API-specific headers
│   │   ├── core/           # Core header files
│   │   │   └── *.h         # Core-specific headers
│   │   ├── database/       # Database header files
│   │   │   └── *.h         # Database-specific headers
│   │   ├── js/             # JavaScript header files
│   │   │   └── *.h         # JavaScript-specific headers
│   │   ├── query/          # Query header files
│   │   │   └── *.h         # Query-specific headers
│   │   ├── rbac/           # RBAC header files
│   │   │   └── *.h         # RBAC-specific headers
│   │   ├── transaction/    # Transaction header files
│   │   │   └── *.h         # Transaction-specific headers
│   │   └── utils/          # Utility header files
│   │       ├── *.h         # Utility-specific headers
│   │       └── memory/     # Memory management utility headers
│   └── components/         # Component-based organization with all implementation files 
│       ├── main.c          # Main entry point
│       ├── api/            # API implementation
│       │   └── *.c         # API implementation files
│       ├── core/           # Core server functionality
│       │   └── *.c         # Core implementation files
│       ├── database/       # Database engine implementation
│       │   └── *.c         # Database implementation files
│       ├── js/             # JavaScript integration implementation
│       │   └── *.c         # JavaScript implementation files
│       ├── query/          # Query language implementation
│       │   └── *.c         # Query implementation files
│       ├── rbac/           # Role-based access control implementation
│       │   └── *.c         # RBAC implementation files
│       ├── tools/          # Server tools implementation
│       │   └── *.c         # Tools implementation files
│       ├── transaction/    # Transaction management implementation
│       │   └── *.c         # Transaction implementation files
│       └── utils/          # Utility functions implementation
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
    ├── jdbx/             # Database files
    ├── log/                # Log files
    └── run/                # Runtime files (PIDs, sockets)
```

## Key Files

- `src/include/jdbx.h`: Main include file for the project
- `src/components/main.c`: Entry point for the server
- `src/components/js/js_engine.c`: JavaScript engine integration
- `src/Makefile`: Main build system

## Component Organization

The project uses a component-based organization with a clean separation between headers and implementation. This approach provides several benefits:

1. **Separation of Concerns**: Headers (interfaces) are kept separate from implementation, improving encapsulation.
2. **Clean Include Paths**: All headers are accessed via the unified `src/include` directory.
3. **Single Source of Truth**: All implementation files are consolidated in `src/components/`, eliminating redundancy.
4. **Discoverability**: Developers can quickly find all related files in a consistent, predictable structure.
5. **Maintenance**: Changes to a component can be made in one place, reducing the risk of inconsistencies.

Headers are located in `src/include/` (organized by component), while all implementation files are in `src/components/` (also organized by component). The main include file `jdbx.h` is located at `src/include/jdbx.h`.

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

1. Respect the separation between headers and implementation:
   - Headers go in `src/include/{component}/`
   - Implementation files go in `src/components/{component}/`
2. Place all implementation files in the `src/components/` directory; **do not** create implementation files in the `src/` root or its subdirectories
3. Include header files using the path relative to `src/include`, e.g., `#include "api/api.h"` or `#include "database/database.h"`
4. Follow the established naming conventions for consistency
5. Update documentation when making significant changes
6. Add tests for new functionality that mirror the component structure
7. Use the main Makefile in `src/` for building all components
8. Maintain a single source of truth by avoiding duplicate implementations

## Related Documentation

- [Repository Organization Guidelines](../guidelines/REPOSITORY_ORGANIZATION.md)
- [Git Guidelines](../guidelines/GIT_GUIDELINES.md)
- [Implementation Status](../status/IMPLEMENTATION_STATUS.md)
- [Component Organization](component_organization.md)
- [Code Organization](code_organization.md)
- [Include Path Updates](include_path_updates.md)