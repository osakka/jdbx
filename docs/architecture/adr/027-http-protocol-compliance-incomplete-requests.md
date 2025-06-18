# ADR-027: HTTP Protocol Compliance for Incomplete Requests

## Status
Accepted

## Context
JDBX was experiencing issues with clients closing connections before sending the complete request body as specified in the Content-Length header. The discovery tests revealed a pattern where clients would send N-1 bytes (e.g., 5039 of 5040 bytes, 50038 of 50039 bytes) and then close the connection.

The initial approach attempted to "fix" incomplete JSON by:
- Detecting when exactly 1 byte was missing
- Analyzing the JSON structure to determine missing closing braces/brackets
- Automatically adding the missing character

This approach was fundamentally flawed because:
1. It violated HTTP protocol standards
2. It made assumptions about client intent
3. It could corrupt valid data by guessing wrong
4. It was a hack, not a proper solution

## Decision
We have implemented proper HTTP protocol compliance by treating incomplete request bodies as client errors. When a client specifies a Content-Length but sends fewer bytes before closing the connection, the server now:

1. **Detects Incomplete Requests**: Properly tracks bytes received vs Content-Length
2. **Returns HTTP 400 Bad Request**: With clear error message about incomplete body
3. **Logs the Issue**: Records the exact bytes received vs expected
4. **Closes Connection**: Ensures clean connection termination

## Consequences

### Positive
- **Protocol Compliance**: Server follows HTTP standards correctly
- **Data Integrity**: No risk of corrupting data with guessed content
- **Clear Error Messages**: Clients receive proper HTTP 400 errors
- **Improved Stability**: No more crashes from partial JSON processing
- **Enterprise Grade**: Professional error handling suitable for production

### Negative
- **No Tolerance**: Server strictly enforces Content-Length accuracy
- **Client Responsibility**: Clients must send complete requests

### Neutral
- **Standard Behavior**: This is how all HTTP servers should behave

## Implementation Details

### Key Changes
1. **Removed Graceful Degradation**: No more processing of partial bodies
2. **Removed JSON Completion**: No more guessing missing characters
3. **Added Proper Error Handling**: Clear HTTP 400 response for incomplete requests
4. **Improved Logging**: Detailed error messages showing bytes received vs expected

### Code Location
- **File**: `src/components/core/handle_client.c`
- **Function**: `handle_client()`
- **Lines**: ~735-754 (incomplete request detection and error response)

### Error Response Format
```
HTTP/1.1 400 Bad Request
Content-Type: text/plain
Content-Length: 51
Connection: close

Incomplete request body - connection closed early
```

## Testing Results
- Server no longer crashes on incomplete requests
- Proper HTTP 400 errors returned for all incomplete bodies
- Discovery tests show improved stability with no segfaults
- Several previously failing tests now pass

## Future Considerations
1. **Chunked Transfer Encoding**: Future support for Transfer-Encoding: chunked
2. **Request Timeout Configuration**: Allow configurable timeouts for slow clients
3. **Metrics Collection**: Track incomplete request patterns for monitoring

## References
- RFC 7230 Section 3.3.3: Message Body Length
- RFC 7231 Section 6.5.1: 400 Bad Request
- HTTP/1.1 Protocol Specification

## Conclusion
By properly handling incomplete requests according to HTTP standards, JDBX has achieved:
- Better protocol compliance
- Improved server stability
- Clear error communication
- Professional enterprise-grade behavior

This is a significant improvement over attempting to "fix" client errors with guesswork.