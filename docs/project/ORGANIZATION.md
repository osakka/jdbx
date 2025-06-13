# JDBX Project Organization

This document defines the organization principles for the JDBX project to maintain clarity and prevent duplication.

## Directory Structure Principles

The project follows a conventional structure with clear responsibilities:

```
jdbx/
├── build/            # GENERATED: Build artifacts only, never source code
├── docs/             # Documentation for all aspects of the project
├── scripts/          # Well-defined utility scripts (never hacks)
├── src/              # Source code only
│   ├── components/   # Implementation files
│   └── include/      # Header files
├── tests/            # Test code only
```

## Rules for Maintaining Clean Organization

### 1. Single Source of Truth

- Each component must have exactly ONE implementation location
- Each header must have exactly ONE definition location
- The Makefile in `src/Makefile` is the single build definition
- The entry point is `src/components/main.c`

### 2. No Duplication

- Never duplicate functionality between components
- Never create backup files in the repository (use git for history)
- Avoid having multiple scripts that do similar things

### 3. Proper Locations

- Implementation files MUST go in `src/components/`
- Header files MUST go in `src/include/`
- Generated files MUST go in `build/`
- Tests MUST go in `tests/`

### 4. Scripts Policy

- Scripts MUST have clear documentation
- Scripts MUST be categorized by purpose (`maintenance/`, `development/`, etc.)
- Scripts MUST NOT contain functionality that should be in the core application
- Temporary scripts for one-time tasks should be removed after use

### 5. Naming Conventions

- Component directories match between `src/components/` and `src/include/`
- Header files: `lowercase_with_underscores.h`
- Source files: `lowercase_with_underscores.c`
- Script files: `descriptive_purpose.sh`

## Enforcement

To enforce these rules:

1. Pull requests must comply with these guidelines
2. Code reviews should check for organizational issues
3. Makefiles should report errors if duplicate files are detected
4. CI processes should validate proper organization

## Immediate Cleanup Tasks

- [ ] Remove any `.bak` or backup files
- [ ] Verify no duplicate implementations exist
- [ ] Ensure all scripts have proper documentation
- [ ] Validate consistent naming across the project

By following these guidelines, we maintain a project that is easy to navigate, free from duplicates, and avoids accumulating technical debt from hacks and workarounds.