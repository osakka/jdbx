# Repository Reorganization Plan

Based on our repository organization guidelines and the GIT_GUIDELINES.md document, this plan outlines the specific steps to restructure the JSONdb repository for better maintainability and adherence to best practices.

## Current Implementation State

Git Describe: *Will be populated during implementation*
Last Updated: 2025-05-12

## Core Principles from GIT_GUIDELINES.md

We will adhere to these principles from the GIT_GUIDELINES.md:

1. **State Tracking**: Use Git describe for implementation state tracking
2. **Tagging Strategy**: Follow the prescribed tagging format and create milestone tags
3. **Commit Message Standards**: Use the standardized format `<type>(<scope>): <subject>`
4. **Clean Tabletop Policy**: Work directly in the codebase, avoid temporary files
5. **Implementation Progress Tracking**: Maintain clear status information

## Directory Structure Changes

### Top-Level Restructuring

1. Move all source code to follow the `/src` structure in our guidelines
2. Consolidate all documentation under `/doc` (rename from `docs`)
3. Relocate test files to a properly structured `/tests` directory
4. Move examples to `/examples` with clear categorization
5. Consolidate Makefiles appropriately
6. Place all include files in `/include`

### Source Reorganization

The source code will be reorganized as follows:

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

### Documentation Reorganization

All documentation will be moved to the standard `doc` directory:

```
doc/
├── api/              # API documentation
├── guidelines/       # Project guidelines including Git
├── architecture/     # Architecture documentation
├── development/      # Developer documentation
└── reference/        # Reference material
```

The GIT_GUIDELINES.md will be placed in `doc/guidelines/` as it provides crucial workflow direction.

## File Cleanup

1. Remove all `.o` files and build artifacts
2. Remove temporary or backup files (`.bak`, `.old`)
3. Remove duplicate files from js_integration_export
4. Consolidate or remove redundant test files

## Makefile Structure

1. Create a main Makefile at the top level
2. Create component-specific Makefiles in appropriate subdirectories
3. Ensure consistent build targets across all Makefiles

## Implementation Plan

The reorganization will be implemented in a single commit to maintain a clean history:

1. Create a reorganization branch
2. Implement all directory restructuring
3. Clean up unnecessary files
4. Update references and paths in source files
5. Test the build system
6. Create a comprehensive commit
7. Tag this state as a new milestone (v1.0.0-reorganization)
8. Merge back to mainline

## New Repository Structure

After reorganization, the repository will follow this structure:

```
jsondb/
├── LICENSE           # License file
├── README.md         # Project overview
├── CONTRIBUTING.md   # Contribution guidelines
├── Makefile          # Main Makefile
├── include/          # Public header files
│   └── jsondb/       # Main public API headers
├── src/              # Source code (properly organized)
├── doc/              # Documentation (renamed from docs)
├── examples/         # Example code and usage
├── tools/            # Development and maintenance tools
├── scripts/          # Utility scripts
└── tests/            # Test suite
```

## Status Tracking

We will create an IMPLEMENTATION_STATUS.md document in the doc directory that follows the format described in the GIT_GUIDELINES.md, including:

- Current implementation state (git describe)
- Last updated date
- Recent changes
- Component status
- Next steps

## Risk Assessment

- **Build breakage**: Ensure all file references are updated correctly
- **Path dependencies**: Update any hardcoded paths in source code
- **Test failures**: Verify tests run correctly after reorganization

## Post-Reorganization Steps

1. Update documentation to reflect new structure
2. Configure Git hooks for state tracking
3. Update any CI/CD processes to match the new structure
4. Create a new tag to mark the reorganized state