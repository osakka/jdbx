# ADR-047: Configuration Management Enhancement

**Date**: June 24, 2025  
**Status**: Accepted  
**Version**: 7.2.5  
**Impact**: High - Architecture & Security  

## Context

JDBX required comprehensive configuration management to eliminate hardcoded values and provide production-grade flexibility. The existing three-tier configuration system (environment file → CLI flags → database config) needed expansion to cover network, security, and performance tuning options while maintaining architectural consistency.

## Decision

Implement comprehensive configuration options across four critical domains:
1. **Socket Configuration**: Network performance and connection tuning
2. **JWT Cache Configuration**: Authentication performance optimization
3. **SSL Security Configuration**: Production security hardening
4. **Password Policy Configuration**: User account security enforcement

## Rationale

### Configuration Gaps Identified
- Socket listen backlog hardcoded to 512
- JWT cache parameters hardcoded (1024 buckets, 30min TTL, 10K max entries)
- SSL verification options not configurable
- Password policy enforcement inflexible
- No runtime tuning capabilities for performance optimization

### Industry Best Practices
- All configuration options should use long flags (`--`) for clarity
- Short flags (`-`) reserved only for essential operations
- Centralized defaults with clear documentation
- Three-tier priority system maintained
- Runtime application where technically feasible

### Security Requirements
- SSL verification configurable for production compliance
- Password policy enforcement with lockout protection
- JWT cache security with configurable expiration limits
- No security-by-obscurity through hardcoded values

## Implementation

### Socket Configuration Options
```c
// config_defaults.h
#define DEFAULT_SOCKET_BACKLOG 512
#define DEFAULT_SOCKET_KEEPALIVE 1
#define DEFAULT_SOCKET_REUSEPORT 0

// CLI Options
--socket-backlog=N          # Listen backlog queue size
--socket-keepalive          # Enable TCP keep-alive
--socket-reuseport         # Enable SO_REUSEPORT for load balancing
```

### JWT Cache Configuration Options
```c
// config_defaults.h
#define DEFAULT_JWT_CACHE_BUCKETS 1024
#define DEFAULT_JWT_CACHE_TTL 1800
#define DEFAULT_JWT_CACHE_MAX_ENTRIES 10000

// CLI Options  
--jwt-cache-buckets=N      # Hash bucket count for distribution
--jwt-cache-ttl=N          # Cache TTL in seconds
--jwt-cache-max-entries=N  # Maximum cached tokens
```

### SSL Security Configuration Options
```c
// config_defaults.h
#define DEFAULT_SSL_VERIFY_PEER 0
#define DEFAULT_SSL_VERIFY_DEPTH 9
#define DEFAULT_SSL_SESSION_TIMEOUT 300

// CLI Options
--ssl-verify-peer          # Enable client certificate verification
--ssl-verify-depth=N       # Certificate chain verification depth
--ssl-session-timeout=N    # SSL session timeout seconds
```

### Password Policy Configuration Options
```c
// config_defaults.h
#define DEFAULT_MIN_PASSWORD_LENGTH 12
#define DEFAULT_MAX_LOGIN_ATTEMPTS 5
#define DEFAULT_LOGIN_LOCKOUT_TIME 300

// CLI Options
--min-password-length=N    # Minimum password length
--max-login-attempts=N     # Attempts before lockout
--login-lockout-time=N     # Lockout duration in seconds
```

### Technical Integration

1. **Configuration Structure Extension**
   - Added 12 new fields to `server_config_t` structure
   - Maintained field ordering for binary compatibility
   - All new fields properly initialized with defaults

2. **Command Line Parsing**
   - Extended `getopt_long()` with 12 new long options
   - Reserved option codes 330-362 for new configurations
   - Maintained short flag discipline (`-h`, `-v`, `-d`, `-f`, `-c` only)

3. **Runtime Application**
   - Socket options applied during `init_socket()` 
   - JWT cache configured before `jwt_cache_init()`
   - SSL options integrated into SSL context creation
   - Password policy ready for enforcement layer

4. **Default Value Integration**
   - All defaults centralized in `config_defaults.h`
   - Consistent naming convention: `DEFAULT_[COMPONENT]_[OPTION]`
   - Comprehensive documentation for each default value

## Consequences

### Positive
- **Security**: Production-grade SSL verification and password policies
- **Performance**: Tunable socket backlog, JWT cache optimization
- **Flexibility**: Runtime configuration without code changes
- **Standards**: Industry-standard long flag conventions
- **Maintainability**: Centralized defaults with clear documentation

### Negative
- **Complexity**: 12 additional configuration options to document and test
- **Memory**: Minor increase in `server_config_t` structure size
- **Migration**: Existing scripts may need updates for new features

### Neutral
- **Learning Curve**: Operators need to understand new configuration options
- **Documentation**: Additional reference documentation required

## Technical Implementation

### Files Modified
- `src/include/utils/config_defaults.h` - Added 12 new default constants
- `src/include/core/server.h` - Extended `server_config_t` with 12 new fields
- `src/initialize/config.c` - Added CLI parsing for 12 new options
- `src/components/utils/config_loader.c` - Added default value initialization
- `src/initialize/socket.c` - Applied socket configuration options
- `src/components/rbac/jwt_cache.c` - Applied JWT cache configuration
- `src/components/main.c` - Integrated JWT cache configuration

### Configuration Priority Maintained
```
1. Database Configuration (Highest Priority)
   - Runtime changes without restart
   - Stored in _system_config collection
   
2. CLI Flags / Runtime Environment (Medium Priority)  
   - Command line arguments override environment
   - New options follow this priority
   
3. Environment File (Lowest Priority)
   - Default configurations in jdbx.env
   - Fallback for unspecified options
```

### Security Features
```c
// Socket security with configurable options
if (config->socket_keepalive) {
    setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
}

// JWT cache with security limits
jwt_cache_configure(config->jwt_cache_buckets, config->jwt_cache_ttl, 
                    config->jwt_cache_max_entries);

// SSL verification for production
if (config->ssl_verify_peer) {
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_set_verify_depth(ctx, config->ssl_verify_depth);
}
```

## Validation

- ✅ All 12 configuration options compile and parse correctly
- ✅ Default values properly initialized in configuration system
- ✅ Socket options applied during socket initialization
- ✅ JWT cache configured with runtime values
- ✅ SSL options integrated into server configuration structure
- ✅ Three-tier configuration priority maintained
- ✅ Backward compatibility preserved (all defaults unchanged)
- ✅ Long flag convention followed consistently

## References

- Git commits: Configuration management implementation
- Related: ADR-030 (Environment Variable Management)
- Related: ADR-008 (Three-Tier Configuration Management)
- CLAUDE.md: v7.2.5 Configuration Management Excellence