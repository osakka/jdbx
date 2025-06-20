# ADR-037: Clean Cut to Integrated WAL - ONE SOURCE OF TRUTH

**Status**: Decided  
**Date**: June 20, 2025  
**Author**: System Architect

## Decision

**COMPLETE CLEAN CUT** - Delete all separate WAL code and implement integrated WAL as the ONLY format.

## Principles

1. **ONE SOURCE OF TRUTH**: Single file format, no alternatives
2. **NO BACKWARD COMPATIBILITY**: Clean break, fresh start
3. **ARCHITECTURAL EXCELLENCE**: Pure, clean implementation
4. **ZERO LEGACY CODE**: Remove all old WAL file handling

## Implementation

### 1. DELETE All Separate WAL Code
- Remove `wal.fd` from page manager
- Remove all WAL file creation/opening
- Remove `*.wal` file handling
- Remove dual-mode detection
- Remove all legacy WAL functions

### 2. NEW Unified JDBX Format
```
[Header Page]           - Enhanced with WAL pointers
[Bitmap Pages]          - Page allocation bitmap  
[WAL Ring Buffer]       - NEW: Integrated WAL pages
[Root Directory]        - Shifted down by WAL size
[Data Pages]            - All user data
```

### 3. SINGLE File Benefits
- One file to backup
- One file to manage
- Atomic operations guaranteed
- No synchronization issues
- No orphaned files

## Clean Cut Implementation Plan

### Step 1: Delete Old Code
```bash
# Remove all separate WAL code
git rm src/components/storage/jdbx_wal_separate.c
git rm src/components/storage/jdbx_wal_legacy.c
# Remove from page manager
# Remove from all headers
```

### Step 2: Implement Pure Integrated WAL
- WAL is ALWAYS part of the file
- No options, no modes, no choices
- Simple, clean, elegant

### Step 3: Update All Tools
- Remove `.wal` file handling from all utilities
- Update documentation to show single file
- Remove all migration code

## Result

**JDBX = One File Database**
- Just like SQLite but better
- No configuration needed
- It just works
- True architectural excellence