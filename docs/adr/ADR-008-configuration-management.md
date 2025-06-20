# ADR-008: Three-Tier Configuration Management

**Date**: May 31, 2025  
**Status**: Accepted  
**Version**: 2.0.11  
**Impact**: High  

## Context

Configuration was scattered across:
- Hardcoded values in source
- Environment variables
- Command-line flags
- No runtime changes
- No clear precedence

## Decision

Implement three-tier configuration system:
1. Environment file (lowest priority)
2. Command-line flags (medium priority)
3. Database configuration (highest priority)

## Rationale

### Operational Requirements
- Runtime configuration changes
- Clear precedence rules
- Environment portability
- Zero hardcoded values

### Design Principles
- Higher tiers override lower
- Runtime changes possible
- Comprehensive coverage
- Type-safe access

## Implementation

### Configuration Sources

1. **Environment File** (`jdbx.env`)
```bash
JDBX_PORT=5000
JDBX_HOST=0.0.0.0
JDBX_LOG_LEVEL=info
JDBX_THREAD_POOL_SIZE=16
```

2. **Command-Line Flags**
```bash
jdbxd --port=5000 --host=0.0.0.0 --log-level=debug
```

3. **Database Configuration**
```json
{
  "type": "config",
  "uuid": "config-system",
  "library": "system",
  "settings": {
    "port": 5000,
    "log_level": "debug",
    "thread_pool_size": 32
  }
}
```

### Precedence Logic
```c
const char* config_get_string(const char* key) {
    // 1. Check database config (highest)
    char* db_value = config_get_from_db(key);
    if (db_value) return db_value;
    
    // 2. Check command-line flags (medium)
    char* flag_value = config_get_from_flags(key);
    if (flag_value) return flag_value;
    
    // 3. Check environment (lowest)
    char* env_value = config_get_from_env(key);
    if (env_value) return env_value;
    
    // 4. Return default
    return config_get_default(key);
}
```

## Consequences

### Positive
- **Flexibility**: Runtime changes without restart
- **Clarity**: Clear precedence rules
- **Portability**: Environment-specific configs
- **Security**: No hardcoded credentials

### Negative
- **Complexity**: Three sources to check
- **Debugging**: Which source is active?
- **Synchronization**: Database config cache

### Mitigations
- Config source logging
- Validation on startup
- Cache invalidation
- Config dump utility

## Technical Details

### Files Created
- `src/components/utils/config_loader.c` - Unified loader
- `src/components/utils/config_database.c` - DB storage
- `src/components/utils/config_env.c` - Environment parsing
- `src/include/utils/config.h` - Public API

### Configuration Keys
```c
// All configuration keys defined in one place
#define CONFIG_PORT "port"
#define CONFIG_HOST "host"
#define CONFIG_LOG_LEVEL "log_level"
#define CONFIG_THREAD_POOL_SIZE "thread_pool_size"
#define CONFIG_SSL_CERT "ssl_cert"
#define CONFIG_SSL_KEY "ssl_key"
// ... 50+ more keys
```

### Runtime Updates
```bash
# Update configuration via API
curl -X PUT http://localhost:5000/api/config \
  -d '{"log_level": "debug"}'
```

## Validation

- ✅ All hardcoded values eliminated
- ✅ Three-tier precedence working
- ✅ Runtime updates tested
- ✅ Configuration API functional
- ✅ Zero regression on startup

## References

- Git commit: `bfa2ebb` - Configuration management
- Related: ADR-009 (Metrics System)