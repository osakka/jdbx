# ADR-009: Billion-Document Scale Architecture

**Date**: June 5, 2025  
**Status**: Active  
**Deciders**: Architecture Team  
**Technical Story**: Transform JDBX for enterprise-scale deployments

## Context and Problem Statement

JDBX v2.x architecture limited to ~1M documents with acceptable performance. Enterprise customers need billion-document support with sub-millisecond response times.

## Decision Drivers

- Performance at scale (1B+ documents)
- Memory efficiency
- Query response time requirements
- Concurrent operation support
- Hardware utilization

## Considered Options

1. **Incremental Optimization** - Tune existing architecture
2. **Distributed Architecture** - Shard across nodes
3. **Complete Redesign** - New storage and indexing
4. **Hybrid Approach** - Memory + disk optimization

## Decision Outcome

Chosen option: **Complete Redesign** with:
- Memory-mapped storage (256MB-100GB configurable)
- O(1) hash indexes for primary keys
- B+tree indexes for range queries
- Zero-copy access patterns
- Lock-free data structures

### Performance Achievements

```
Operation               Latency     Throughput
Random Read            2.06 μs     480K ops/sec
Sequential Write      10.28 μs      97K ops/sec
Range Query          403.00 μs       2K ops/sec
Concurrent Ops       230.53 μs      30K ops/sec
```

### Positive Consequences

- Sub-millisecond response at billion-document scale
- Efficient memory usage via mmap
- Excellent concurrent performance
- Hardware-optimized operations

### Negative Consequences

- Complex implementation
- Higher memory requirements
- Platform-specific optimizations needed
- Backward compatibility challenges

## Implementation Details

### Storage Architecture
```c
typedef struct {
    void* mmap_base;        // Memory-mapped region
    size_t mmap_size;       // Configurable 256MB-100GB
    hash_index_t* primary;  // O(1) primary key index
    btree_t* secondary;     // B+tree for ranges
} storage_engine_t;
```

### Index Design
- **Primary Index**: Open-addressing hash table
- **Secondary Indexes**: B+tree with 256-entry nodes
- **Memory Layout**: Cache-line aligned structures
- **Concurrency**: Lock-free readers, minimal writer locks

## Links

- [Performance Benchmarks](../../reference/specifications/performance-specs.md)
- [Storage Engine Design](../core-concepts/storage-engine.md)