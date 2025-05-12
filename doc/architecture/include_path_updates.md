# Include Path Updates for Reorganized Source Structure

After reorganizing the source code structure, the include paths in source files need to be updated. This document provides guidelines for updating include statements to conform to the new directory structure.

## New Include Hierarchy

```
include/
├── api/
├── core/
├── database/
├── js/
├── query/
├── rbac/
├── transaction/
└── utils/
    └── memory/
```

## Include Path Conventions

### Internal Component Headers

When including a header from the same component, use a relative path:

```c
// Before: In src/transaction/transaction.c including transaction.h
#include "../include/transaction.h"

// After reorganization: 
#include "transaction/transaction.h"
```

### Cross-Component Headers

When including headers from other components, use the component path:

```c
// Before: In src/api/transaction_api.c including transaction.h
#include "../include/transaction.h"

// After reorganization:
#include "transaction/transaction.h"
```

## Common Header Replacements

| Old Include Path | New Include Path |
|-----------------|------------------|
| `#include "../include/api.h"` | `#include "api/api.h"` |
| `#include "../include/database.h"` | `#include "database/database.h"` |
| `#include "../include/js_engine.h"` | `#include "js/js_engine.h"` |
| `#include "../include/json.h"` | `#include "utils/json.h"` |
| `#include "../include/server.h"` | `#include "core/server.h"` |
| `#include "../include/transaction.h"` | `#include "transaction/transaction.h"` |
| `#include "../include/query_language.h"` | `#include "query/query_language.h"` |
| `#include "../include/rbac.h"` | `#include "rbac/rbac.h"` |
| `#include "../include/jwt.h"` | `#include "rbac/jwt.h"` |
| `#include "../include/utils/metrics.h"` | `#include "utils/metrics.h"` |
| `#include "../include/utils/memory/ref_counter.h"` | `#include "utils/memory/ref_counter.h"` |

## Update Process

The reorganization script (`scripts/reorganize_source.sh`) attempts to update some common include paths automatically. However, additional manual updates may be required. Follow these steps to ensure all include paths are correctly updated:

1. Run the reorganization script
2. Check for compilation errors related to missing header files
3. Update include paths according to the new structure
4. Test the build again
5. Repeat steps 2-4 until the project builds successfully

## Example Conversion

### Before

```c
#include "../include/api.h"
#include "../include/database.h"
#include "../include/json.h"
#include "../include/transaction.h"
#include "../include/utils/memory/ref_counter.h"
```

### After

```c
#include "api/api.h"
#include "database/database.h"
#include "utils/json.h"
#include "transaction/transaction.h"
#include "utils/memory/ref_counter.h"
```

## Special Cases

1. **External Libraries**: Include paths for external libraries (`quickjs.h`, standard library headers) should remain unchanged

2. **UUID Headers**: The `uuid.h` header should be included as:
   ```c
   #include "utils/uuid/uuid.h"
   ```