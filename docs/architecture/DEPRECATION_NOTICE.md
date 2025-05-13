# DEPRECATION NOTICE

## Source File Deprecation

As of May 13, 2025, all source files directly under the `src/` directory have been officially deprecated and relocated to the `src/components/` directory as part of the source code consolidation effort.

### Deprecated Locations

The following source file locations are now deprecated:

- `src/api/*.c` - Moved to `src/components/api/*.c`
- `src/core/*.c` - Moved to `src/components/core/*.c`
- `src/database/*.c` - Moved to `src/components/database/*.c`
- `src/js/*.c` - Moved to `src/components/js/*.c`
- `src/query/*.c` - Moved to `src/components/query/*.c`
- `src/rbac/*.c` - Moved to `src/components/rbac/*.c`
- `src/tools/*.c` - Moved to `src/components/tools/*.c`
- `src/transaction/*.c` - Moved to `src/components/transaction/*.c`
- `src/utils/*.c` - Moved to `src/components/utils/*.c`
- `src/main.c` - Moved to `src/components/main.c`

### New File Organization

The new file organization follows these principles:

1. **Headers** are located in `src/include/`, organized by component
2. **Implementation** files are located in `src/components/`, organized by component
3. **Single source of truth** is maintained for each file

### Developer Guidelines

Developers should:

1. Always add new implementation files to `src/components/`
2. Always add new header files to `src/include/`
3. Use the include paths relative to `src/include` (e.g., `#include "database/database.h"`)
4. Never create implementation files directly under `src/`
5. Refer to updated project documentation in `docs/architecture/project_structure.md`

### Migration Complete

All source files have been migrated, and the build system has been updated to reference the new locations. The project should be built using the main Makefile at `src/Makefile`.

The deprecated directories have been removed from the repository to prevent confusion and maintain a clean codebase.