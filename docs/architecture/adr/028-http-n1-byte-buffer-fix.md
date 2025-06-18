# ADR-028: HTTP N-1 Byte Buffer Fix

**Status**: Implemented  
**Date**: June 18, 2025  
**Author**: JDBX Development Team

## Context

JDBX server was experiencing "Incomplete request body" errors when HTTP clients sent requests, particularly noticeable with:
- curl 8.x (using OpenSSL 3.x)
- Python requests library
- Requests near buffer size boundaries (4096 bytes)
- Large JSON documents

Initial investigation incorrectly focused on SSL/TLS issues and OpenSSL 3.x client behavior. The real issue was in our HTTP buffer handling code.

## Problem

The server was treating HTTP content as C strings during buffer read operations, reserving 1 byte for null termination:

```c
// INCORRECT: Reserving space for null terminator during reads
size_t read_size = buffer_size - total_bytes_read - 1;
```

This caused the server to read exactly 1 byte less than the Content-Length header specified, resulting in:
- "Incomplete request body" errors
- Failed document creation
- Client compatibility issues

## Decision

Remove the `- 1` from all buffer read calculations and treat HTTP content as binary data:

```c
// CORRECT: Read full buffer without reserving null terminator space
size_t read_size = buffer_size - total_bytes_read;
```

## Implementation

### Changes Made

1. **File**: `src/components/core/handle_client.c`
   - Line 427: Fixed header reading loop
   - Line 652: Fixed body reading with dynamic buffer reallocation
   - Line 409: Fixed loop termination condition
   - Multiple locations: Removed `- 1` from all `buffer_size - total_bytes_read` calculations

2. **Principle**: HTTP content is binary data, not C strings
   - Read the full Content-Length as specified
   - Add null termination AFTER reading if needed for processing
   - Never modify read size based on string assumptions

### Testing

Created comprehensive test suite (`testing/test_n1_comprehensive.py`) that validates:
- Document sizes from 100 bytes to 1MB+
- Buffer boundary conditions (4095, 4096, 4097 bytes)
- Edge cases: null bytes, UTF-8 content, exact buffer sizes
- Multiple client types: curl, Python requests, raw sockets

## Consequences

### Positive
- ✅ 100% client compatibility restored (curl 8.x, Python requests, etc.)
- ✅ Proper HTTP protocol compliance
- ✅ No more "Incomplete request body" errors
- ✅ Handles binary content correctly
- ✅ Zero performance impact

### Negative
- None identified

### Neutral
- Requires understanding that HTTP content != C strings
- Null termination must be added explicitly when needed

## Lessons Learned

1. **Don't assume string semantics for binary protocols**: HTTP content is binary data
2. **Root cause analysis is critical**: Initial SSL/TLS focus was incorrect
3. **Test with multiple clients**: Different clients revealed the issue
4. **Buffer calculations must be exact**: Off-by-one errors break protocols

## Alternatives Considered

1. **Modify Content-Length parsing**: Rejected - would break HTTP standard
2. **Add padding to client requests**: Rejected - client-side hack
3. **Special handling for OpenSSL 3.x**: Rejected - not the real issue
4. **Complete JSON on server**: Rejected - dangerous content modification

## References

- HTTP/1.1 RFC 2616 Section 4.4 (Message Length)
- Original issue analysis in `testing/N1_BYTE_FIX_SUMMARY.md`
- Test suite: `testing/test_n1_comprehensive.py`

## Review

This ADR documents a critical bug fix that restored proper HTTP protocol compliance in JDBX server by correctly handling Content-Length without C string assumptions.