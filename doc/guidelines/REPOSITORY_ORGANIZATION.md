# JSONdb Repository Organization Guidelines

This document outlines the guidelines for organizing the JSONdb repository to maintain a clean, maintainable, and efficient project structure.

## Core Principles

1. **Clarity**: Directory and file names should clearly indicate their purpose
2. **Consistency**: Similar files should be organized in a consistent manner
3. **Separation of Concerns**: Different aspects of the project should be organized separately
4. **Minimalism**: Only include necessary files; avoid duplicates and obsolete files
5. **Discoverability**: Important files should be easy to find
6. **Automation-friendly**: Organization should support automated builds and testing

## Directory Structure

The repository should follow this top-level structure:

```
jsondb/
├── LICENSE           # License file
├── README.md         # Project overview, quickstart guide
├── CONTRIBUTING.md   # Contribution guidelines
├── Makefile          # Main Makefile
├── src/              # Source code
├── include/          # Public header files
├── lib/              # External libraries
├── doc/              # Documentation
├── examples/         # Example code and usage
├── tools/            # Development and maintenance tools
├── scripts/          # Utility scripts
├── tests/            # Test suite
└── .github/          # GitHub-specific files (if applicable)
```

### Source Code (`src/`)

Source code should be organized into logical components:

```
src/
├── Makefile          # Source-specific Makefile
├── core/             # Core database functionality
├── api/              # API implementation
├── js/               # JavaScript engine integration
├── query/            # Query language implementation
├── transaction/      # Transaction handling
├── utils/            # Utility functions
├── database/         # Database operations
└── rbac/             # Role-based access control
```

### Include Files (`include/`)

Public header files should mirror the organization of `src/`:

```
include/
├── jsondb/           # Main public API headers
├── core/             # Core declarations
├── js/               # JavaScript engine declarations
├── query/            # Query language declarations
├── transaction/      # Transaction declarations
├── utils/            # Utility declarations
├── database/         # Database declarations
└── rbac/             # RBAC declarations
```

### Documentation (`doc/`)

Documentation should be well-organized by topic:

```
doc/
├── api/              # API documentation
├── guides/           # User guides
│   ├── getting-started.md
│   ├── js-integration.md
│   └── ...
├── architecture/     # Architectural documentation
│   ├── overview.md
│   ├── components.md
│   └── ...
├── development/      # Developer documentation
│   ├── building.md
│   ├── testing.md
│   └── ...
└── reference/        # Reference documentation
    ├── configuration.md
    ├── query-language.md
    └── ...
```

### Examples (`examples/`)

Examples should be organized by type and complexity:

```
examples/
├── basic/            # Basic usage examples
├── advanced/         # Advanced usage examples
├── js-extensions/    # JavaScript extension examples
└── client/           # Client implementation examples
```

### Tests (`tests/`)

Tests should be organized to mirror the source structure:

```
tests/
├── Makefile          # Test-specific Makefile
├── unit/             # Unit tests
├── integration/      # Integration tests
├── performance/      # Performance tests
├── js/               # JavaScript-specific tests
└── data/             # Test data
```

## File Naming Conventions

1. Source files should use descriptive names in snake_case
2. Header files should match their corresponding source files
3. Documentation files should use kebab-case for readability
4. Test files should clearly indicate what they test

## Git Workflow Guidelines

1. **Branch naming**: Use `feature/name`, `bugfix/name`, or `docs/name` for branches
2. **Commit messages**: Follow the format `type(scope): description` (e.g., `feat(js): add JavaScript file validation`)
3. **Commit frequency**: Make small, focused commits that address a single concern
4. **Clean history**: Rebase feature branches before merging to maintain a clean history
5. **No build artifacts**: Never commit build artifacts, object files, or binaries

## Build System Guidelines

1. The main `Makefile` should provide common targets (build, test, clean)
2. Component-specific `Makefile`s should be available in subdirectories
3. Build targets should be consistent across components
4. All build files should be generated in a separate `build/` directory
5. Use conditional compilation for optional features like JavaScript support

## Documentation Guidelines

1. Keep documentation up-to-date with code changes
2. Use Markdown for all documentation
3. Include examples in documentation
4. Document all public APIs thoroughly
5. Maintain separate documents for different concerns (e.g., API, architecture)

## Testing Guidelines

1. Write tests for all new features
2. Maintain both unit and integration tests
3. Organize tests to mirror the source structure
4. Automate test execution through the build system
5. Include performance tests for critical operations

## Repository Maintenance

1. **Regular cleanup**: Periodically review and clean up the repository
2. **Obsolete file removal**: Remove obsolete files rather than just commenting them out
3. **Dependency management**: Clearly document and maintain external dependencies
4. **Versioning**: Use semantic versioning for releases
5. **Changelog**: Maintain a detailed changelog for each release

By following these guidelines, the JSONdb project will maintain a clean, organized, and efficient repository structure that enhances developer productivity and project maintainability.