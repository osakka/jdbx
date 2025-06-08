# JSONdb Configuration Fix Implementation Plan

**Date**: January 6, 2025  
**Target Version**: 3.2.0

## Overview

This plan addresses the configuration issues identified in CONFIGURATION_AUDIT.md, prioritizing the removal of hardcoded values and completion of the three-tier configuration system.

## Phase 1: Remove Hardcoded Values (Immediate)

### 1.1 Fix Hardcoded Install Directory

**File**: `src/components/utils/config_loader.c`  
**Lines**: 89-91

**Current**:
```c
LOG_DEBUG("Using default directory: /opt/jsondb.");
strncpy(g_binary_dir, "/opt/jsondb", PATH_MAX - 1);
```

**Fix**:
```c
const char* install_dir = getenv("JSONDB_INSTALL_DIR");
if (!install_dir) {
    // Try to use parent of binary directory
    char temp_path[PATH_MAX];
    if (getcwd(temp_path, sizeof(temp_path))) {
        // If we're in build/bin, go up two levels
        char* last_sep = strrchr(temp_path, '/');
        if (last_sep && strcmp(last_sep + 1, "bin") == 0) {
            *last_sep = '\0';
            last_sep = strrchr(temp_path, '/');
            if (last_sep) *last_sep = '\0';
        }
        install_dir = temp_path;
    } else {
        install_dir = ".";
    }
}
LOG_DEBUG("Using install directory: %s", install_dir);
strncpy(g_binary_dir, install_dir, PATH_MAX - 1);
```

### 1.2 Fix Hardcoded Documentation Path

**File**: `src/components/database/database.c`  
**Line**: 1186

**Current**:
```c
"For documentation and examples, check the `/opt/jsondb/docs/` directory."
```

**Fix**:
1. Add to `config_defaults.h`:
```c
#define DEFAULT_DOCS_DIR "docs"
```

2. Add to server_config_t structure:
```c
char* docs_dir;  /* Documentation directory */
```

3. Update welcome message:
```c
char docs_path[PATH_MAX];
snprintf(docs_path, sizeof(docs_path), "%s/%s", 
         config_get_binary_dir(), DEFAULT_DOCS_DIR);
         
char* message = malloc(4096);
snprintf(message, 4096, 
    "# Welcome to JSONdb!\n\n"
    "...\n"
    "## Getting Help\n\n"
    "For documentation and examples, check the `%s` directory.\n\n"
    "Happy coding! 🚀", 
    docs_path);
```

### 1.3 Fix Localhost Reference in Runtime Script

**File**: `build/jsondb_runtime.sh`  
**Line**: 96

**Current**:
```bash
(echo > /dev/tcp/localhost/$port) >/dev/null 2>&1
```

**Fix**:
```bash
(echo > /dev/tcp/${JSONDB_HOST}/$port) >/dev/null 2>&1
```

## Phase 2: Complete Environment Variable Support

### 2.1 Add Missing Environment Variables

**File**: `src/components/utils/environment.c`

Add support for:
```c
// Thread pool configuration
const char* thread_min = getenv("JSONDB_THREAD_POOL_MIN");
const char* thread_max = getenv("JSONDB_THREAD_POOL_MAX");
const char* thread_queue = getenv("JSONDB_THREAD_POOL_QUEUE_SIZE");
const char* thread_idle = getenv("JSONDB_THREAD_POOL_IDLE_TIMEOUT");

// Cache configuration
const char* cache_size = getenv("JSONDB_CACHE_SIZE");
const char* cache_ttl = getenv("JSONDB_CACHE_TTL");

// Additional paths
const char* docs_dir = getenv("JSONDB_DOCS_DIR");

// Update configuration
if (thread_min) config->thread_pool_min = atoi(thread_min);
if (thread_max) config->thread_pool_max = atoi(thread_max);
if (thread_queue) config->thread_pool_queue_size = atoi(thread_queue);
if (thread_idle) config->thread_pool_idle_timeout = atoi(thread_idle);
if (cache_size) config->cache_size = atol(cache_size);
if (cache_ttl) config->cache_ttl = atoi(cache_ttl);
if (docs_dir) {
    free(config->docs_dir);
    config->docs_dir = strdup(docs_dir);
}
```

