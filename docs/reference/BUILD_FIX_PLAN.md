# Build System Fix Plan

## Problem Analysis

After attempting to build the JSON Database Server, we've identified several critical issues causing build failures:

1. **Duplicate Function Definitions**: API handler functions are defined in multiple files:
   - `api_handle_transaction_*` functions are defined in both `src/transaction.c` and `src/api/transaction_status_api.c`
   - `api_handle_visualization_*` functions are defined in both `src/transaction/transaction_visualization.c` and `src/api/visualization_api.c`

2. **Missing Function Implementations**: Some functions are called but not properly implemented:
   - `visualization_format_to_string` is called but not implemented
   - `transaction_set_isolation_level` is called but not implemented

3. **Function Signature Mismatches**: Several function calls have incorrect parameters:
   - `transaction_set_timeout` is called with wrong argument count
   - `transaction_create_savepoint` is called with incorrect parameter types
   - `transaction_release_savepoint` is called with incorrect parameter types

4. **Project Structure Issues**: The codebase appears to be in a transitional state between old and new organization:
   - A source reorganization plan exists (`src_reorganization_plan.md`) but has been only partially implemented
   - Some files exist in both root `src/` and in subdirectories like `src/api/`

## Solution Plan

### 1. Immediate Build Fixes (Short-term)

To get a working build as quickly as possible:

1. **Fix Function Declarations/Definitions**:
   - Move all API handler functions from `src/transaction.c` to `src/api/transaction_status_api.c`
   - Move all API handler functions from `src/transaction_visualization.c` to `src/api/visualization_api.c`
   - Create stub implementations for missing functions like `visualization_format_to_string`
   - Fix function signatures to match their declarations in header files

2. **Update Makefile**:
   - Modify the Makefile to exclude duplicate implementations
   - Ensure correct dependency tracking between files

### 2. Proper Project Organization (Medium-term)

Following the existing source reorganization plan:

1. **Complete Directory Structure Migration**:
   - Move all files to their designated directories according to `src_reorganization_plan.md`
   - Update include paths in all files
   - Ensure header files are in corresponding `include/` directories

2. **Header File Clean-up**:
   - Ensure each header file has proper include guards
   - Define clear interfaces with appropriate function declarations
   - Move type definitions to their most appropriate headers

3. **Function Organization**:
   - API handlers should be in `src/api/` files
   - Core transaction logic should be in `src/transaction/` files
   - Visualization code should be separate from core transaction code

### 3. Long-term Structure Improvements

1. **Implement Memory Management Improvements** described in `src_reorganization_plan.md`:
   - Memory Pool
   - Memory Tracker
   - Reference Counting

2. **Create Clear Component Boundaries**:
   - Each component should have a well-defined API
   - Reduce coupling between components
   - Implement proper layering to prevent circular dependencies

## Implementation Steps

### Phase 1: Emergency Build Fix

1. Create a branch for build fixes
2. Remove/comment duplicate function definitions
3. Implement missing functions with minimal stubs
4. Fix function signatures to match declarations
5. Update Makefile as needed
6. Test build until successful
7. Document all changes

### Phase 2: Function Implementation

1. Properly implement all the stub functions
2. Complete any partially implemented features
3. Add comprehensive error handling
4. Ensure memory safety in all operations

### Phase 3: Project Reorganization

1. Follow the reorganization plan to complete the directory structure migration
2. Refactor code to match the new organization
3. Update documentation to reflect changes
4. Create a proper component-based architecture

## Success Criteria

1. The project builds without errors
2. All test scripts run successfully
3. No duplicate symbol errors during linking
4. All API endpoints work as documented
5. Memory issues are resolved
6. Code organization follows the plan in `src_reorganization_plan.md`