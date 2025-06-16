# ADR-022: Checkpoint-Based Memory Management

**Date**: June 16, 2025  
**Status**: Active  
**Deciders**: Architecture Team  
**Technical Story**: Revolutionary approach to database memory management

## Context and Problem Statement

Manual memory management in C leads to memory leaks, especially on error paths. Database operations involve complex allocation patterns that are difficult to track manually.

## Decision Drivers

- Eliminate memory leaks by design
- Simplify error handling code
- Maintain performance
- Thread safety requirements
- Developer productivity

## Considered Options

1. **Garbage Collection** - Add GC to JDBX
2. **Reference Counting** - Smart pointers approach
3. **Arena Allocators** - Memory pools per operation
4. **Checkpoint System** - Transaction-like memory management

## Decision Outcome

Chosen option: **Checkpoint System** providing:
- Create checkpoints at operation boundaries
- Automatic cleanup on error via rewind
- Memory promotion for persistence
- Thread-local checkpoint stacks
- Zero overhead for success path

### Positive Consequences

- Memory leaks eliminated by design
- Vastly simplified error handling
- Clear ownership semantics
- Thread-safe by design
- Minimal performance impact

### Negative Consequences

- New paradigm to learn
- Migration effort required
- Checkpoint management overhead
- Promotion decisions needed

## Implementation Details

### Core API
```c
// Create checkpoint before operation
memory_checkpoint_t* checkpoint = memory_checkpoint_create();

// Allocate memory within checkpoint
user_t* user = memory_alloc(sizeof(user_t));
user->name = memory_strdup("John");

// On error - automatic cleanup
if (error) {
    memory_checkpoint_rewind(checkpoint);
    return ERROR;  // All memory freed
}

// Success - commit or promote
memory_promote(user);  // Survives checkpoint
memory_checkpoint_commit(checkpoint);
```

### Thread-Local Architecture
```c
typedef struct {
    memory_checkpoint_t* current;
    checkpoint_stack_t* stack;
    size_t depth;
} thread_memory_state_t;

static __thread thread_memory_state_t tls_memory;
```

### Magic Number Protection
- Allocation: `0xDEADBEEF`
- Freed: `0xFEEDF00D`
- Checkpoint: `0xCHECK123`

## Migration Strategy

1. Replace `malloc` → `memory_alloc`
2. Replace `free` → `memory_free`
3. Add checkpoints at operation boundaries
4. Identify promotable allocations
5. Remove manual cleanup code

### Before
```c
user_t* user = malloc(sizeof(user_t));
if (!user) return ERROR;

user->name = strdup(name);
if (!user->name) {
    free(user);
    return ERROR;
}

if (db_insert(user) != OK) {
    free(user->name);
    free(user);
    return ERROR;
}
```

### After
```c
memory_checkpoint_t* cp = memory_checkpoint_create();

user_t* user = memory_alloc(sizeof(user_t));
user->name = memory_strdup(name);

if (db_insert(user) != OK) {
    memory_checkpoint_rewind(cp);
    return ERROR;
}

memory_checkpoint_commit(cp);
```

## Performance Impact

- Allocation: ~5% overhead for checkpoint tracking
- Success path: Negligible (commit is fast)
- Error path: Faster than manual cleanup
- Memory usage: ~32 bytes per checkpoint

## Links

- [Memory Manager Integration Guide](../core-concepts/memory-manager-integration.md)
- [Migration Examples](../../examples/memory-migration/)