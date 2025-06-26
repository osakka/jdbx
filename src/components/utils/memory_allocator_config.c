/**
 * @file memory_allocator_config.c
 * @brief Memory Allocator Configuration and Rollback System
 * 
 * This module provides configuration management and emergency rollback
 * capabilities for the exotic memory allocators (TLSF and Arena).
 * Integrates with the existing JDBX configuration system and provides
 * runtime control over allocator enablement.
 * 
 * Configuration Variables:
 * - JDBX_ENABLE_EXOTIC_ALLOCATORS: Master enable/disable (default: false)
 * - JDBX_ENABLE_ARENA_ALLOCATOR: Arena allocator control (default: true)
 * - JDBX_ENABLE_TLSF_ALLOCATOR: TLSF allocator control (default: true)
 * - JDBX_MEM_DEBUG: Debug output for allocator operations (existing)
 * - JDBX_FORCE_SYSTEM_MALLOC: Emergency fallback to system malloc (default: false)
 * 
 * Emergency Rollback:
 * - Single environment variable can disable all exotic allocators
 * - Runtime configuration updates supported
 * - Zero-downtime rollback capability
 */

#include "utils/memory_allocator_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>

/* Configuration state structure */
typedef struct {
    atomic_bool exotic_allocators_enabled;
    atomic_bool arena_allocator_enabled;
    atomic_bool tlsf_allocator_enabled;
    atomic_bool force_system_malloc;
    atomic_bool debug_enabled;
    atomic_bool initialized;
    pthread_mutex_t config_mutex;
} memory_allocator_config_t;

static memory_allocator_config_t g_mem_config = {0};

/**
 * ============================================================================
 * CONFIGURATION INITIALIZATION
 * ============================================================================
 */

/* Initialize memory allocator configuration */
int memory_allocator_config_init(void) {
    if (atomic_load(&g_mem_config.initialized)) {
        return 0; /* Already initialized */
    }
    
    /* Initialize mutex */
    if (pthread_mutex_init(&g_mem_config.config_mutex, NULL) != 0) {
        if (getenv("JDBX_MEM_DEBUG")) {
            fprintf(stderr, "Failed to initialize memory allocator config mutex\n");
        }
        return -1;
    }
    
    /* Load configuration from environment variables */
    memory_allocator_config_reload();
    
    atomic_store(&g_mem_config.initialized, true);
    
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "Memory allocator configuration initialized\n");
    }
    return 0;
}

/* Shutdown memory allocator configuration */
void memory_allocator_config_shutdown(void) {
    if (!atomic_load(&g_mem_config.initialized)) {
        return;
    }
    
    atomic_store(&g_mem_config.initialized, false);
    pthread_mutex_destroy(&g_mem_config.config_mutex);
    
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "Memory allocator configuration shutdown\n");
    }
}

/* Reload configuration from environment variables */
void memory_allocator_config_reload(void) {
    pthread_mutex_lock(&g_mem_config.config_mutex);
    
    /* Master enable/disable switch - CRITICAL for emergency rollback */
    const char* exotic_enabled = getenv("JDBX_ENABLE_EXOTIC_ALLOCATORS");
    bool enable_exotic = false;
    if (exotic_enabled) {
        enable_exotic = (strcmp(exotic_enabled, "true") == 0 || 
                        strcmp(exotic_enabled, "1") == 0 ||
                        strcmp(exotic_enabled, "yes") == 0);
    }
    atomic_store(&g_mem_config.exotic_allocators_enabled, enable_exotic);
    
    /* Individual allocator controls */
    const char* arena_enabled = getenv("JDBX_ENABLE_ARENA_ALLOCATOR");
    bool enable_arena = true; /* Default enabled when exotic is enabled */
    if (arena_enabled) {
        enable_arena = (strcmp(arena_enabled, "true") == 0 || 
                       strcmp(arena_enabled, "1") == 0 ||
                       strcmp(arena_enabled, "yes") == 0);
    }
    atomic_store(&g_mem_config.arena_allocator_enabled, enable_arena);
    
    const char* tlsf_enabled = getenv("JDBX_ENABLE_TLSF_ALLOCATOR");
    bool enable_tlsf = true; /* Default enabled when exotic is enabled */
    if (tlsf_enabled) {
        enable_tlsf = (strcmp(tlsf_enabled, "true") == 0 || 
                      strcmp(tlsf_enabled, "1") == 0 ||
                      strcmp(tlsf_enabled, "yes") == 0);
    }
    atomic_store(&g_mem_config.tlsf_allocator_enabled, enable_tlsf);
    
    /* Emergency system malloc fallback */
    const char* force_system = getenv("JDBX_FORCE_SYSTEM_MALLOC");
    bool force_malloc = false;
    if (force_system) {
        force_malloc = (strcmp(force_system, "true") == 0 || 
                       strcmp(force_system, "1") == 0 ||
                       strcmp(force_system, "yes") == 0);
    }
    atomic_store(&g_mem_config.force_system_malloc, force_malloc);
    
    /* Debug mode (existing JDBX_MEM_DEBUG) */
    const char* debug_enabled = getenv("JDBX_MEM_DEBUG");
    bool enable_debug = (debug_enabled != NULL);
    atomic_store(&g_mem_config.debug_enabled, enable_debug);
    
    pthread_mutex_unlock(&g_mem_config.config_mutex);
    
    /* Log configuration state if debug enabled */
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "Memory allocator configuration reloaded:\n");
        fprintf(stderr, "  Exotic allocators: %s\n", enable_exotic ? "enabled" : "disabled");
        fprintf(stderr, "  Arena allocator: %s\n", enable_arena ? "enabled" : "disabled");
        fprintf(stderr, "  TLSF allocator: %s\n", enable_tlsf ? "enabled" : "disabled");
        fprintf(stderr, "  Force system malloc: %s\n", force_malloc ? "yes" : "no");
        fprintf(stderr, "  Debug mode: %s\n", enable_debug ? "enabled" : "disabled");
    }
}

