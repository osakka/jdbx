# Source Code Reorganization Implementation Plan

## Current State Analysis

After analyzing the current source code organization, we identified the following issues:

1. **Duplicate Files**: Most source files exist in both the root `src/` directory and within subdirectories (such as `src/api/`, `src/database/`, etc.), with almost all duplicate files being identical.

2. **Inconsistent Organization**: Some modules are partially organized (some files in subdirectories, some in the root).

3. **Special Cases**:
   - `transaction_api.c` exists in both `src/` and `src/api/` but with differences
   - Multiple versions of `main.c` exist: `main.c`, `main_fixed.c`, and `main_refactored.c`
   - There are tools in `src/tools/` that should be built separately

## Implementation Plan

### 1. Directory Structure

We'll implement the structure outlined in src_reorganization_plan.md:

```
src/
├── api/                      # API-related code
├── core/                     # Core server functionality
│   ├── server.c
│   ├── main.c
│   ├── ssl.c
│   └── cors.c
├── database/                 # Core database functionality
├── js/                       # JavaScript engine integration
├── query/                    # Query language processing
│   └── query_language.c
├── rbac/                     # Role-based access control
│   ├── rbac.c
│   └── jwt.c
├── transaction/              # Transaction system
├── utils/                    # Utility modules
│   ├── cache.c
│   ├── import_export.c
│   ├── json.c
│   ├── memory/               # Memory management utilities
│   └── metrics.c
└── tools/                    # Standalone tools (built separately)
    ├── jsondb_metrics.c
    └── jsondb_tools.c

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

### 2. File Reorganization Steps

1. **Create New Directory Structure**
   - Add missing directories (core, query, rbac)
   - Ensure all required subdirectories exist

2. **Resolve Duplicated Files**
   - Keep the subdirectory versions where they exist
   - Remove duplicates from the root src/ directory
   - For `transaction_api.c`, review differences and merge accordingly

3. **Move Top-Level Files to Appropriate Directories**
   - Move server.c, cors.c, ssl.c → src/core/
   - Move json.c → src/utils/
   - Move query_language.c → src/query/
   - Move rbac.c, jwt.c → src/rbac/
   - Select best main.c variant → src/core/

4. **Update Include Paths**
   - Update #include statements in all files to reflect new structure
   - Create proper header organization matching source organization

### 3. Header Files Update

1. **Create Matching Header Structure**
   - Reorganize header files to match source organization
   - Add missing header files where needed

2. **Update Header Include Guards**
   - Ensure all headers have proper include guards matching their new paths

3. **Standardize Header Usage**
   - Ensure source files include appropriate local headers

### 4. Makefile Update

1. **Update Source File Variables**
   - Update source file paths in Makefile to reflect new structure
   - Organize source files by module/component in Makefile

2. **Ensure Tools Are Built Separately**
   - Maintain separate build targets for tools
   - Prevent tools from being linked into main server

3. **Update Include Paths**
   - Set include paths to work with new structure

## Execution Steps

1. **Create Backup**
   - Create a backup branch before making changes

2. **Create New Directory Structure**
   - Execute mkdir commands to create necessary directories

3. **Move Files**
   - Execute git mv commands to move files to new locations
   - Resolve conflicts for transaction_api.c

4. **Update Include Paths**
   - Run search and replace for include paths

5. **Update Makefile**
   - Modify Makefile to match new structure

6. **Test Build**
   - Ensure the project builds successfully with new structure

7. **Create Documentation**
   - Update documentation to reflect the new structure

8. **Commit Changes**
   - Commit the reorganized code with clear message