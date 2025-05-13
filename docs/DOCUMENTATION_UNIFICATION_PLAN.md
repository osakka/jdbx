# Documentation Unification Plan

## Background
This project has been maintaining documentation in two separate directories: `/doc` and `/docs`. This causes confusion and inconsistency. Following the "One source of truth" principle from CLAUDE.md, we have now unified all documentation in the `/docs` directory.

## Current State
- `/docs` is now the single source of truth for all documentation
- The migration has been completed
- All documentation follows a consistent directory structure
- Cross-references have been updated to reflect the new organization

## Migration Plan

### 1. Current Directory Structure
The unified documentation structure in `/docs` is:
```
docs/
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
- The `/doc` directory has been removed
- All references to documentation in source code now point to `/docs`
- All README.md files have proper locations referenced

### 4. Implementation Status
Following the "Delete partial concept files and ideas" principle:
1. Created necessary directories in `/docs`
2. Migrated all content
3. Deleted `/doc` directory
4. Updated all references

## Validation
The migration has been validated:
1. All content has been preserved
2. Cross-references have been updated
3. Files are properly organized in the structure
4. Original directory has been removed after successful migration

## Next Steps
With unification complete:
1. Continue maintaining documentation in the `/docs` directory
2. Ensure all new documentation follows the established structure
3. Update documentation as the codebase evolves