# Configuration Management Action Plan

## Objective
Ensure 100% compliance with three-tier configuration priority system:
1. Environment file (lowest priority)
2. Command-line flags (medium priority) 
3. Database configuration (highest priority)

**Critical Rule**: NO hardcoded paths, filenames, flags, options, or IDs anywhere in the codebase.

## Phase 1: Fix Critical Hardcoded Values (IMMEDIATE)

### 1.1 Remove Hardcoded Paths
- [ ] Fix `/opt/jsondb` fallback in config_loader.c
- [ ] Remove hardcoded documentation path in main.c
- [ ] Update runtime script to use configurable paths
- [ ] Add BASE_PATH configuration option

### 1.2 Add Missing Environment Variables
- [ ] JSONDB_BASE_PATH
- [ ] JSONDB_DOC_PATH
- [ ] JSONDB_VAR_PATH
- [ ] JSONDB_BUILD_PATH
- [ ] JSONDB_SHARE_PATH

## Phase 2: Complete Command-Line Flag Implementation

### 2.1 Add Missing Long Flags
- [ ] --base-path (base installation directory)
- [ ] --var-path (runtime data directory)
- [ ] --doc-path (documentation directory)
- [ ] --thread-pool-min (minimum threads)
- [ ] --thread-pool-max (maximum threads)
- [ ] --thread-pool-queue-size
- [ ] --thread-pool-idle-timeout
- [ ] --cache-enabled
- [ ] --cache-max-size
- [ ] --cache-ttl
- [ ] --metrics-enabled
- [ ] --metrics-retention
- [ ] --index-query-threshold
- [ ] --index-time-threshold

### 2.2 Standardize Flag Format
- Long flags: Always use `--` prefix
- Short flags: Reserve `-` for essential options only
- Current short flags to keep:
  - `-h` (help)
  - `-v` (version)
  - `-d` (daemon)
  - `-p` (port)

## Phase 3: Implement Database Configuration

### 3.1 Create System Configuration Schema
```json
{
  "_id": "system_config",
  "type": "configuration",
  "version": 1,
  "settings": {
    "server": {
      "host": "0.0.0.0",
      "port": 5000,
      "daemon": true,
      "ssl": {
        "enabled": true,
        "cert_path": "/etc/ssl/certs/jsondb.crt",
        "key_path": "/etc/ssl/private/jsondb.key"
      }
    },
    "paths": {
      "base": "/opt/jsondb",
      "var": "/opt/jsondb/var",
      "log": "/opt/jsondb/var/jsondb_server.log"
    },
    "thread_pool": {
      "min_threads": 4,
      "max_threads": 16,
      "queue_size": 1000,
      "idle_timeout": 60
    },
    "cache": {
      "enabled": true,
      "max_size": 10000,
      "ttl": 300
    },
    "metrics": {
      "enabled": true,
      "retention": 15
    },
    "indexing": {
      "query_threshold": 10,
      "time_threshold": 50,
      "system_query_threshold": 5,
      "system_time_threshold": 10
    },
    "logging": {
      "level": "INFO",
      "trace_categories": ["NETWORK", "DATABASE"]
    }
  }
}
```

### 3.2 Implement Configuration API
- [ ] GET /api/config - Retrieve current configuration
- [ ] PUT /api/config - Update configuration (admin only)
- [ ] GET /api/config/reload - Reload configuration from database

### 3.3 Implement Live Configuration Updates
- [ ] Add configuration change notifications
- [ ] Implement safe configuration reloading
- [ ] Add configuration validation

## Phase 4: Update Runtime Scripts

### 4.1 jsondb_runtime.sh Updates
- [ ] Remove all hardcoded paths
- [ ] Use environment variables consistently
- [ ] Add configuration file detection
- [ ] Support all command-line flags

### 4.2 Create Configuration Template
- [ ] Create jsondb.env.template with all options
- [ ] Document each configuration option
- [ ] Provide production-ready defaults

## Phase 5: Documentation and Validation

### 5.1 Create Configuration Guide
- [ ] Document all configuration options
- [ ] Provide examples for each tier
- [ ] Document precedence rules
- [ ] Add troubleshooting guide

### 5.2 Add Configuration Validation
- [ ] Validate paths exist and are accessible
- [ ] Validate port ranges
- [ ] Validate SSL certificates if enabled
- [ ] Validate thread pool settings

### 5.3 Add Configuration Tests
- [ ] Test environment variable loading
- [ ] Test command-line flag parsing
- [ ] Test database configuration
- [ ] Test configuration precedence

## Implementation Order

1. **Week 1**: Phase 1 - Fix critical hardcoded values
2. **Week 2**: Phase 2 - Complete command-line implementation
3. **Week 3**: Phase 3 - Implement database configuration
4. **Week 4**: Phase 4 - Update runtime scripts
5. **Week 5**: Phase 5 - Documentation and validation

## Success Criteria

- [ ] Zero hardcoded values in codebase
- [ ] All configuration options available in all three tiers
- [ ] Configuration changes can be made at runtime
- [ ] Complete documentation of all options
- [ ] Automated tests for configuration system
- [ ] Production-ready configuration templates