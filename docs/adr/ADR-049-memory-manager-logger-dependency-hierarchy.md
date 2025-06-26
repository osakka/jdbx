# ADR-049: Memory Manager Logger Dependency Hierarchy

**Status**: ACCEPTED  
**Date**: 2025-06-26  
**Priority**: CRITICAL ARCHITECTURE  
**Scope**: Memory Management, Logging, Initialization Order

## Summary

Establishes the fundamental architectural principle that **memory management has no dependencies on logging systems**, ensuring memory allocation is available during all system initialization phases and preventing circular dependencies.

## Context

During Phase 1 of the exotic memory allocator integration, we discovered a critical architectural decision point: should memory management components depend on the logging system, or should they remain dependency-free?

### Investigation Findings

**Existing Memory Manager Pattern:**
```c
// From memory_manager.c - uses fprintf for all output
if (getenv("JDBX_MEM_DEBUG")) {
    fprintf(stderr, "memory_alloc: allocated_ptr=%p, header=%p, size=%zu\n", 
            allocated_ptr, header, size);
}
```

**Initialization Order Requirements:**
1. Memory management must be available FIRST
2. Logger initialization requires memory allocation
3. Creating memory→logger dependency would create circular dependency

**Single Source of Truth Principle:**
- Existing memory manager uses `fprintf(stderr, ...)` exclusively
- Zero external dependencies
- Works in all contexts (early init, main runtime, shutdown)

## Decision

**ACCEPTED**: Memory management components SHALL NOT depend on logging systems.

### Core Principles

1. **Dependency Hierarchy**: Memory Manager → Logger → Other Systems
2. **Zero Dependencies**: Memory management has no external dependencies except libc
3. **Consistent Output**: All memory management uses `fprintf(stderr, ...)` pattern
4. **Debug Control**: `JDBX_MEM_DEBUG` environment variable controls output

## Rationale

### 1. Initialization Order Critical Path
```
System Boot → Memory Manager Init → Logger Init → Application Init
     ↑              ↑                   ↑
   Requires      Requires         Requires both
   nothing      memory mgr         memory + logging
```

### 2. Circular Dependency Prevention
```
WRONG: Memory Manager → Logger → Memory Manager (CIRCULAR!)
RIGHT: Memory Manager → Logger (CLEAN HIERARCHY)
```

### 3. Emergency Reliability
- Memory allocation must work even if logging fails
- Emergency rollback must function without logging dependencies
- Early boot diagnostics require memory manager before logger

### 4. Single Source of Truth
- Existing memory_manager.c established the pattern
- Configuration system follows identical pattern
- Zero deviation from established architecture

## Implementation

### Memory Management Output Pattern
```c
/* CORRECT: Zero dependency pattern */
if (getenv("JDBX_MEM_DEBUG")) {
    fprintf(stderr, "Memory operation: details\n");
}

/* WRONG: Logger dependency pattern */
LOG_INFO("Memory operation: details");  // FORBIDDEN
```

### Files Following This Pattern
- `src/components/utils/memory_manager.c` ✅ (established)
- `src/components/utils/memory_allocator_config.c` ✅ (follows pattern)
- `src/components/utils/arena_allocator.c` ✅ (will follow)
- `src/components/utils/tlsf_allocator.c` ✅ (will follow)

### Debug Control Mechanism
```bash
# Enable memory debugging
export JDBX_MEM_DEBUG=1

# Run with debug output
./bin/jdbxd --daemon
```

## Consequences

### Positive
✅ **Zero Circular Dependencies**: Clean initialization hierarchy  
✅ **Maximum Reliability**: Memory allocation always available  
✅ **Emergency Resilience**: Works even if logging fails  
✅ **Consistent Architecture**: Single source of truth maintained  
✅ **Early Boot Support**: Memory management available immediately  

### Constraints
⚠️ **Limited Formatting**: No structured logging for memory operations  
⚠️ **Debug-Only Output**: No production memory operation logging  
⚠️ **stderr Output**: All memory debug goes to stderr, not log files  

### Trade-offs Accepted
- **Consistency over Convenience**: fprintf pattern vs LOG macros
- **Reliability over Features**: Simple output vs structured logging
- **Architecture over Aesthetics**: Dependency hierarchy over uniform logging

## Compliance

### Required Patterns
```c
/* Memory management debug output */
if (getenv("JDBX_MEM_DEBUG")) {
    fprintf(stderr, "Operation: %s, Status: %s\n", operation, status);
}

/* Emergency notifications */
fprintf(stderr, "EMERGENCY: Critical memory event\n");

/* Configuration status */
fprintf(stderr, "Memory allocator: %s\n", enabled ? "active" : "disabled");
```

### Forbidden Patterns
```c
/* FORBIDDEN: Logger dependencies */
#include "utils/logger.h"        // NO
LOG_INFO("Memory operation");    // NO
LOG_ERROR("Memory failure");     // NO
```

### Validation
- All memory management files compile without logger includes
- No undefined references to logger functions
- stderr output visible during early boot and emergency scenarios

## Related Decisions

- **ADR-028**: Checkpoint-based Memory Manager - established foundation
- **ADR-046**: Memory Allocator Integration - builds on this hierarchy
- **ADR-037**: Enterprise Logging Standards - applies AFTER memory management

## Implementation Status

- ✅ **Existing Memory Manager**: Already follows this pattern
- ✅ **Configuration System**: Aligned with pattern in Phase 1
- 🔄 **Arena Allocator**: Will follow pattern in Phase 2
- 🔄 **TLSF Allocator**: Will follow pattern in Phase 3

## Success Metrics

1. **Zero Circular Dependencies**: No memory→logger→memory chains
2. **Early Init Support**: Memory allocation works before logger init
3. **Emergency Reliability**: Memory rollback works without logging
4. **Consistent Output**: All memory components use identical fprintf pattern

---

**Architect**: Memory Management Team  
**Reviewer**: System Architecture Team  
**Implementation**: Phase 1 Complete, Ongoing through Phase 7

This architectural decision ensures **brain surgeon precision** in dependency management and establishes the foundation for **bar-raising** memory allocator integration without compromising system reliability.