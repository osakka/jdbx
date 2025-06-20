# ADR-030: Three-Tier Configuration and Environment Variable Management

**Date**: June 17, 2025  
**Status**: Accepted  
**Version**: 6.5.0  
**Impact**: Security  

## Context

The JDBX server had security vulnerabilities and configuration inconsistencies:
- Hardcoded admin credentials in code
- Environment variables not properly inherited by daemon process
- Conflicting environment variable names (`JDBX_INITIAL_*` vs `JDBX_BOOTSTRAP_*`)
- `setenv()` with overwrite flag preventing runtime configuration
- No secure password change mechanism

## Decision

Implement comprehensive three-tier configuration system with proper environment management:
1. Fix environment variable precedence (runtime > file)
2. Unify all variable names to `JDBX_BOOTSTRAP_*`
3. Implement secure password change endpoint
4. Ensure proper daemon inheritance of environment

## Rationale

### Security Requirements
- Production deployments need secure credential management
- No hardcoded values in binaries
- Password rotation capability required
- Environment isolation for security

### Configuration Flexibility  
- Different environments need different configs
- Runtime overrides for testing/debugging
- Database config for live updates
- Clear precedence rules

### Single Source of Truth
- One naming convention for clarity
- No duplicate variable names
- Consistent across all components

## Implementation

### Three-Tier Priority System
```
1. Database Configuration (Highest Priority)
   - Runtime changes without restart
   - Stored in _system_config collection
   
2. CLI Flags / Runtime Environment (Medium Priority)  
   - Command line arguments
   - Environment variables at runtime
   
3. Environment File (Lowest Priority)
   - /opt/jdbx/share/config/jdbx.env
   - Default configurations
```

### Critical Fix - setenv() Precedence
```c
// BEFORE - File overwrites runtime
setenv(key, value, 1);  // Always overwrite

// AFTER - Runtime takes precedence  
setenv(key, value, 0);  // Don't overwrite existing
```

### Unified Variable Names
```bash
# BEFORE - Inconsistent naming
JDBX_INITIAL_ADMIN_USER=admin
JDBX_BOOTSTRAP_ADMIN_USER=admin

# AFTER - Single convention
JDBX_BOOTSTRAP_ADMIN_USER=admin
JDBX_BOOTSTRAP_ADMIN_PASS=secure_password
```

### Password Change Endpoint
```http
PUT /api/auth/password
Authorization: Bearer <jwt_token>
Content-Type: application/json

{
  "current_password": "old_password",
  "new_password": "new_secure_password"
}
```

### Implementation Details
- PBKDF2-HMAC-SHA-256 verification
- Current password required
- Virtual layer field preservation
- Atomic document update

## Consequences

### Positive
- **Security**: No hardcoded credentials
- **Flexibility**: Runtime configuration without restart
- **Consistency**: Single naming convention
- **Usability**: Password rotation capability

### Negative
- **Migration**: Existing scripts need variable updates
- **Complexity**: Three-tier system needs documentation

### Neutral
- **Learning**: Operators must understand precedence
- **Tooling**: Management tools may need updates

## Technical Implementation

### Files Modified
- `src/initialize/config.c` - Fixed setenv() collision
- `src/components/rbac/rbac_database.c` - Unified variable names
- `src/components/api/auth_session_api.c` - Password change endpoint
- `build/jdbx_runtime.sh` - Environment precedence

### Security Features
```c
// Bootstrap credentials from environment
const char* admin_user = getenv("JDBX_BOOTSTRAP_ADMIN_USER");
const char* admin_pass = getenv("JDBX_BOOTSTRAP_ADMIN_PASS");

if (!admin_user || !admin_pass) {
    LOG_WARNING("Bootstrap admin credentials not configured");
    // Use secure defaults or require configuration
}
```

### Daemon Environment Inheritance
```bash
# Runtime script ensures environment passed to daemon
export JDBX_BOOTSTRAP_ADMIN_USER="${JDBX_BOOTSTRAP_ADMIN_USER}"
export JDBX_BOOTSTRAP_ADMIN_PASS="${JDBX_BOOTSTRAP_ADMIN_PASS}"
```

## Validation

- ✅ Environment variables properly inherited
- ✅ Runtime overrides working correctly
- ✅ Password change endpoint functional
- ✅ No hardcoded credentials remain
- ✅ Virtual layer compliance maintained

## References

- Git commits: `43ae932`, `de2a0b3`
- Related: ADR-029 (RBAC Single Source)
- CLAUDE.md: v6.5.0 section