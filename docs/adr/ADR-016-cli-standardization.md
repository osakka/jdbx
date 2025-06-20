# ADR-016: CLI Standardization

**Date**: June 16, 2025  
**Status**: Accepted  
**Version**: 6.2.0  
**Impact**: Medium  

## Context

Command-line interface was inconsistent:
- Mix of short and long flags
- No standard patterns
- Missing help system
- Incomplete coverage
- Poor discoverability

## Decision

Implement comprehensive CLI with:
- Standard flag patterns
- Short and long options
- Grouped by category
- Complete help system
- 33+ configuration options

## Rationale

### Usability Goals
- Industry standards
- Easy discovery
- Consistent patterns
- Complete coverage

### Design Principles
- GNU-style long options
- POSIX short options
- Logical grouping
- Clear descriptions

## Implementation

### CLI Categories

1. **Essential Operations**
```bash
-h, --help              Show help message
-v, --version           Show version information
-d, --daemon            Run as daemon
-f, --foreground        Run in foreground
-c, --config FILE       Configuration file
```

2. **Network Configuration**
```bash
-p, --port PORT         Server port (default: 5000)
-H, --host HOST         Bind address (default: 0.0.0.0)
    --ssl               Enable SSL/TLS
    --ssl-cert FILE     SSL certificate file
    --ssl-key FILE      SSL private key file
```

3. **Security Options**
```bash
--bootstrap-admin-user  Initial admin username
--bootstrap-admin-pass  Initial admin password
--jwt-secret           JWT signing secret
--disable-auth         Disable authentication
```

4. **Performance Tuning**
```bash
--thread-pool-min      Minimum threads (default: 4)
--thread-pool-max      Maximum threads (default: 16)
--max-connections      Connection limit
--request-timeout      Request timeout (seconds)
```

5. **Storage Configuration**
```bash
--data-dir DIR         Data directory
--db-file FILE         Database filename
--wal-enabled          Enable write-ahead log
--mmap-size SIZE       Memory map size
```

### Implementation Structure
```c
static struct option long_options[] = {
    // Essential
    {"help",                no_argument,       0, 'h'},
    {"version",             no_argument,       0, 'v'},
    {"daemon",              no_argument,       0, 'd'},
    {"foreground",          no_argument,       0, 'f'},
    {"config",              required_argument, 0, 'c'},
    
    // Network
    {"port",                required_argument, 0, 'p'},
    {"host",                required_argument, 0, 'H'},
    {"ssl",                 no_argument,       0, 0},
    
    // ... 25 more options
    {0, 0, 0, 0}
};
```

## Consequences

### Positive
- **Usability**: Easy to discover options
- **Standards**: Follows conventions
- **Complete**: All options exposed
- **Professional**: Enterprise-ready

### Negative
- **Complexity**: Many options
- **Documentation**: Must maintain
- **Testing**: More combinations

### Mitigations
- Grouped help output
- Man page generation
- Configuration files
- Sensible defaults

## Technical Details

### Files Modified
- `src/components/core/main.c` - CLI parsing
- `src/include/utils/cli.h` - Option definitions

### Help System
```
JDBX Database Server v6.2.0

Usage: jdbxd [OPTIONS]

Essential Operations:
  -h, --help              Show this help message
  -v, --version           Show version information
  ...

Network Configuration:
  -p, --port PORT         Server port (default: 5000)
  ...
```

### Validation
- All options parsed correctly
- Help system comprehensive
- Defaults appropriate
- Error messages clear

## Validation

- ✅ 33 options implemented
- ✅ Help system complete
- ✅ Short/long options working
- ✅ Categories logical
- ✅ Error handling robust

## References

- Git commit: CLI standardization
- GNU coding standards
- Related: ADR-015 (Security Infrastructure)