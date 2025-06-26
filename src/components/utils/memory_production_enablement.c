/**
 * @file memory_production_enablement.c
 * @brief Atomic Production Enablement System for Memory Allocators
 * 
 * Provides zero-downtime activation, validation, and emergency rollback
 * capabilities for the exotic memory allocators in production environments.
 */

#include "utils/memory_allocator_config.h"
#include "utils/memory_manager.h"
#include "utils/tlsf_allocator.h"
#include "utils/arena_allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

/* Production enablement states */
typedef enum {
    PRODUCTION_STATE_DISABLED = 0,
    PRODUCTION_STATE_VALIDATING = 1,
    PRODUCTION_STATE_ENABLING = 2,
    PRODUCTION_STATE_ENABLED = 3,
    PRODUCTION_STATE_EMERGENCY = 4
} production_state_t;

/* Production enablement configuration */
typedef struct {
    production_state_t state;
    time_t enablement_time;
    time_t validation_start_time;
    uint32_t validation_duration_seconds;
    uint32_t allocation_test_count;
    uint32_t successful_allocations;
    uint32_t failed_allocations;
    uint32_t emergency_triggers;
    pthread_mutex_t state_mutex;
    int auto_rollback_enabled;
} production_enablement_t;

static production_enablement_t g_production = {
    .state = PRODUCTION_STATE_DISABLED,
    .validation_duration_seconds = 60, /* 1 minute validation by default */
    .allocation_test_count = 1000,     /* Test allocation count */
    .auto_rollback_enabled = 1,
    .state_mutex = PTHREAD_MUTEX_INITIALIZER
};

/**
 * ============================================================================
 * VALIDATION FRAMEWORK
 * ============================================================================
 */

/* Validate Arena allocator functionality */
static int validate_arena_allocator(void) {
    if (!memory_allocator_config_arena_enabled()) {
        return 1; /* Skip validation if disabled */
    }
    
    memory_checkpoint_t* cp = memory_checkpoint_create();
    if (!cp) {
        fprintf(stderr, "Arena validation: Failed to create checkpoint\n");
        return 0;
    }
    
    /* Test Arena allocations */
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = memory_alloc(64 + i * 32);
        if (!ptrs[i]) {
            fprintf(stderr, "Arena validation: Allocation %d failed\n", i);
            memory_checkpoint_commit(cp);
            return 0;
        }
        
        /* Write test pattern */
        memset(ptrs[i], 0xA0 + i, 64 + i * 32);
    }
    
    /* Verify patterns */
    for (int i = 0; i < 10; i++) {
        if (((char*)ptrs[i])[0] != (char)(0xA0 + i)) {
            fprintf(stderr, "Arena validation: Data corruption in allocation %d\n", i);
            memory_checkpoint_commit(cp);
            return 0;
        }
    }
    
    /* Test checkpoint rewind */
    memory_checkpoint_rewind(cp);
    memory_checkpoint_commit(cp);
    
    return 1; /* Arena validation passed */
}

/* Validate TLSF allocator functionality */
static int validate_tlsf_allocator(void) {
    if (!memory_allocator_config_tlsf_enabled()) {
        return 1; /* Skip validation if disabled */
    }
    
    /* Disable Arena to force TLSF usage */
    setenv("JDBX_ENABLE_ARENA_ALLOCATOR", "false", 1);
    memory_allocator_config_reload();
    
    /* Test TLSF allocations */
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        size_t size = 128 + i * 64;
        ptrs[i] = memory_alloc(size);
        if (!ptrs[i]) {
            fprintf(stderr, "TLSF validation: Allocation %d failed (size=%zu)\n", i, size);
            return 0;
        }
        
        /* Write test pattern */
        memset(ptrs[i], 0xB0 + i, size);
    }
    
    /* Verify patterns */
    for (int i = 0; i < 10; i++) {
        if (((char*)ptrs[i])[0] != (char)(0xB0 + i)) {
            fprintf(stderr, "TLSF validation: Data corruption in allocation %d\n", i);
            return 0;
        }
    }
    
    /* Free all allocations */
    for (int i = 0; i < 10; i++) {
        memory_free(ptrs[i]);
    }
    
    /* Re-enable Arena */
    setenv("JDBX_ENABLE_ARENA_ALLOCATOR", "true", 1);
    memory_allocator_config_reload();
    
    return 1; /* TLSF validation passed */
}

