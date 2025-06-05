# Plan to Address Remaining Issues

## Issue 1: B+Tree and Hash Index Crashes

### Root Causes
1. Missing null pointer checks
2. No bounds validation on memory access
3. Unchecked memory allocations
4. Race conditions in concurrent access

### Fix Plan
```c
// 1. Add bounds checking
if (page_id + BTREE_PAGE_SIZE > tree->storage->mapped_size) {
    LOG_ERROR("Page ID %lu exceeds mapped region", page_id);
    return NULL;
}

// 2. Add null checks
btree_node_t* child = load_node(tree, node->pointers[index]);
if (!child) {
    LOG_ERROR("Failed to load child node");
    return -1;
}

// 3. Validate allocations
write_buffer_entry_t* entry = malloc(sizeof(write_buffer_entry_t));
if (!entry) {
    LOG_ERROR("Memory allocation failed");
    return -1;
}
```

## Issue 2: Integration of High-Performance Components

### Current State
- Memory-mapped storage implemented but not integrated
- B+Tree/Hash indexes exist but not used by queries
- Database still using old linear scan approach

### Integration Steps
1. Replace database storage backend
2. Wire indexes to query engine
3. Update persistence to use new format
4. Create migration tools

## Issue 3: Consistent Sub-millisecond Performance

### Current Bottlenecks
1. HTTP parsing overhead (~0.5ms)
2. JSON serialization (~0.2ms)
3. Thread context switches (~0.1ms)
4. Memory allocations (~0.1ms)

### Optimization Plan
1. **Implement Zero-Copy Path**
   - Direct buffer to socket writes
   - Pre-allocated response buffers
   - Minimal JSON generation

2. **CPU Affinity**
   - Pin worker threads to cores
   - NUMA-aware memory allocation
   - Reduce context switches

3. **Memory Pool Improvements**
   - Per-thread pools
   - Lock-free allocation
   - Pre-warmed buffers

## Implementation Priority

### Phase 1: Fix Critical Bugs (1 day)
- [ ] Add null checks to B+tree operations
- [ ] Add bounds validation to hash index
- [ ] Fix memory allocation error handling
- [ ] Add debug logging at crash points

### Phase 2: Basic Integration (2-3 days)
- [ ] Create adapter layer for mmap_storage
- [ ] Wire B+tree to query engine
- [ ] Update persistence format
- [ ] Create data migration tool

### Phase 3: Performance Optimization (2-3 days)
- [ ] Implement zero-copy response path
- [ ] Add CPU affinity settings
- [ ] Optimize memory pools
- [ ] Profile and eliminate bottlenecks

## Success Metrics
1. No segmentation faults in 1M operation test
2. All queries use indexes (no linear scans)
3. P99 latency < 1ms for point queries
4. P99 latency < 5ms for range queries
5. Support for 1B documents demonstrated

## Testing Plan
1. Unit tests for each component
2. Integration tests for full stack
3. Stress tests with 1M+ documents
4. Performance benchmarks at each stage
5. Data corruption tests

## Next Immediate Action
Start with Phase 1 - fixing the B+tree and hash index crashes by adding proper error handling and bounds checking.