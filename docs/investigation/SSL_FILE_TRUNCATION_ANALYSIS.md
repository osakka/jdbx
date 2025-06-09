# SSL File Truncation Investigation

**Date**: June 9, 2025  
**Issue**: Large files (like app.js, 309KB) are truncated when served over HTTPS, but small files work fine

## Facts

1. **Working State Before Changes**:
   - SSL was configured but NOT enforced
   - Client could connect via HTTP or HTTPS
   - Large files worked correctly
   - Used recursive keep-alive model

2. **Current State After Changes**:
   - SSL is now enforced (HTTP connections rejected)
   - Consolidated multiple client handlers into single source
   - Changed from recursive to loop-based keep-alive handling
   - Small files (< 8KB) work over SSL
   - Large files (309KB app.js) are truncated mid-transmission

3. **Specific Observations**:
   - Server sends correct Content-Length: 309344
   - Browser receives truncated content ending at "function renderDocumentsWithBatchSelection() {"
   - Small file (theme.js, 7KB) works perfectly
   - Connection appears to close prematurely
   - File descriptor becomes 0 in logs during cleanup

4. **Code Flow Issues Identified**:
   - SSL_write may return SSL_ERROR_WANT_WRITE when buffer is full
   - Our ssl_write() returns SUCCESS with 0 bytes_written in this case
   - The send loop treats 0 bytes as "connection closed" and breaks
   - Double-close of file descriptors in cleanup path
   - SSL connection cleanup happens in multiple places

## Root Cause Analysis

### Primary Issue: SSL Write Buffer Handling
When SSL's internal buffer is full, SSL_write returns SSL_ERROR_WANT_WRITE. Our code:
1. Returns SSL_SUCCESS with bytes_written = 0
2. Send loop interprets 0 bytes as connection failure
3. Loop breaks, connection closes prematurely

### Secondary Issues:
1. **Resource Management**: Multiple cleanup paths, double-close of FDs
2. **Error Handling**: Not distinguishing between "retry needed" vs "connection closed"
3. **Architecture**: Mixing concerns - SSL handling intertwined with HTTP logic

## Proposed Solution Architecture

### 1. Clean SSL Write Abstraction
Create a proper SSL write function that handles all retry logic internally:
- Handle SSL_ERROR_WANT_WRITE/READ transparently
- Return actual error only on real failures
- Ensure all requested bytes are written

### 2. Unified Connection Model
Single connection structure with clear lifecycle:
- One place for SSL setup
- One place for SSL cleanup  
- One send function that handles both SSL and plain sockets
- Clear ownership of file descriptors

### 3. Simplified Send Loop
High-level send loop that:
- Calls unified write function
- Handles only real errors
- No special casing for SSL vs plain

## Implementation Plan

### Phase 1: Fix SSL Write Function
1. Modify ssl_write to handle WANT_WRITE internally with retry loop
2. Ensure it returns error only on actual failure
3. Add proper logging for debugging

### Phase 2: Clean Up Connection Handling  
1. Remove duplicate SSL cleanup calls
2. Fix file descriptor ownership (set to -1 after close)
3. Ensure single cleanup path

### Phase 3: Simplify Send Loop
1. Remove SSL-specific error handling from send loop
2. Let ssl_write handle all SSL complexities
3. Make send loop protocol-agnostic

### Phase 4: Test & Verify
1. Test with large files over SSL
2. Test with multiple concurrent connections
3. Verify no resource leaks

## Success Criteria
- Large files (300KB+) transfer completely over SSL
- No connection drops or truncation
- Clean, single-source implementation
- No resource leaks or double-frees