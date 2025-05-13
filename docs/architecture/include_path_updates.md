# Include Path Updates for Reorganized Source Structure

After completing the source code reorganization, the include paths in source files have been updated to reflect the new directory structure. This document provides guidelines for using the correct include paths in the codebase.

## Current Include Hierarchy

```
src/
├── include/           # Global header files
│   ├── api/           # API headers
│   ├── core/          # Core server headers
│   ├── database/      # Database engine headers
│   ├── js/            # JavaScript integration headers
│   ├── query/         # Query language headers 
│   ├── rbac/          # RBAC headers
│   ├── transaction/   # Transaction headers
│   └── utils/         # Utility headers
│       └── memory/    # Memory management headers
│
└── components/        # Component implementation with their headers
    ├── api/           # API implementation
    ├── core/          # Core server implementation
    ├── database/      # Database engine implementation
    ├── js/            # JavaScript integration implementation
    ├── query/         # Query language implementation
    ├── rbac/          # RBAC implementation
    ├── transaction/   # Transaction implementation
    └── utils/         # Utility implementation
        └── memory/    # Memory management implementation
```

## Include Path Conventions

### Component Implementation Including Headers

When including header files in source code, always use the `src/include/` path:

```c
// In src/components/transaction/transaction.c including transaction.h
#include "src/include/transaction/transaction.h"

// In src/components/api/admin_api.c including database.h
#include "src/include/database/database.h"
```

### Cross-Component Headers

When including headers from other components, always use the full path:

```c
// In src/components/api/transaction_api.c including transaction.h
#include "src/include/transaction/transaction.h"

// In src/components/js/js_api.c including database.h
#include "src/include/database/database.h"
```

## Common Header Paths

| Module | Header Include Path |
|--------|---------------------|
| API | `#include "src/include/api/api.h"` |
| Database | `#include "src/include/database/database.h"` |
| JavaScript | `#include "src/include/js/js_engine.h"` |
| JSON | `#include "src/include/utils/json.h"` |
| Server | `#include "src/include/core/server.h"` |
| Transaction | `#include "src/include/transaction/transaction.h"` |
| Query | `#include "src/include/query/query_language.h"` |
| RBAC | `#include "src/include/rbac/rbac.h"` |
| JWT | `#include "src/include/rbac/jwt.h"` |
| Metrics | `#include "src/include/utils/metrics.h"` |
| Reference Counter | `#include "src/include/utils/memory/ref_counter.h"` |

## Main Entry Point

For the main entry point, use the unified jsondb.h header:

```c
#include "src/include/jsondb.h"
```

## External Libraries

External libraries should be included with angle brackets or as recommended by the library:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

For QuickJS, use the path:

```c
#include "src/include/js/quickjs.h"
#include "src/include/js/quickjs-libc.h"
```

## Include Migration Script

A migration script (`scripts/maintenance/fix_includes_final.sh`) has been created to automatically update include paths across the codebase. This script:

1. Updates `include/jsondb/` references to use `src/include/`
2. Updates relative paths using `../include/` to use `src/include/`
3. Updates the basic `jsondb.h` inclusion to `src/include/jsondb.h`
4. Updates QuickJS header references
5. Standardizes component references

To run the script:

```bash
cd /path/to/project/root
./scripts/maintenance/fix_includes_final.sh
```

## Build System

The Makefile has been updated to use only `src/include` and `src/components` directories. The compiler flags now include:

```
CFLAGS += -I./src
```

This allows the include statements to work correctly with the new structure.

## Example Code

### Before Reorganization

```c
#include <stdio.h>
#include <stdlib.h>

#include "../include/api.h"
#include "../include/database.h"
#include "../include/json.h"
#include "../include/transaction.h"
#include "../include/utils/memory/ref_counter.h"
```

### After Reorganization

```c
#include <stdio.h>
#include <stdlib.h>

#include "src/include/api/api.h"
#include "src/include/database/database.h"
#include "src/include/utils/json.h"
#include "src/include/transaction/transaction.h"
#include "src/include/utils/memory/ref_counter.h"
```

## Testing Include Paths

To verify your include paths are correct, you can run the following commands:

```bash
# Compile with verbose output
make -v

# List all includes in a specific file
gcc -E -I./src your_file.c | grep "#include"
```

If you encounter any "file not found" errors, check your include paths against this documentation.