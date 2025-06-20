# ADR-020: HTTP Protocol Compliance

**Date**: June 18, 2025  
**Status**: Accepted  
**Version**: 6.5.9  
**Impact**: High  

## Context

Dangerous violation of HTTP standards:
- Attempting to "guess" missing JSON content
- Adding closing braces to incomplete requests
- Violating Content-Length semantics
- Creating security vulnerabilities

## Decision

Implement proper HTTP protocol compliance:
1. Reject incomplete requests with HTTP 400
2. Remove all content guessing logic
3. Enforce Content-Length strictly
4. Clear error messages for clients

## Rationale

### Protocol Violations
- HTTP requires exact Content-Length bytes
- Guessing content violates RFC standards
- Security risk from content manipulation
- Data integrity compromised

### Professional Standards
- Follow HTTP RFCs strictly
- No magical fixes for client errors
- Clear error communication
- Predictable server behavior

## Implementation

### Remove Dangerous Hack
```c
// BEFORE - Dangerous content guessing
if (incomplete_json) {
    // Try to "fix" by adding closing braces
    strcat(buffer, "}}");  // DANGEROUS!
    LOG_WARNING("Guessed JSON completion");
}

// AFTER - Proper error handling
if (body_received < content_length) {
    http_response_t* error = http_response_create(400);
    http_response_set_body(error, 
        "{\"error\":\"Incomplete request body: received %zu of %zu bytes\"}", 
        body_received, content_length);
    return error;
}
```

### Strict Content-Length Enforcement
```c
// Read exactly Content-Length bytes
size_t content_length = get_content_length(request);
size_t received = 0;

while (received < content_length) {
    int chunk = read_chunk(client, buffer + received, 
                          content_length - received);
    if (chunk <= 0) {
        // Connection error or timeout
        return create_error_response(400, 
            "Incomplete request body");
    }
    received += chunk;
}

// Validate complete JSON
if (!json_validate(buffer, received)) {
    return create_error_response(400, 
        "Invalid JSON in request body");
}
```

## Consequences

### Positive
- **Standards**: Full HTTP compliance
- **Security**: No content manipulation
- **Clarity**: Clear error messages
- **Reliability**: Predictable behavior

### Negative
- **Compatibility**: Broken clients fail
- **Strictness**: No tolerance for errors

### Mitigations
- Clear error messages
- Documentation of requirements
- Client library examples
- Debugging guides

## Technical Details

### Error Response Format
```json
{
  "error": "Incomplete request body: received 5039 of 5040 bytes",
  "code": "INCOMPLETE_REQUEST",
  "details": {
    "expected": 5040,
    "received": 5039,
    "missing": 1
  }
}
```

### Testing Validation
```bash
# Test incomplete request
curl -X POST http://localhost:5000/api/documents \
     -H "Content-Length: 100" \
     -d '{"incomplete": true'

Response: HTTP/1.1 400 Bad Request
{
  "error": "Incomplete request body: received 19 of 100 bytes"
}
```

## Validation

- ✅ Content guessing removed
- ✅ Proper 400 errors returned
- ✅ HTTP standards compliance
- ✅ Clear error messages
- ✅ No security vulnerabilities

## References

- HTTP/1.1 RFC 7230 Section 3.3.3
- Git commit: HTTP compliance
- Related: ADR-031 (HTTP N-1 Byte Fix)