/**
 * ============================================================================
 * CONFIGURATION QUERIES
 * ============================================================================
 */

/* Check if exotic allocators are enabled */
bool memory_allocator_config_exotic_enabled(void) {
    if (!atomic_load(&g_mem_config.initialized)) {
        return false;
    }
    
    /* Emergency fallback check */
    if (atomic_load(&g_mem_config.force_system_malloc)) {
        return false;
    }
    
    return atomic_load(&g_mem_config.exotic_allocators_enabled);
}

/* Check if Arena allocator should be used */
bool memory_allocator_config_arena_enabled(void) {
    if (!memory_allocator_config_exotic_enabled()) {
        return false;
    }
    
    return atomic_load(&g_mem_config.arena_allocator_enabled);
}

/* Check if TLSF allocator should be used */
bool memory_allocator_config_tlsf_enabled(void) {
    if (!memory_allocator_config_exotic_enabled()) {
        return false;
    }
    
    return atomic_load(&g_mem_config.tlsf_allocator_enabled);
}

/* Check if debug mode is enabled */
bool memory_allocator_config_debug_enabled(void) {
    return atomic_load(&g_mem_config.debug_enabled);
}

/* Check if system malloc is forced */
bool memory_allocator_config_force_system_malloc(void) {
    return atomic_load(&g_mem_config.force_system_malloc);
}

/**
 * ============================================================================
 * RUNTIME CONFIGURATION UPDATES
 * ============================================================================
 */

/* Emergency disable all exotic allocators */
int memory_allocator_emergency_disable(void) {
    if (!atomic_load(&g_mem_config.initialized)) {
        return -1;
    }
    
    pthread_mutex_lock(&g_mem_config.config_mutex);
    
    /* Set emergency fallback */
    atomic_store(&g_mem_config.force_system_malloc, true);
    atomic_store(&g_mem_config.exotic_allocators_enabled, false);
    
    pthread_mutex_unlock(&g_mem_config.config_mutex);
    
    fprintf(stderr, "EMERGENCY: Exotic memory allocators disabled - falling back to system malloc\n");
    
    return 0;
}

/* Enable exotic allocators (requires explicit configuration) */
int memory_allocator_enable(bool arena, bool tlsf) {
    if (!atomic_load(&g_mem_config.initialized)) {
        return -1;
    }
    
    pthread_mutex_lock(&g_mem_config.config_mutex);
    
    /* Clear emergency fallback */
    atomic_store(&g_mem_config.force_system_malloc, false);
    
    /* Enable master switch */
    atomic_store(&g_mem_config.exotic_allocators_enabled, true);
    
    /* Set individual allocators */
    atomic_store(&g_mem_config.arena_allocator_enabled, arena);
    atomic_store(&g_mem_config.tlsf_allocator_enabled, tlsf);
    
    pthread_mutex_unlock(&g_mem_config.config_mutex);
    
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "Exotic memory allocators enabled (Arena: %s, TLSF: %s)\n", 
                arena ? "yes" : "no", tlsf ? "yes" : "no");
    }
    
    return 0;
}

/**
 * ============================================================================
 * CONFIGURATION STATUS REPORTING
 * ============================================================================
 */

/* Get detailed configuration status */
void memory_allocator_config_status(memory_allocator_status_t* status) {
    if (!status) {
        return;
    }
    
    memset(status, 0, sizeof(memory_allocator_status_t));
    
    if (!atomic_load(&g_mem_config.initialized)) {
        status->initialized = false;
        return;
    }
    
    status->initialized = true;
    status->exotic_enabled = atomic_load(&g_mem_config.exotic_allocators_enabled);
    status->arena_enabled = atomic_load(&g_mem_config.arena_allocator_enabled);
    status->tlsf_enabled = atomic_load(&g_mem_config.tlsf_allocator_enabled);
    status->force_system_malloc = atomic_load(&g_mem_config.force_system_malloc);
    status->debug_enabled = atomic_load(&g_mem_config.debug_enabled);
    
    /* Calculate effective status */
    status->arena_active = status->exotic_enabled && status->arena_enabled && !status->force_system_malloc;
    status->tlsf_active = status->exotic_enabled && status->tlsf_enabled && !status->force_system_malloc;
}