### 2.2 Update Environment Template

**File**: `share/config/jsondb.env`

Add new sections:
```bash
# ====================================================================
# THREAD POOL CONFIGURATION
# ====================================================================

# Thread pool settings for connection handling
# JSONDB_THREAD_POOL_MIN=4
# JSONDB_THREAD_POOL_MAX=16
# JSONDB_THREAD_POOL_QUEUE_SIZE=1024
# JSONDB_THREAD_POOL_IDLE_TIMEOUT=60

# ====================================================================
# CACHE CONFIGURATION
# ====================================================================

# Query cache settings
# JSONDB_CACHE_SIZE=10485760    # 10MB in bytes
# JSONDB_CACHE_TTL=300          # 5 minutes

# ====================================================================
# ADDITIONAL PATHS
# ====================================================================

# Documentation directory
# JSONDB_DOCS_DIR=/opt/jsondb/docs

# Installation directory (for resolving relative paths)
# JSONDB_INSTALL_DIR=/opt/jsondb
```

## Phase 3: Implement Database Configuration

### 3.1 Create System Configuration Schema

**File**: `src/include/database/system_schemas.h`

```c
// System configuration document structure
typedef struct {
    const char* key;
    const char* value;
    const char* type;  // "string", "number", "boolean"
    const char* description;
    int priority;      // Higher number = higher priority
} system_config_entry_t;

// Define configuration schema
static const system_config_entry_t SYSTEM_CONFIG_SCHEMA[] = {
    {"server.port", NULL, "number", "Server port", 100},
    {"server.host", NULL, "string", "Server bind address", 100},
    {"server.max_connections", NULL, "number", "Maximum connections", 100},
    {"thread.pool_min", NULL, "number", "Minimum thread pool size", 100},
    {"thread.pool_max", NULL, "number", "Maximum thread pool size", 100},
    {"thread.queue_size", NULL, "number", "Thread pool queue size", 100},
    {"thread.idle_timeout", NULL, "number", "Thread idle timeout", 100},
    {"cache.size", NULL, "number", "Cache size in bytes", 100},
    {"cache.ttl", NULL, "number", "Cache TTL in seconds", 100},
    {"ssl.enabled", NULL, "boolean", "Enable SSL/TLS", 100},
    {"ssl.cert_path", NULL, "string", "SSL certificate path", 100},
    {"ssl.key_path", NULL, "string", "SSL private key path", 100},
    {NULL, NULL, NULL, NULL, 0}
};
```

### 3.2 Implement Database Configuration Functions

**File**: `src/components/utils/config_loader.c`

```c
int config_load_from_database(server_config_t* config) {
    if (!config || !g_database) {
        return 0;
    }
    
    LOG_DEBUG("Loading configuration from database");
    
    // Get _system_config collection
    collection_t* coll = db_get_collection(g_database, "_system_config");
    if (!coll) {
        LOG_DEBUG("No _system_config collection found");
        return 1; // Not an error - optional
    }
    
    // Query all configuration documents
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("configuration"));
    
    json_value_t* results = collection_find(coll, query, NULL, 0, 1000);
    if (!results || results->type != JSON_ARRAY) {
        json_free(query);
        return 1;
    }
    
    // Apply each configuration entry
    size_t count = json_array_size(results);
    for (size_t i = 0; i < count; i++) {
        json_value_t* doc = json_array_get(results, i);
        if (!doc) continue;
        
        json_value_t* key = json_object_get(doc, "key");
        json_value_t* value = json_object_get(doc, "value");
        
        if (key && key->type == JSON_STRING && value) {
            apply_config_value(config, key->value.string, value);
        }
    }
    
    json_free(query);
    json_free(results);
    
    LOG_INFO("Loaded %zu configuration entries from database", count);
    return 1;
}

static void apply_config_value(server_config_t* config, const char* key, json_value_t* value) {
    if (strcmp(key, "server.port") == 0 && value->type == JSON_INTEGER) {
        config->port = (int)value->value.integer;
        LOG_DEBUG("Database config: port = %d", config->port);
    } else if (strcmp(key, "server.host") == 0 && value->type == JSON_STRING) {
        free(config->host);
        config->host = strdup(value->value.string);
        LOG_DEBUG("Database config: host = %s", config->host);
    } else if (strcmp(key, "server.max_connections") == 0 && value->type == JSON_INTEGER) {
        config->max_connections = (int)value->value.integer;
        LOG_DEBUG("Database config: max_connections = %d", config->max_connections);
    }
    // ... continue for all configuration options
}
```

