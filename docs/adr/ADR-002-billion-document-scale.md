# ADR-002: Billion-Document Scale Architecture

**Date**: June 5, 2025  
**Status**: Accepted  
**Version**: 3.0.0  
**Impact**: High  

## Context

JDBX v2.x architecture limited to ~1M documents with acceptable performance. Enterprise customers need billion-document support with sub-millisecond response times.

## Decision

Complete storage engine redesign for billion-document scale:
- Memory-mapped storage (256MB-100GB configurable)
- O(1) hash indexes for primary keys
- B+tree indexes for range queries
- Zero-copy access patterns
- Lock-free data structures

## Rationale

### Performance Requirements
- Sub-millisecond response at 1B+ documents
- 100K+ concurrent operations/second
- Linear scaling with hardware
- Predictable latencies

### Technical Approach
- **Memory-Mapped Files**: OS-managed paging
- **Lock-Free Reads**: Wait-free reader access
- **Cache-Aligned**: Optimize CPU cache usage
- **Zero-Copy**: Direct pointer access to data

## Implementation

### Storage Architecture
```c
typedef struct {
    void* mmap_base;        // Memory-mapped region
    size_t mmap_size;       // Configurable 256MB-100GB
    hash_index_t* primary;  // O(1) primary key index
    btree_t* secondary;     // B+tree for ranges
    skiplist_t* data;       // Lock-free skiplist
} storage_engine_t;
```

### Performance Achievements
```
Operation               Latency     Throughput
Random Read            2.06 μs     480K ops/sec
Sequential Write      10.28 μs      97K ops/sec
Range Query          403.00 μs       2K ops/sec
Concurrent Ops       230.53 μs      30K ops/sec
```

### Index Design
1. **Primary Index**: Open-addressing hash table
   - O(1) lookup complexity
   - Cache-friendly probing
   - Dynamic resizing

2. **Secondary Indexes**: B+tree structure
   - 256-entry nodes
   - Optimized for range scans
   - Copy-on-write updates

3. **Skiplist Storage**: Lock-free operations
   - Probabilistic balancing
   - Wait-free traversal
   - Hazard pointer protection

## Consequences

### Positive
- **Enterprise Scale**: Handles billions of documents
- **Performance**: Sub-millisecond operations
- **Concurrency**: Lock-free reader scalability
- **Efficiency**: Memory-mapped I/O optimization

### Negative
- **Complexity**: Sophisticated implementation
- **Memory**: Higher base memory requirements
- **Platform**: OS-specific optimizations needed
- **Migration**: Breaking changes from v2.x

### Mitigations
- Comprehensive documentation
- Migration tools provided
- Fallback to disk for memory pressure
- Platform abstraction layer

## Technical Details

### Key Components
- `src/components/storage/mmap_storage.c` - Memory mapping
- `src/components/index/hash_index.c` - Primary index
- `src/components/index/btree_index.c` - Range indexes
- `src/components/lockfree/skiplist.c` - Data storage

### Configuration
- `--mmap-size`: Memory map size (256MB-100GB)
- `--index-cache-size`: Index cache allocation
- `--max-concurrent-ops`: Connection limits

## Validation

- ✅ 1 billion document test passed
- ✅ Sub-millisecond latencies achieved
- ✅ 480K reads/second sustained
- ✅ Lock-free readers verified
- ✅ Memory efficiency validated

## References

- Performance benchmarks: `docs/reference/performance-benchmarks.md`
- Related: ADR-003 (Lock-Free Architecture)