# Unified Memory Reclamation System Design

## Problem Statement

JDBX currently has two incompatible memory management systems:
1. **Checkpoint Memory**: Transaction-scoped automatic cleanup
2. **Hazard Pointers**: Deferred reclamation for lock-free data structures

These systems conflict when checkpoint memory tries to free objects still protected by hazard pointers.

## Bar-Raising Solution: Unified Reclamation

### Core Concept

Create a unified memory reclamation system where:
- Checkpoint memory delegates to hazard pointers for protected objects
- Hazard pointer retirement integrates with checkpoint cleanup
- Single source of truth for object lifecycle

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   Unified Memory Manager                 │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌─────────────────┐       ┌─────────────────┐        │
│  │   Checkpoint     │       │     Hazard      │        │
│  │     Stack        │◄─────►│    Pointers     │        │
│  └─────────────────┘       └─────────────────┘        │
│           │                          │                  │
│           ▼                          ▼                  │
│  ┌─────────────────────────────────────────┐          │
│  │        Unified Reclamation Queue         │          │
│  └─────────────────────────────────────────┘          │
│                      │                                  │
│                      ▼                                  │
│            ┌─────────────────┐                        │
│            │   Memory Free    │                        │
│            └─────────────────┘                        │
└─────────────────────────────────────────────────────────┘
```

### Implementation Details

1. **Tagged Allocations**:
   ```c
   typedef struct memory_block {
       memory_header_t header;
       uint32_t flags;  // MEMORY_FLAG_HAZARD_PROTECTED
       hazard_node_t* hazard_node;  // If protected
   } memory_block_t;
   ```

2. **Smart Checkpoint Rewind**:
   ```c
   void memory_checkpoint_rewind(memory_checkpoint_t* cp) {
       memory_block_t* block = cp->allocations;
       while (block) {
           if (block->flags & MEMORY_FLAG_HAZARD_PROTECTED) {
               // Delegate to hazard pointer system
               hazard_pointer_retire(block->hazard_node);
           } else {
               // Immediate free
               memory_free_internal(block);
           }
           block = block->next;
       }
   }
   ```

3. **Hazard Pointer Integration**:
   ```c
   void hazard_pointer_retire_callback(void* ptr) {
       memory_block_t* block = GET_BLOCK(ptr);
       if (block->header.checkpoint && 
           !block->header.checkpoint->committed) {
           // Already rewound - now safe to free
           memory_free_internal(block);
       }
   }
   ```

### Benefits

1. **Zero Memory Leaks**: All memory eventually freed
2. **Zero Use-After-Free**: Hazard pointers ensure safety
3. **Unified API**: Single allocation interface
4. **Performance**: No duplicate tracking overhead
5. **Debugging**: One system to instrument

### Migration Path

1. **Phase 1**: Add flags to memory blocks
2. **Phase 2**: Implement hazard pointer callbacks
3. **Phase 3**: Update skiplist to mark allocations
4. **Phase 4**: Test and validate
5. **Phase 5**: Remove workarounds

### Success Metrics

- Zero crashes under memory pressure
- Memory usage flat under create/delete cycles
- Performance within 5% of current system
- Clean valgrind/ASAN reports