/* Print configuration status to log */
void memory_allocator_config_log_status(void) {
    memory_allocator_status_t status;
    memory_allocator_config_status(&status);
    
    if (!status.initialized) {
        fprintf(stderr, "Memory allocator configuration: NOT INITIALIZED\n");
        return;
    }
    
    fprintf(stderr, "=== Memory Allocator Configuration Status ===\n");
    fprintf(stderr, "Master exotic allocators: %s\n", status.exotic_enabled ? "ENABLED" : "DISABLED");
    fprintf(stderr, "Arena allocator: %s (active: %s)\n", 
            status.arena_enabled ? "enabled" : "disabled",
            status.arena_active ? "YES" : "NO");
    fprintf(stderr, "TLSF allocator: %s (active: %s)\n", 
            status.tlsf_enabled ? "enabled" : "disabled",
            status.tlsf_active ? "YES" : "NO");
    fprintf(stderr, "Force system malloc: %s\n", status.force_system_malloc ? "YES" : "NO");
    fprintf(stderr, "Debug mode: %s\n", status.debug_enabled ? "ENABLED" : "DISABLED");
    
    if (status.force_system_malloc) {
        fprintf(stderr, "WARNING: EMERGENCY MODE: All allocations using system malloc\n");
    } else if (!status.exotic_enabled) {
        fprintf(stderr, "Using system malloc (exotic allocators disabled)\n");
    } else {
        fprintf(stderr, "Using exotic allocators as configured\n");
    }
}

/**
 * ============================================================================
 * INTEGRATION WITH EXISTING CONFIG SYSTEM
 * ============================================================================
 */

/* Add memory allocator configuration to environment file */
int memory_allocator_config_write_env_template(const char* env_file) {
    FILE* fp = fopen(env_file, "a");
    if (!fp) {
        fprintf(stderr, "Failed to open environment file for writing: %s\n", env_file);
        return -1;
    }
    
    fprintf(fp, "\n# ====================================================================\n");
    fprintf(fp, "# MEMORY ALLOCATOR CONFIGURATION\n");
    fprintf(fp, "# ====================================================================\n\n");
    
    fprintf(fp, "# Master enable/disable for exotic memory allocators\n");
    fprintf(fp, "# Set to true to enable TLSF and Arena allocators for performance\n");
    fprintf(fp, "# Set to false for emergency rollback to system malloc\n");
    fprintf(fp, "# JDBX_ENABLE_EXOTIC_ALLOCATORS=false\n\n");
    
    fprintf(fp, "# Individual allocator controls (only effective when exotic allocators enabled)\n");
    fprintf(fp, "# JDBX_ENABLE_ARENA_ALLOCATOR=true   # Checkpoint-scoped allocations\n");
    fprintf(fp, "# JDBX_ENABLE_TLSF_ALLOCATOR=true    # General runtime allocations\n\n");
    
    fprintf(fp, "# Emergency fallback - forces system malloc regardless of other settings\n");
    fprintf(fp, "# Use this for immediate rollback in production emergencies\n");
    fprintf(fp, "# JDBX_FORCE_SYSTEM_MALLOC=false\n\n");
    
    fprintf(fp, "# Memory allocation debugging (generates verbose output)\n");
    fprintf(fp, "# JDBX_MEM_DEBUG=false\n\n");
    
    fclose(fp);
    
    if (getenv("JDBX_MEM_DEBUG")) {
        fprintf(stderr, "Memory allocator configuration template written to %s\n", env_file);
    }
    return 0;
}

/* Validate configuration consistency */
int memory_allocator_config_validate(void) {
    memory_allocator_status_t status;
    memory_allocator_config_status(&status);
    
    if (!status.initialized) {
        fprintf(stderr, "Memory allocator configuration not initialized\n");
        return -1;
    }
    
    /* Check for conflicting settings */
    if (status.force_system_malloc && status.exotic_enabled) {
        fprintf(stderr, "WARNING: Conflicting configuration: FORCE_SYSTEM_MALLOC=true but exotic allocators enabled\n");
        fprintf(stderr, "WARNING: System malloc will be used (emergency mode takes precedence)\n");
    }
    
    /* Validate that at least one allocator is available */
    if (status.exotic_enabled && !status.arena_enabled && !status.tlsf_enabled) {
        fprintf(stderr, "WARNING: Exotic allocators enabled but no individual allocators active\n");
        fprintf(stderr, "WARNING: Will fall back to system malloc\n");
    }
    
    return 0;
}