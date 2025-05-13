# Documentation Unification Plan

## Background
This project has been maintaining documentation in two separate directories: `/doc` and `/docs`. This causes confusion and inconsistency. Following the "One source of truth" principle from CLAUDE.md, we will unify all documentation in the `/doc` directory.

## Current State
- `/doc` contains a well-structured organization with consistent subdirectories
- `/docs` contains partially duplicated content with some unique files
- Some documentation exists in both places, creating maintenance challenges
- The reorganization plan already specifies `/doc` as the target directory

## Migration Plan

### 1. Directory Structure
Maintain and extend the current `/doc` structure:
```
doc/
├── api/              # API documentation
├── architecture/     # Architecture documentation
├── development/      # Developer documentation
│   └── build/        # Build-related documentation
├── guidelines/       # Project guidelines
├── guides/           # User guides (new, from docs/guides)
├── integration/      # Integration documentation (new, from docs/integration)
├── reference/        # Reference material
└── status/           # Status tracking (new, from docs/status)
```

### 2. File Migration Steps

a) **Handle Duplicate Files**
- Compare content of files with identical names
- Keep the most comprehensive and up-to-date version
- If both contain unique information, merge the content
- Ensure consistent formatting

b) **Unique Files in `/docs`**
- Move `/docs/integration/` files to `/doc/integration/` ✓
- Move `/docs/status/` files to `/doc/status/` ✓
- Move `/docs/PRODUCTION_READINESS.md` to `/doc/reference/` ✓
- Move `/docs/reference/FORMAT_TRUNCATION_FIXES.md` to `/doc/reference/` ✓
- Copy root `/docs/index.md` to `/doc/` ✓
- Evaluate other unique files for relevance and placement

c) **Guides Directory**
- Create `/doc/guides/` directory ✓
- Copy `/docs/guides/index.md` to `/doc/guides/` ✓
- Incorporate unique content into appropriate `/doc/` subdirectories
- Focus on removing duplication rather than preserving directory structure

### 3. Clean Up
- Remove `/docs` directory after migration is complete
- Update any references to `/docs` in source code
- Ensure all README.md files have proper locations referenced

### 4. Implementation Approach
Following the "Delete partial concept files and ideas" principle:
1. Create new directories needed in `/doc` (integration, status)
2. Migrate unique content
3. Delete `/docs` when complete

## Validation
For each file migration:
1. Verify content is preserved
2. Update any cross-references
3. Ensure file is properly placed in the structure
4. Remove source file only after successful migration

## Next Steps
After unification:
1. Update any references in code to documentation files
2. Update documentation index files to reflect the new structure
3. Document the new documentation organization for contributors