# Binary Persistence System

## Overview

The JSONdb server now includes a complete binary persistence system that automatically saves database changes to disk in an efficient binary format (.jdb). This system provides thread-safe, high-performance data persistence with configurable triggers and comprehensive error handling.

## Key Features

### 1. Automatic Persistence
- **Buffer-based triggers**: Saves triggered by 50 operations OR 1MB data threshold
- **Periodic saves**: Safety net saves every 30 seconds (only if changes exist)
- **Shutdown saves**: Guaranteed save on graceful server shutdown
- **Thread-safe**: All operations use proper mutex/condition variable patterns

### 2. Binary Format (.jdb)
- **Magic number verification**: Files start with 0x4A534442 ("JSDB")
- **CRC32 checksums**: Data integrity verification for headers and data
- **TLV encoding**: Type-Length-Value encoding for efficient storage
- **Variable-length serialization**: Optimized space usage
- **Metadata support**: Timestamps, version info, collection counts

### 3. Error Handling
- **API error propagation**: Disk write failures return errors to API clients
- **Rollback support**: Failed persistence triggers database rollback
- **Error tracking**: Last error message and timestamp stored
- **Graceful degradation**: Server continues operating on persistence failures

## Architecture

### Persistence Thread
The system uses a dedicated persistence thread that:
- Monitors database changes via condition variables
- Tracks operation counts and data size estimates
- Performs background saves without blocking main operations
- Handles both immediate and periodic save triggers

### Database Integration
- `db_notify_data_change_sync()`: Synchronous notification of changes
- `db_start_persistence_thread()`: Initialize persistence on database startup
- `db_stop_persistence_thread()`: Clean shutdown with final save

### Binary Format Structure
```
[Binary Header]
  - Magic number (4 bytes)
  - Version (2 bytes) 
  - Flags (2 bytes)
  - Timestamp (8 bytes)
  - Database size (8 bytes)
  - Collection count (4 bytes)
  - Checksum (4 bytes)

[Collection Data]
  - Collection header (name length, doc count, offsets, checksum)
  - Collection name (variable length)
  - Document data (TLV encoded JSON)
```

## Configuration

### Thresholds (defined in persistence.c)
```c
#define PERSISTENCE_BUFFER_OP_THRESHOLD 50        // Operations threshold
#define PERSISTENCE_BUFFER_SIZE_THRESHOLD 1048576 // 1MB size threshold  
#define PERSISTENCE_PERIODIC_SAVE_INTERVAL 30     // 30 seconds
```

### File Locations
- Database file: `/opt/jsondb/build/var/database.jdb`
- Extension: `.jdb` (JSON Database Binary)

## Thread Safety

The persistence system is fully thread-safe:
- Database operations acquire `database->lock` before modifications
- Persistence thread uses separate `persistence->mutex` for internal state
- Condition variables coordinate between operation and persistence threads
- No deadlocks: serialization function relies on caller-held locks

## Error Recovery

### Persistence Failures
1. API operations return error responses
2. Database changes are rolled back
3. Error details logged with timestamps
4. Server continues normal operation

### Startup Recovery
1. Server attempts to load existing .jdb file
2. Binary format validation with magic number and checksums
3. Fallback to empty database if file is corrupted
4. Automatic initialization of new binary format

## Performance

### Optimizations
- Buffer-based writes reduce disk I/O
- Binary format is more compact than JSON
- Background persistence doesn't block API operations
- Periodic saves provide safety without over-saving

### Monitoring
- Operation counts tracked per persistence cycle
- Data size estimates for buffer management
- Save timing and error tracking
- Debug logging for persistence events

## Implementation Files

### Core Components
- `src/components/database/persistence.c` - Persistence thread implementation
- `src/components/binary/binary_format.c` - Binary serialization/deserialization
- `src/components/database/simplified_db.c` - Database operations with persistence integration
- `src/include/database/database.h` - Persistence thread structure definitions

### Integration Points
- Database initialization: `db_init()` starts persistence thread
- Database operations: All mutations call `db_notify_data_change_sync()`
- Database shutdown: `db_close()` stops persistence thread with final save
- Error handling: Persistence failures propagate to API layer

## Testing

The system has been tested with:
- Multiple concurrent document insertions
- Server restart persistence verification
- Error condition handling
- Thread safety under load
- Binary format integrity checks

## Future Enhancements

Potential improvements:
- Configurable persistence thresholds
- Multiple persistence strategies
- Compression support
- Index persistence optimization
- Transaction log integration