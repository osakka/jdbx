# ADR-025: Security Bootstrap Architecture

**Date**: June 18, 2025  
**Status**: Accepted  
**Version**: 6.5.5  
**Impact**: Security  

## Context

Critical security vulnerability discovered:
- Bootstrap mode bypassed ALL authentication
- Any endpoint accessible without credentials
- Intended for setup, enabled attack vector
- Document operations exposed during bootstrap

## Decision

Implement restricted bootstrap authentication:
1. Limit bypass to essential endpoints only
2. Maintain auth for all data operations
3. Auto-populate missing document fields
4. Balance security with usability

## Rationale

### Security Analysis
- Blanket bypass created huge vulnerability
- Bootstrap should only allow setup
- Data operations need protection
- Zero trust architecture required

### Usability Balance
- Initial setup must work
- Reduce friction for developers
- Smart defaults for documents
- Clear error messages

## Implementation

### Restricted Bootstrap
```c
// BEFORE - Dangerous blanket bypass
if (is_bootstrap_mode()) {
    return 1;  // Allow ALL requests!
}

// AFTER - Restricted to essentials
if (is_bootstrap_mode()) {
    const char* path = request->path;
    
    // Only allow essential endpoints
    if (strcmp(path, "/api/auth/login") == 0 ||
        strcmp(path, "/api/health") == 0 ||
        strcmp(path, "/api/status") == 0) {
        return 1;  // Allow without auth
    }
    
    // Everything else requires auth
    return 0;
}
```

### Smart Document Defaults
```c
// Auto-populate missing required fields
if (!json_object_get(document, "type")) {
    json_object_set(document, "type", 
                   json_create_string("document"));
}

if (!json_object_get(document, "owner")) {
    const char* user = get_current_user(ctx);
    json_object_set(document, "owner", 
                   json_create_string(user ?: "system"));
}

if (!json_object_get(document, "library")) {
    json_object_set(document, "library", 
                   json_create_string("default"));
}
```

## Consequences

### Positive
- **Security**: Attack vector eliminated
- **Zero Trust**: Default deny architecture
- **Usability**: Smart defaults reduce errors
- **Compliance**: Enterprise security ready

### Negative
- **Breaking**: Changes bootstrap behavior
- **Complexity**: More validation logic

### Mitigations
- Clear documentation
- Migration guide
- Error messages
- Setup wizard

## Technical Details

### Protected Endpoints
```
Bootstrap Allowed:
- POST /api/auth/login
- GET /api/health  
- GET /api/status

Requires Auth (even in bootstrap):
- ALL /api/documents/*
- ALL /api/collections/*
- ALL /api/libraries/*
- ALL other endpoints
```

### Security Validation
```bash
# Test unauthorized access
curl http://localhost:5000/api/documents
Response: {"error": "Unauthorized"}

# Login still works
curl -X POST http://localhost:5000/api/auth/login \
     -d '{"username":"admin","password":"admin123"}'
Response: {"token": "..."}
```

## Validation

- ✅ Bootstrap bypass restricted
- ✅ Document operations protected
- ✅ Smart defaults working
- ✅ Authentication flow intact
- ✅ Zero security regressions

## References

- Security audit findings
- Git commit: Bootstrap security
- Related: ADR-026 (Developer Experience)