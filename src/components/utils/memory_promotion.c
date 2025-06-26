/**
 * @file memory_promotion.c
 * @brief Memory Lifetime Management and Promotion System
 * 
 * Provides intelligent memory promotion capabilities for objects that
 * outlive their original allocation context (e.g., Arena objects that
 * need to survive checkpoint rewind).
 */

#include "utils/memory_types.h"
#include "utils/memory_manager.h"
#include "utils/memory_allocator_config.h"
#include "utils/tlsf_allocator.h"
#include "utils/arena_allocator.h"
#include <string.h>
#include <stdio.h>

/**
 * Promotion Statistics
 */
typedef struct {
    uint64_t arena_to_tlsf_promotions;
    uint64_t arena_to_system_promotions;
    uint64_t tlsf_to_system_promotions;
    uint64_t promotion_failures;
    uint64_t bytes_promoted;
} promotion_stats_t;

static promotion_stats_t g_promotion_stats = {0};

/**
 * Promote Arena allocation to TLSF for lifetime extension
 */
void* memory_promote_arena_to_tlsf(void* arena_ptr, size_t size) {
    if (!arena_ptr || !size) {
        return NULL;
    }
    
    /* Verify this is an arena allocation */
    memory_header_t* arena_header = get_memory_header(arena_ptr);
    if (!arena_header || !(arena_header->flags & MEMORY_FLAG_ARENA_ALLOCATED)) {
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "memory_promote_arena_to_tlsf: Not an arena allocation\n");
        }
        return NULL;
    }
    
    /* Allocate in TLSF */
    void* tlsf_ptr = NULL;
    if (should_use_tlsf_allocator(size)) {
        tlsf_ptr = memory_alloc(size);
        if (tlsf_ptr) {
            /* Copy data from arena to TLSF */
            memcpy(tlsf_ptr, arena_ptr, size);
            
            /* Update statistics */
            __sync_fetch_and_add(&g_promotion_stats.arena_to_tlsf_promotions, 1);
            __sync_fetch_and_add(&g_promotion_stats.bytes_promoted, size);
            
            if (SHOULD_DEBUG_MEMORY()) {
                fprintf(stderr, "memory_promote_arena_to_tlsf: Promoted %zu bytes from arena to TLSF\n", size);
            }
            
            return tlsf_ptr;
        }
    }
    
    /* Fallback to system allocation */
    void* system_ptr = memory_alloc(size);
    if (system_ptr) {
        memcpy(system_ptr, arena_ptr, size);
        
        __sync_fetch_and_add(&g_promotion_stats.arena_to_system_promotions, 1);
        __sync_fetch_and_add(&g_promotion_stats.bytes_promoted, size);
        
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "memory_promote_arena_to_tlsf: Promoted %zu bytes from arena to system\n", size);
        }
        
        return system_ptr;
    }
    
    /* Promotion failed */
    __sync_fetch_and_add(&g_promotion_stats.promotion_failures, 1);
    return NULL;
}

/**
 * Promote TLSF allocation to system malloc (for very long-lived objects)
 */
void* memory_promote_tlsf_to_system(void* tlsf_ptr, size_t size) {
    if (!tlsf_ptr || !size) {
        return NULL;
    }
    
    /* Verify this is a TLSF allocation */
    memory_header_t* tlsf_header = get_memory_header(tlsf_ptr);
    if (!tlsf_header || !(tlsf_header->flags & MEMORY_FLAG_TLSF_ALLOCATED)) {
        if (SHOULD_DEBUG_MEMORY()) {
            fprintf(stderr, "memory_promote_tlsf_to_system: Not a TLSF allocation\n");
        }
        return NULL;
    }
    
    /* Allocate in system malloc */
    void* system_ptr = memory_alloc(size);
    if (system_ptr) {
        /* Verify system allocation */
        memory_header_t* system_header = get_memory_header(system_ptr);
        if (system_header && !(system_header->flags & (MEMORY_FLAG_ARENA_ALLOCATED | MEMORY_FLAG_TLSF_ALLOCATED))) {
            /* Copy data */
            memcpy(system_ptr, tlsf_ptr, size);
            
            /* Free original TLSF allocation */
            memory_free(tlsf_ptr);
            
            /* Update statistics */
            __sync_fetch_and_add(&g_promotion_stats.tlsf_to_system_promotions, 1);
            __sync_fetch_and_add(&g_promotion_stats.bytes_promoted, size);
            
            if (SHOULD_DEBUG_MEMORY()) {
                fprintf(stderr, "memory_promote_tlsf_to_system: Promoted %zu bytes from TLSF to system\n", size);
            }
            
            return system_ptr;
        }
    }
    
    /* Promotion failed */
    __sync_fetch_and_add(&g_promotion_stats.promotion_failures, 1);
    return NULL;
}

