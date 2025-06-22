# ADR-029: API Monolith Decomposition

**Status**: Implemented  
**Date**: June 22, 2025  
**Version**: 7.2.0  

## Context

The main API routing file (`src/components/core/api.c`) had grown to 5,013 lines handling 89 different routes across 12 different functional areas. This monolithic structure presented several challenges:

1. **Maintainability**: Finding specific functionality required searching through thousands of lines
2. **Testing**: Difficult to unit test specific API domains in isolation
3. **Development**: Multiple developers working on different API features created merge conflicts
4. **Compilation**: Any change to any API required recompiling the entire 5,000+ line file
5. **Cognitive Load**: Understanding the full API surface required reading one massive file

## Decision

We will systematically decompose the API monolith into focused, domain-specific modules following these principles:

1. **Domain Separation**: Each major API domain gets its own module (documents, auth, libraries, etc.)
2. **Single Source of Truth**: No duplicate implementations - each handler exists in exactly one place
3. **Zero Regressions**: All existing functionality must work identically after extraction
4. **Clean Extraction**: Remove all commented code after successful migration
5. **Incremental Approach**: Extract one module at a time with full validation between each

## Implementation

### Phase 1: Document Operations Module (Completed)

**Module**: `api_documents.c` (848 lines)  
**Routes Extracted**: 14 document-related endpoints  
**Reduction**: api.c reduced from 5,013 → 3,600 lines (28%)  

#### Routes Moved:
- Unified Documents API (6 routes): `/api/documents/*`
- Collection-Scoped API (6 routes): `/api/collections/*/documents/*`
- Field-Level Operations (2 routes): field access endpoints

#### Technical Approach:
1. Created new module with proper headers and includes
2. Extracted all handler functions with identical signatures
3. Moved helper functions used only by document handlers
4. Updated api.c to include the new module header
5. Removed old implementations after verification

### Planned Decomposition (Phases 2-12)

1. **Authentication & Sessions** (~800 lines, 13 routes)
2. **Library Management** (~600 lines, 9 routes)
3. **RBAC Management** (~800 lines, 10 routes)
4. **Metrics & Monitoring** (~500 lines, 8 routes)
5. **System Administration** (~400 lines, 4 routes)
6. **Index Management** (~700 lines, 8 routes)
7. **JavaScript Integration** (~600 lines, 6 routes)
8. **Data Visualization** (~600 lines, 6 routes)
9. **Schema Validation** (~500 lines, 6 routes)
10. **Import/Export & Backup** (~500 lines, 3 routes)
11. **Collections Management** (~300 lines, 2 routes)

### Final Target State

- `api.c`: ~800 lines (core routing logic only)
- 12 focused modules: ~600 lines average each
- Total unchanged: ~8,000 lines across all files
- Clear separation of concerns
- Parallel development enabled

## Consequences

### Positive

1. **Improved Maintainability**: Each module focuses on one domain
2. **Better Testing**: Domain-specific modules can be unit tested in isolation
3. **Faster Compilation**: Changes to one API domain don't recompile everything
4. **Parallel Development**: Multiple developers can work on different modules
5. **Clearer Architecture**: API structure is evident from file organization
6. **Reduced Cognitive Load**: Understanding one domain doesn't require reading 5,000 lines

### Negative

1. **More Files**: 13 files instead of 1 (mitigated by clear naming)
2. **Initial Effort**: Systematic extraction requires careful work
3. **Include Dependencies**: Must manage module dependencies properly

### Neutral

1. **Binary Size**: No change - same code, different organization
2. **Performance**: No runtime impact - compile-time organization only
3. **API Surface**: Zero changes to external API

## Validation

Each extracted module must pass:

1. **Compilation**: Zero warnings with `-Wall -Wextra`
2. **Functionality**: All routes work identically
3. **Testing**: Existing tests pass without modification
4. **Performance**: No degradation in response times
5. **Integration**: Seamless integration with remaining monolith

## Lessons Learned

From Phase 1 (Document Operations):

1. **Helper Functions**: Extract module-specific helpers to avoid dependencies
2. **Clean Git History**: Use #if 0 blocks initially, then remove in separate commit
3. **Incremental Testing**: Test after each function extraction
4. **Header Design**: Clear, well-documented headers improve module usability
5. **Single Source of Truth**: Resist temptation to "improve" while extracting

## References

- Issue: API Monolith Decomposition
- Related: ADR-028 (Checkpoint Memory Management)
- Design Doc: `/opt/jdbx/docs/API_DECOMPOSITION_ANALYSIS.md`
- Plan: `/opt/jdbx/docs/COMPREHENSIVE_SYSTEMATIC_EXECUTION_PLAN.md`