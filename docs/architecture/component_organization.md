# Source Code Consolidation Implementation

This document describes the current source code organization following the consolidation work that established a single source of truth for the codebase.

## Overview

The repository has a clean, component-based organization with a clear separation between header files and implementation files. This organization provides a consistent structure that makes the code more maintainable and reduces the risk of inconsistencies.

## Project Organization

The codebase follows this organization:

1. **Headers**: All header files are located in `src/include/`, organized by component.
2. **Implementation**: All implementation files are located in `src/components/`, also organized by component.
3. **JavaScript Resources**: JavaScript utility files are stored in `share/js/functions/`.
4. **Build System**: The main Makefile is in `src/Makefile` and handles the entire build process.

## Component Structure

The components are organized as follows:

1. **API**: API endpoints and request handlers (`src/components/api/`)
2. **Core**: Core server functionality (`src/components/core/`)
3. **Database**: Database engine implementation (`src/components/database/`)
4. **JavaScript**: JavaScript integration (`src/components/js/`)
5. **Query**: Query language processing (`src/components/query/`)
6. **RBAC**: Role-based access control (`src/components/rbac/`)
7. **Tools**: Server utilities (`src/components/tools/`)
8. **Transaction**: Transaction management (`src/components/transaction/`)
9. **Utils**: Utility functions (`src/components/utils/`)

## Organization Benefits

The current organization provides several benefits:

1. **Clear Responsibility**: Each component has a well-defined responsibility and scope.
2. **Maintainability**: With a single source of truth for each file, maintenance is simplified.
3. **Discoverability**: Developers can quickly find related files within the component structure.
4. **Consistency**: The uniform structure makes the code more consistent and predictable.

## Build Process

The build process is straightforward:

1. The main `src/Makefile` handles all builds.
2. Headers are included using their component-relative paths (e.g., `#include "database/database.h"`).
3. The build compiles all implementation files from `src/components/`.
4. The output is a single binary at `bin/jsondb_server`.

## Development Guidelines

When working with this codebase:

1. Place new header files in the appropriate component directory under `src/include/`.
2. Place new implementation files in the appropriate component directory under `src/components/`.
3. Use component-relative include paths (e.g., `#include "database/database.h"`).
4. Follow the established naming conventions for consistency.
5. Ensure that every header has only one corresponding implementation file.

## Related Documentation

- [Project Structure](project_structure.md): Detailed documentation on the project's directory structure.
- [Repository Organization Guidelines](../guidelines/REPOSITORY_ORGANIZATION.md): Guidelines for repository organization.
- [Include Path Updates](include_path_updates.md): Documentation on include path usage and conventions.
- [Code Organization](code_organization.md): Overview of the source code organization.