/**
 * Automatic promotion based on allocation lifetime heuristics
 */
void* memory_auto_promote(void* ptr, size_t size, int lifetime_hint) {
    if (!ptr || !size) {
        return NULL;
    }
    
    memory_header_t* header = get_memory_header(ptr);
    if (!header) {
        return ptr; /* Not a managed allocation */
    }
    
    /* Promotion decision based on current allocator and lifetime hint */
    if (header->flags & MEMORY_FLAG_ARENA_ALLOCATED) {
        /* Arena allocations should be promoted if they need to survive checkpoint */
        if (lifetime_hint > 0) { /* Lifetime hint: 0=checkpoint, 1=short-term, 2=long-term */
            return memory_promote_arena_to_tlsf(ptr, size);
        }
    } else if (header->flags & MEMORY_FLAG_TLSF_ALLOCATED) {
        /* TLSF allocations should be promoted to system for very long-lived objects */
        if (lifetime_hint >= 2) {
            return memory_promote_tlsf_to_system(ptr, size);
        }
    }
    
    /* No promotion needed */
    return ptr;
}

/**
 * Batch promotion for checkpoint commit operations
 */
int memory_promote_checkpoint_survivors(memory_checkpoint_t* checkpoint) {
    if (!checkpoint) {
        return -1;
    }
    
    int promoted_count = 0;
    memory_header_t* header = checkpoint->first_alloc;
    
    while (header) {
        memory_header_t* next = header->next;
        
        /* Only promote arena allocations that are marked for survival */
        if ((header->flags & MEMORY_FLAG_ARENA_ALLOCATED) && 
            (header->flags & MEMORY_FLAG_HAZARD_PROTECTED)) {
            
            void* user_ptr = (char*)header + HEADER_SIZE;
            void* promoted_ptr = memory_promote_arena_to_tlsf(user_ptr, header->size);
            
            if (promoted_ptr) {
                /* Update any references to point to promoted allocation */
                /* This is application-specific and would need callback mechanism */
                promoted_count++;
            }
        }
        
        header = next;
    }
    
    if (SHOULD_DEBUG_MEMORY() && promoted_count > 0) {
        fprintf(stderr, "memory_promote_checkpoint_survivors: Promoted %d allocations\n", promoted_count);
    }
    
    return promoted_count;
}

/**
 * Get promotion statistics
 */
void memory_get_promotion_stats(promotion_stats_t* stats) {
    if (!stats) {
        return;
    }
    
    stats->arena_to_tlsf_promotions = g_promotion_stats.arena_to_tlsf_promotions;
    stats->arena_to_system_promotions = g_promotion_stats.arena_to_system_promotions;
    stats->tlsf_to_system_promotions = g_promotion_stats.tlsf_to_system_promotions;
    stats->promotion_failures = g_promotion_stats.promotion_failures;
    stats->bytes_promoted = g_promotion_stats.bytes_promoted;
}

/**
 * Reset promotion statistics
 */
void memory_reset_promotion_stats(void) {
    memset(&g_promotion_stats, 0, sizeof(promotion_stats_t));
}

/**
 * Log promotion statistics
 */
void memory_log_promotion_stats(void) {
    fprintf(stderr, "=== Memory Promotion Statistics ===\n");
    fprintf(stderr, "Arena → TLSF promotions: %lu\n", g_promotion_stats.arena_to_tlsf_promotions);
    fprintf(stderr, "Arena → System promotions: %lu\n", g_promotion_stats.arena_to_system_promotions);
    fprintf(stderr, "TLSF → System promotions: %lu\n", g_promotion_stats.tlsf_to_system_promotions);
    fprintf(stderr, "Promotion failures: %lu\n", g_promotion_stats.promotion_failures);
    fprintf(stderr, "Total bytes promoted: %lu\n", g_promotion_stats.bytes_promoted);
    
    uint64_t total_promotions = g_promotion_stats.arena_to_tlsf_promotions + 
                               g_promotion_stats.arena_to_system_promotions + 
                               g_promotion_stats.tlsf_to_system_promotions;
    
    if (total_promotions > 0) {
        double avg_promotion_size = (double)g_promotion_stats.bytes_promoted / total_promotions;
        fprintf(stderr, "Average promotion size: %.1f bytes\n", avg_promotion_size);
        
        if (g_promotion_stats.promotion_failures > 0) {
            double failure_rate = (double)g_promotion_stats.promotion_failures / 
                                 (total_promotions + g_promotion_stats.promotion_failures) * 100.0;
            fprintf(stderr, "Promotion failure rate: %.1f%%\n", failure_rate);
        }
    }
}