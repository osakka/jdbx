# ADR-035: Integrated WAL Architecture - Eliminating Separate WAL Files

**Status**: Proposed  
**Date**: June 20, 2025  
**Author**: System Architect

## Context

JDBX currently maintains two separate files:
- `.jdbx` - Main database file with B-tree pages and data
- `.wal` - Write-Ahead Log for crash recovery and durability

This dual-file approach has several drawbacks:
1. **File Management Complexity**: Users must manage two files instead of one
2. **Atomic Operations**: Cannot guarantee atomic operations across two files
3. **Recovery Complexity**: Must coordinate between two files during recovery
4. **Space Overhead**: Separate file headers, metadata, and alignment waste
5. **Performance**: Additional file descriptors, system calls, and I/O operations

## Decision

**Integrate the Write-Ahead Log directly into the JDBX file format**, creating a true single-file database with built-in durability guarantees.

## Proposed Architecture

### 1. **Reserved WAL Region in JDBX File**
```
JDBX File Layout:
[Header Page 0]
[Bitmap Pages 1-N]
[WAL Region Pages N+1 to N+W]  <- NEW: Dedicated WAL pages
[Root Directory Page]
[Data Pages...]
```

### 2. **WAL Page Structure**
```c
typedef struct __attribute__((packed)) {
    page_header_t header;        // Standard page header with PAGE_TYPE_WAL
    uint64_t wal_sequence;       // WAL sequence number
    uint64_t num_entries;        // Number of WAL entries in this page
    uint64_t next_wal_page;      // Next WAL page in chain
    uint8_t entries[];           // Variable-length WAL entries
} wal_page_t;
```

### 3. **Circular WAL Buffer**
- Allocate fixed number of pages for WAL (e.g., 256 pages = 1MB)
- Use circular buffer with head/tail pointers in header
- Checkpoint when WAL is 80% full or on timer
- Reuse pages after checkpoint

### 4. **Enhanced Header Structure**
```c
typedef struct __attribute__((packed)) {
    // Existing fields...
    uint64_t wal_start_page;     // First WAL page
    uint64_t wal_num_pages;      // Number of WAL pages
    uint64_t wal_head_page;      // Current write position
    uint64_t wal_tail_page;      // Oldest unprocessed entry
    uint64_t wal_sequence;       // Global WAL sequence number
    // ...
} jdbx_header_t;
```

## Implementation Benefits

### 1. **True Single-File Database**
- Users manage ONE file only
- Simpler deployment and backup
- Atomic file operations

### 2. **Improved Performance**
- Single mmap() region for both data and WAL
- Reduced system calls
- Better cache locality
- No file coordination overhead

### 3. **Enhanced Durability**
- WAL and data always consistent
- No split-brain scenarios
- Simplified recovery logic

### 4. **Space Efficiency**
- Single file header/metadata
- Better page alignment
- Configurable WAL size based on workload

### 5. **Operational Simplicity**
- No WAL file cleanup needed
- No orphaned WAL files
- Single file permissions/ownership

## Migration Strategy

### Phase 1: Dual Mode Support
- Support both separate and integrated WAL
- New databases use integrated WAL
- Existing databases continue with separate WAL

### Phase 2: Migration Tool
- Provide tool to migrate existing databases
- Merge WAL file into main database file
- Preserve all data and transaction history

### Phase 3: Deprecate Separate WAL
- Remove separate WAL code paths
- Simplify codebase
- Document as major version change

## Technical Considerations

### 1. **WAL Size Management**
- Dynamic WAL region growth (relocate if needed)
- Configurable min/max WAL pages
- Automatic checkpoint triggers

### 2. **Crash Recovery**
- Scan WAL region on startup
- Apply uncommitted transactions
- Clear processed WAL entries

### 3. **Concurrent Access**
- WAL pages use same locking as data pages
- Maintain write ordering guarantees
- Support multiple readers during WAL writes

## Risks and Mitigations

1. **Risk**: Increased file size
   - **Mitigation**: Configurable WAL size, aggressive checkpointing

2. **Risk**: WAL fragmentation in file
   - **Mitigation**: Circular buffer design, periodic defragmentation

3. **Risk**: Backward compatibility
   - **Mitigation**: Version field in header, migration tools

## Conclusion

Integrating WAL into the JDBX format represents a true "bar-raising" architectural improvement that:
- Simplifies the user experience (one file to rule them all)
- Improves performance through unified memory mapping
- Enhances reliability through atomic file operations
- Reduces operational complexity
- Aligns with the "single source of truth" principle

This change would position JDBX as a modern, enterprise-grade embedded database with the simplicity of SQLite but the advanced features of larger systems.