/* Comprehensive allocator validation */
static int validate_allocator_system(void) {
    fprintf(stderr, "Starting allocator system validation...\n");
    
    /* Validate configuration system */
    if (!memory_allocator_config_exotic_enabled()) {
        fprintf(stderr, "Validation: Exotic allocators not enabled\n");
        return 0;
    }
    
    /* Validate Arena allocator */
    if (!validate_arena_allocator()) {
        fprintf(stderr, "Validation: Arena allocator failed\n");
        return 0;
    }
    
    /* Validate TLSF allocator */
    if (!validate_tlsf_allocator()) {
        fprintf(stderr, "Validation: TLSF allocator failed\n");
        return 0;
    }
    
    /* Stress test with mixed allocations */
    for (uint32_t i = 0; i < g_production.allocation_test_count; i++) {
        size_t size = 16 + (rand() % 4080); /* 16 to 4096 bytes */
        void* ptr = memory_alloc(size);
        if (ptr) {
            g_production.successful_allocations++;
            memory_free(ptr);
        } else {
            g_production.failed_allocations++;
        }
    }
    
    /* Check failure rate */
    if (g_production.failed_allocations > 0) {
        double failure_rate = (double)g_production.failed_allocations / 
                             g_production.allocation_test_count * 100.0;
        if (failure_rate > 1.0) { /* More than 1% failure rate */
            fprintf(stderr, "Validation: High allocation failure rate: %.1f%%\n", failure_rate);
            return 0;
        }
    }
    
    fprintf(stderr, "Allocator system validation completed successfully\n");
    fprintf(stderr, "  Successful allocations: %u\n", g_production.successful_allocations);
    fprintf(stderr, "  Failed allocations: %u\n", g_production.failed_allocations);
    
    return 1;
}

/**
 * ============================================================================
 * ATOMIC ENABLEMENT SYSTEM
 * ============================================================================
 */

/* Start validation process */
int memory_production_start_validation(void) {
    pthread_mutex_lock(&g_production.state_mutex);
    
    if (g_production.state != PRODUCTION_STATE_DISABLED) {
        pthread_mutex_unlock(&g_production.state_mutex);
        return -1; /* Already in progress */
    }
    
    g_production.state = PRODUCTION_STATE_VALIDATING;
    g_production.validation_start_time = time(NULL);
    g_production.successful_allocations = 0;
    g_production.failed_allocations = 0;
    
    pthread_mutex_unlock(&g_production.state_mutex);
    
    fprintf(stderr, "Starting exotic allocator validation process...\n");
    return 0;
}

/* Perform validation and atomic enablement */
int memory_production_validate_and_enable(void) {
    pthread_mutex_lock(&g_production.state_mutex);
    
    if (g_production.state != PRODUCTION_STATE_VALIDATING) {
        pthread_mutex_unlock(&g_production.state_mutex);
        return -1; /* Not in validation state */
    }
    
    g_production.state = PRODUCTION_STATE_ENABLING;
    pthread_mutex_unlock(&g_production.state_mutex);
    
    /* Perform comprehensive validation */
    int validation_result = validate_allocator_system();
    
    pthread_mutex_lock(&g_production.state_mutex);
    
    if (validation_result) {
        /* Validation successful - enable for production */
        g_production.state = PRODUCTION_STATE_ENABLED;
        g_production.enablement_time = time(NULL);
        
        fprintf(stderr, "Exotic allocators ENABLED for production use\n");
        fprintf(stderr, "  Validation duration: %ld seconds\n", 
                g_production.enablement_time - g_production.validation_start_time);
        
        pthread_mutex_unlock(&g_production.state_mutex);
        return 0;
    } else {
        /* Validation failed - emergency disable */
        g_production.state = PRODUCTION_STATE_EMERGENCY;
        g_production.emergency_triggers++;
        
        /* Emergency rollback */
        memory_allocator_emergency_disable();
        
        fprintf(stderr, "EMERGENCY: Allocator validation FAILED - rolled back to system malloc\n");
        
        pthread_mutex_unlock(&g_production.state_mutex);
        return -1;
    }
}

