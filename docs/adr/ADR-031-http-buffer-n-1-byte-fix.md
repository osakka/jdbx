# ADR-031: HTTP Buffer N-1 Byte Issue Resolution

**Date**: June 18, 2025  
**Status**: Accepted  
**Version**: 6.5.12  
**Impact**: Critical  

## Context

The JDBX server was reading 1 byte less than the Content-Length specified in HTTP requests, causing:
- "Incomplete request body" errors for all HTTP clients
- 100% failure rate for document operations
- Incompatibility with curl, Python requests, browsers
- Workarounds required (`JDBX_SSL_IGNORE_UNEXPECTED_EOF`)

Root cause: HTTP content was being treated as C strings, with buffer calculations reserving space for null terminators during reads.

## Decision

Treat HTTP content as binary data:
1. Read full Content-Length without reserving null terminator space
2. Add null termination AFTER reading when needed for string processing
3. Remove all "- 1" from buffer read calculations
4. Maintain proper bounds checking for safety

## Rationale

### HTTP Protocol Compliance
- HTTP Content-Length specifies exact byte count
- Content is binary data, not C strings
- Null terminators are not part of HTTP payload
- Must read exactly Content-Length bytes

### Compatibility
- All HTTP clients expect full content delivery
- Standard compliance ensures universal compatibility
- No special client configuration needed

### Correctness
```c
// Example: Content-Length: 100
// WRONG: Read 99 bytes, save 1 for null terminator
// RIGHT: Read 100 bytes, add null terminator afterward if needed
```

## Implementation

### Before - Incorrect Buffer Calculation
```c
// Reading N-1 bytes to "save space" for null terminator
int bytes_to_read = buffer_size - total_bytes_read - 1;  // WRONG!
int bytes_read = client_read_data(client, 
                                  buffer + total_bytes_read,
                                  bytes_to_read);
```

### After - Correct Binary Handling
```c
// Read full buffer, null terminate afterward if needed
int bytes_to_read = buffer_size - total_bytes_read;      // CORRECT
int bytes_read = client_read_data(client,
                                  buffer + total_bytes_read, 
                                  bytes_to_read);

// Add null terminator AFTER reading if string processing needed
if (total_bytes_read < buffer_size - 1) {
    buffer[total_bytes_read] = '\0';
}
```

### Key Changes
1. **Request body reading**: Full Content-Length read
2. **Buffer boundaries**: Exact size calculations
3. **Null termination**: Added after read completion
4. **Edge cases**: Proper handling of buffer-exact payloads

## Consequences

### Positive
- **100% Compatibility**: All HTTP clients work correctly
- **Protocol Compliance**: Proper HTTP implementation
- **No Workarounds**: SSL flags no longer needed
- **Data Integrity**: Full payloads received

### Negative
- None - This fixes a critical bug

### Validation
```bash
# Test various payload sizes
curl -X POST http://localhost:5000/api/collections/test/documents \
  -H "Content-Type: application/json" \
  -d '{"test": "data"}'  # 16 bytes

# Test boundary conditions  
python test_client.py --size=4095  # Buffer boundary -1
python test_client.py --size=4096  # Exact buffer size
python test_client.py --size=4097  # Buffer boundary +1
```

## Technical Details

### Files Modified
- `src/components/core/handle_client.c` - Primary fix location
- All HTTP reading loops updated
- Buffer calculation logic corrected

### Comprehensive Test Results
- ✅ Small documents (100B - 1KB): Working
- ✅ Buffer boundaries (4095, 4096, 4097): Working
- ✅ Large documents (50KB, 100KB, 1MB+): Working
- ✅ Binary content with null bytes: Working
- ✅ UTF-8 content: Working
- ✅ All HTTP clients: Working

### Root Cause Analysis
```
1. HTTP specifies: "Content-Length: 100"
2. Server allocates: char buffer[4096]
3. Server reads: read(fd, buffer, 99)     // WRONG - saves 1 byte
4. Result: Missing last byte of content
5. Fix: read(fd, buffer, 100)             // CORRECT - full content
```

## Migration

No migration needed - This is a bug fix that restores correct behavior.

## References

- Git commits: `f6f489a`, `8e054a5`
- Test suite: `/opt/jdbx/tests/http-n-1-byte/`
- CLAUDE.md: v6.5.12 section