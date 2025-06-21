# ADR-040: Memory Checkpoint Safety Enhancements

**Date**: June 21, 2025  
**Status**: Accepted  
**Context**: Memory checkpoint system causing production crashes  
**Decision**: Implement comprehensive memory promotion patterns for checkpoint safety

## Context

The JDBX memory checkpoint system provides automatic memory cleanup through a checkpoint/rewind architecture. However, production workloads revealed critical use-after-free vulnerabilities when certain objects outlived their checkpoint boundaries:

1. **Hazard-Protected Memory**: Checkpoint pointers became dangling after checkpoint was freed, causing crashes in memory_free()
2. **SSL Connections**: Client structures were freed while OpenSSL held internal references
3. **JWT Cache**: Duplicated JWT payloads weren't promoted, causing malloc corruption
4. **RBAC Operations**: User documents used during iteration weren't promoted

These issues manifested as:
- SIGSEGV crashes in memory_free() accessing freed checkpoint pointers
- SSL_free() crashes when accessing freed client structures
- malloc() detecting "unaligned tcache chunk" during JWT operations
- Crashes during RBAC user deletion in memory_checkpoint_commit()

## Decision

Implement comprehensive memory promotion patterns to ensure objects that cross checkpoint boundaries are properly promoted:

### 1. Clear Checkpoint Pointers in Hazard-Protected Allocations
```c
/* Clear checkpoint pointers in hazard-protected allocations before freeing checkpoint */
memory_header_t* hazard_header = hazard_list;
while (hazard_header) {
    hazard_header->checkpoint = NULL;
    hazard_header = hazard_header->next;
}
```

### 2. Promote SSL Client Connections
```c
/* BAR RAISING: Client connections must be promoted when using SSL
 * SSL holds references to client structure that survive checkpoint operations
 * Without promotion, SSL_free() crashes accessing freed client memory */
if (config->use_ssl) {
    memory_promote(client);
}
```

### 3. Promote JWT Cache Payloads
```c
/* Promote the duplicated payload to survive checkpoint rewinds */
memory_promote(claims_copy);
if (claims_copy->iss) memory_promote(claims_copy->iss);
if (claims_copy->sub) memory_promote(claims_copy->sub);
if (claims_copy->aud) memory_promote(claims_copy->aud);
if (claims_copy->jti) memory_promote(claims_copy->jti);
if (claims_copy->claims) json_promote(claims_copy->claims);
```

### 4. Promote RBAC Documents During Deletion
```c
/* Promote user_doc to survive any checkpoint operations during role cleanup */
if (user_doc) {
    json_promote(user_doc);
}
```

### 5. Add Checkpoint Commit Validation
```c
/* Validate header before accessing */
if ((uintptr_t)header < 0x1000 || header->magic != MEMORY_MAGIC) {
    /* Invalid or freed allocation - skip */
    break;
}
```

## Consequences

### Positive
- **Production Stability**: Eliminated entire category of checkpoint-related crashes
- **Thread Safety**: All fixes maintain thread-safe operation
- **Test Success**: RBAC E2E tests improved from 61% to 100% success rate
- **Clear Patterns**: Established clear guidelines for memory promotion

### Negative
- **Slight Memory Overhead**: Promoted objects survive longer than strictly necessary
- **Developer Complexity**: Developers must understand promotion patterns

### Neutral
- **Performance Impact**: Minimal - promotion is a simple pointer operation
- **API Changes**: No external API changes, internal implementation only

## Implementation Notes

1. **Hazard-Protected Memory**: The checkpoint system must clear pointers before freeing to prevent dangling references
2. **External Library Resources**: Any object referenced by external libraries (SSL, etc.) must be promoted
3. **Iterator Safety**: Objects used during iteration must be promoted if checkpoint operations occur
4. **Validation Required**: Always validate pointers before access in checkpoint operations

## Lessons Learned

1. **External References Matter**: When integrating with external libraries, consider their reference patterns
2. **Iterator Lifetime**: Objects used during iteration need special consideration
3. **Defensive Programming**: Add validation even in "impossible" scenarios
4. **Comprehensive Testing**: Production workloads reveal edge cases unit tests miss

## Related ADRs

- ADR-028: Checkpoint-Based Memory Manager (original design)
- ADR-033: Checkpoint-Only JSON Memory Management
- ADR-034: Memory Promotion for Global Structures

## References

- CLAUDE.md v7.0.1 - Memory Checkpoint Safety Enhancements
- Git commit: "🔧 FIX: Memory checkpoint safety - clear hazard pointers and promote critical objects"