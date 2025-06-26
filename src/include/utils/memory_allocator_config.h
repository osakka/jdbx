/**
 * @file memory_allocator_config.h
 * @brief Memory Allocator Configuration and Rollback System - Header
 * 
 * Provides configuration management and emergency rollback capabilities
 * for the exotic memory allocators. Integrates with the existing JDBX
 * configuration system for seamless operation.
 */

#ifndef MEMORY_ALLOCATOR_CONFIG_H
#define MEMORY_ALLOCATOR_CONFIG_H

#include <stdbool.h>

/**
 * Memory allocator configuration status structure
 */
typedef struct {
    bool initialized;           /* Configuration system initialized */
    bool exotic_enabled;        /* Master exotic allocator switch */
    bool arena_enabled;         /* Arena allocator configuration */
    bool tlsf_enabled;          /* TLSF allocator configuration */
    bool force_system_malloc;   /* Emergency system malloc fallback */
    bool debug_enabled;         /* Debug output enabled */
    bool arena_active;          /* Arena allocator actually active */
    bool tlsf_active;           /* TLSF allocator actually active */
} memory_allocator_status_t;

/**
 * ============================================================================
 * CONFIGURATION MANAGEMENT
 * ============================================================================
 */

/**
 * Initialize memory allocator configuration system
 * 
 * @return 0 on success, -1 on failure
 */
int memory_allocator_config_init(void);

/**
 * Shutdown memory allocator configuration system
 */
void memory_allocator_config_shutdown(void);

/**
 * Reload configuration from environment variables
 */
void memory_allocator_config_reload(void);

/**
 * ============================================================================
 * CONFIGURATION QUERIES
 * ============================================================================
 */

/**
 * Check if exotic allocators are enabled and should be used
 * 
 * @return true if exotic allocators should be used, false for system malloc
 */
bool memory_allocator_config_exotic_enabled(void);

/**
 * Check if Arena allocator should be used for checkpoint allocations
 * 
 * @return true if Arena allocator is active, false otherwise
 */
bool memory_allocator_config_arena_enabled(void);

/**
 * Check if TLSF allocator should be used for general allocations
 * 
 * @return true if TLSF allocator is active, false otherwise
 */
bool memory_allocator_config_tlsf_enabled(void);

/**
 * Check if debug mode is enabled for memory operations
 * 
 * @return true if debug output should be generated, false otherwise
 */
bool memory_allocator_config_debug_enabled(void);

/**
 * Check if system malloc is forced (emergency mode)
 * 
 * @return true if system malloc should be used regardless of other settings
 */
bool memory_allocator_config_force_system_malloc(void);

/**
 * ============================================================================
 * RUNTIME CONFIGURATION UPDATES
 * ============================================================================
 */

/**
 * Emergency disable all exotic allocators
 * 
 * Immediately disables all exotic allocators and forces system malloc.
 * Used for emergency rollback in production environments.
 * 
 * @return 0 on success, -1 on failure
 */
int memory_allocator_emergency_disable(void);

/**
 * Enable exotic allocators with specific configuration
 * 
 * @param arena Enable Arena allocator for checkpoint allocations
 * @param tlsf Enable TLSF allocator for general allocations
 * @return 0 on success, -1 on failure
 */
int memory_allocator_enable(bool arena, bool tlsf);

/**
 * ============================================================================
 * STATUS REPORTING
 * ============================================================================
 */

/**
 * Get detailed configuration status
 * 
 * @param status Pointer to status structure to fill
 */
void memory_allocator_config_status(memory_allocator_status_t* status);

/**
 * Print configuration status to log
 */
void memory_allocator_config_log_status(void);

/**
 * Validate configuration consistency
 * 
 * @return 0 if configuration is valid, -1 if issues found
 */
int memory_allocator_config_validate(void);

/**
 * ============================================================================
 * INTEGRATION UTILITIES
 * ============================================================================
 */

/**
 * Add memory allocator configuration template to environment file
 * 
 * @param env_file Path to environment file to append to
 * @return 0 on success, -1 on failure
 */
int memory_allocator_config_write_env_template(const char* env_file);

/**
 * ============================================================================
 * CONVENIENCE MACROS
 * ============================================================================
 */

/* Quick configuration checks for use in allocation paths */
#define SHOULD_USE_ARENA_ALLOCATOR() memory_allocator_config_arena_enabled()
#define SHOULD_USE_TLSF_ALLOCATOR() memory_allocator_config_tlsf_enabled()
#define SHOULD_DEBUG_MEMORY() memory_allocator_config_debug_enabled()
#define SHOULD_FORCE_SYSTEM_MALLOC() memory_allocator_config_force_system_malloc()

#endif /* MEMORY_ALLOCATOR_CONFIG_H */