## Phase 4: Add Configuration API Endpoints

### 4.1 Configuration API

**File**: `src/components/api/config_api.c` (new)

```c
void handle_config_get(http_request_t* request, http_response_t* response, void* ctx) {
    // Return current configuration
    json_value_t* config_json = config_to_json(g_server_config);
    
    char* json_str = json_stringify(config_json, JSON_PRETTY);
    http_response_set_body(response, json_str);
    http_response_set_status(response, 200);
    http_response_add_header(response, "Content-Type", "application/json");
    
    free(json_str);
    json_free(config_json);
}

void handle_config_update(http_request_t* request, http_response_t* response, void* ctx) {
    // Parse request body
    json_value_t* updates = json_parse(request->body);
    if (!updates) {
        http_response_set_status(response, 400);
        http_response_set_body(response, "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Validate and apply updates
    int applied = 0;
    json_value_t* errors = json_create_array();
    
    // Iterate through updates
    const char** keys = json_object_keys(updates);
    for (int i = 0; keys[i]; i++) {
        json_value_t* value = json_object_get(updates, keys[i]);
        if (validate_config_key(keys[i], value)) {
            // Save to database
            save_config_to_database(keys[i], value);
            // Apply to running config
            apply_config_value(g_server_config, keys[i], value);
            applied++;
        } else {
            json_array_append(errors, json_create_string(keys[i]));
        }
    }
    
    // Return response
    json_value_t* result = json_create_object();
    json_object_set(result, "applied", json_create_integer(applied));
    json_object_set(result, "errors", errors);
    
    char* json_str = json_stringify(result, 0);
    http_response_set_body(response, json_str);
    http_response_set_status(response, 200);
    
    free(json_str);
    json_free(result);
    json_free(updates);
}
```

## Phase 5: Testing and Validation

### 5.1 Configuration Test Suite

Create comprehensive tests:

1. **Test Configuration Precedence**:
   ```bash
   # Test that database config overrides all
   export JSONDB_PORT=5001
   ./jsondb_server --port=5002 &
   # Set port to 5003 in database
   # Verify server restarts on port 5003
   ```

2. **Test All Configuration Options**:
   - Test each environment variable
   - Test each command-line flag
   - Test each database configuration

3. **Test Invalid Configurations**:
   - Invalid paths
   - Out-of-range numbers
   - Missing required values

### 5.2 Migration Guide

Create documentation for existing users:

1. **Backup existing configuration**
2. **Update environment files**
3. **Test in staging environment**
4. **Deploy to production**

## Timeline

- **Week 1**: Fix hardcoded values (Phase 1)
- **Week 2**: Complete environment support (Phase 2)
- **Week 3**: Implement database configuration (Phase 3)
- **Week 4**: Add API endpoints (Phase 4)
- **Week 5**: Testing and documentation (Phase 5)

## Success Criteria

1. No hardcoded paths or configuration values
2. All settings configurable through all three tiers
3. Configuration changes without restart (where applicable)
4. Complete documentation of all options
5. Zero regression in existing functionality

## Risk Mitigation

1. **Backward Compatibility**: Maintain support for existing configuration files
2. **Default Values**: Ensure sensible defaults for all settings
3. **Validation**: Validate all configuration values before applying
4. **Rollback**: Ability to revert configuration changes
5. **Testing**: Comprehensive test coverage before release