/* Emergency disable with immediate rollback */
int memory_production_emergency_disable(void) {
    pthread_mutex_lock(&g_production.state_mutex);
    
    production_state_t previous_state = g_production.state;
    g_production.state = PRODUCTION_STATE_EMERGENCY;
    g_production.emergency_triggers++;
    
    pthread_mutex_unlock(&g_production.state_mutex);
    
    /* Perform emergency rollback */
    memory_allocator_emergency_disable();
    
    fprintf(stderr, "EMERGENCY DISABLE: Exotic allocators disabled immediately\n");
    fprintf(stderr, "  Previous state: %d\n", previous_state);
    fprintf(stderr, "  Emergency triggers: %u\n", g_production.emergency_triggers);
    
    return 0;
}

/* Reset to disabled state */
int memory_production_reset(void) {
    pthread_mutex_lock(&g_production.state_mutex);
    
    g_production.state = PRODUCTION_STATE_DISABLED;
    g_production.enablement_time = 0;
    g_production.validation_start_time = 0;
    g_production.successful_allocations = 0;
    g_production.failed_allocations = 0;
    
    pthread_mutex_unlock(&g_production.state_mutex);
    
    fprintf(stderr, "Production enablement system reset to disabled state\n");
    return 0;
}

/**
 * ============================================================================
 * MONITORING AND STATUS
 * ============================================================================
 */

/* Get current production state */
production_state_t memory_production_get_state(void) {
    pthread_mutex_lock(&g_production.state_mutex);
    production_state_t state = g_production.state;
    pthread_mutex_unlock(&g_production.state_mutex);
    return state;
}

/* Get detailed production status */
void memory_production_get_status(void) {
    pthread_mutex_lock(&g_production.state_mutex);
    
    fprintf(stderr, "=== Production Enablement Status ===\n");
    
    const char* state_names[] = {
        "DISABLED", "VALIDATING", "ENABLING", "ENABLED", "EMERGENCY"
    };
    
    fprintf(stderr, "Current state: %s\n", state_names[g_production.state]);
    
    if (g_production.enablement_time > 0) {
        time_t uptime = time(NULL) - g_production.enablement_time;
        fprintf(stderr, "Enabled since: %ld seconds ago\n", uptime);
    }
    
    if (g_production.validation_start_time > 0) {
        time_t validation_time = time(NULL) - g_production.validation_start_time;
        fprintf(stderr, "Validation duration: %ld seconds\n", validation_time);
    }
    
    fprintf(stderr, "Successful allocations: %u\n", g_production.successful_allocations);
    fprintf(stderr, "Failed allocations: %u\n", g_production.failed_allocations);
    fprintf(stderr, "Emergency triggers: %u\n", g_production.emergency_triggers);
    fprintf(stderr, "Auto-rollback: %s\n", g_production.auto_rollback_enabled ? "enabled" : "disabled");
    
    pthread_mutex_unlock(&g_production.state_mutex);
}

/* Health check for monitoring systems */
int memory_production_health_check(void) {
    production_state_t state = memory_production_get_state();
    
    switch (state) {
        case PRODUCTION_STATE_ENABLED:
            return 0; /* Healthy */
        case PRODUCTION_STATE_EMERGENCY:
            return 2; /* Critical - emergency state */
        case PRODUCTION_STATE_VALIDATING:
        case PRODUCTION_STATE_ENABLING:
            return 1; /* Warning - transitional state */
        case PRODUCTION_STATE_DISABLED:
        default:
            return 1; /* Warning - not enabled */
    }
}

/**
 * ============================================================================
 * CONFIGURATION INTEGRATION
 * ============================================================================
 */

/* Configure validation parameters */
void memory_production_configure(uint32_t validation_duration, uint32_t test_count, int auto_rollback) {
    pthread_mutex_lock(&g_production.state_mutex);
    
    g_production.validation_duration_seconds = validation_duration;
    g_production.allocation_test_count = test_count;
    g_production.auto_rollback_enabled = auto_rollback;
    
    pthread_mutex_unlock(&g_production.state_mutex);
    
    fprintf(stderr, "Production enablement configured:\n");
    fprintf(stderr, "  Validation duration: %u seconds\n", validation_duration);
    fprintf(stderr, "  Test allocation count: %u\n", test_count);
    fprintf(stderr, "  Auto-rollback: %s\n", auto_rollback ? "enabled" : "disabled");
}