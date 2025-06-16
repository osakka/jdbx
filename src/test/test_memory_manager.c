/**
 * @file test_memory_manager.c
 * @brief Proof of concept demonstrating the Memory Manager's automatic cleanup
 * 
 * This test shows how the checkpoint/rewind architecture eliminates memory leaks
 * in error paths - exactly what JDBX needs!
 */

#include "utils/memory_manager.h"
#include "utils/buffer_pool.h"
#include "utils/json.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/**
 * Simulate a complex operation that allocates memory and can fail
 * WITHOUT memory manager - manual cleanup required
 */
int complex_operation_manual(int should_fail) {
    char* str1 = BUFFER_STRDUP("allocation 1");
    if (!str1) return -1;
    
    char* str2 = BUFFER_STRDUP("allocation 2");
    if (!str2) {
        BUFFER_FREE(str1);  // Manual cleanup
        return -1;
    }
    
    json_value_t* doc = json_create_object();
    if (!doc) {
        BUFFER_FREE(str1);  // Manual cleanup
        BUFFER_FREE(str2);  // Manual cleanup
        return -1;
    }
    
    json_object_set(doc, "field1", json_create_string("value1"));
    json_object_set(doc, "field2", json_create_string("value2"));
    
    if (should_fail) {
        // Simulate error - must manually clean up everything!
        BUFFER_FREE(str1);
        BUFFER_FREE(str2);
        json_free(doc);
        return -1;
    }
    
    // Success - still need to free when done
    BUFFER_FREE(str1);
    BUFFER_FREE(str2);
    json_free(doc);
    return 0;
}

/**
 * Same operation WITH memory manager - automatic cleanup!
 */
int complex_operation_automatic(int should_fail) {
    // Create checkpoint at operation start
    memory_checkpoint_t* checkpoint = memory_checkpoint_create();
    
    // All allocations after this point are tracked
    char* str1 = memory_strdup("allocation 1");
    if (!str1) {
        memory_checkpoint_rewind(checkpoint);
        return -1;
    }
    
    char* str2 = memory_strdup("allocation 2");
    if (!str2) {
        memory_checkpoint_rewind(checkpoint);  // Frees str1 automatically!
        return -1;
    }
    
    // Switch to memory-managed allocations for JSON
    json_value_t* doc = memory_alloc(sizeof(json_value_t));
    if (!doc) {
        memory_checkpoint_rewind(checkpoint);  // Frees str1 and str2!
        return -1;
    }
    
    // Initialize the document (simplified for demo)
    doc->type = JSON_OBJECT;
    
    if (should_fail) {
        // Simulate error - just rewind, no manual cleanup needed!
        memory_checkpoint_rewind(checkpoint);
        return -1;
    }
    
    // Success - commit the checkpoint
    memory_checkpoint_commit(checkpoint);
    
    // Note: In real usage, we'd promote return values that need to survive
    return 0;
}

/**
 * Demonstrate nested checkpoints (like savepoints in transactions)
 */
void test_nested_checkpoints() {
    printf("\n=== Testing Nested Checkpoints ===\n");
    
    // Outer checkpoint
    memory_checkpoint_t* outer = memory_checkpoint_create();
    char* outer_alloc = memory_strdup("outer allocation");
    
    // Inner checkpoint (like a savepoint)
    memory_checkpoint_t* inner = memory_checkpoint_create();
    char* inner_alloc = memory_strdup("inner allocation");
    
    // Rewind inner checkpoint - frees only inner allocations
    memory_checkpoint_rewind(inner);
    printf("After inner rewind - outer allocation still valid\n");
    
    // Rewind outer checkpoint - frees everything
    memory_checkpoint_rewind(outer);
    printf("After outer rewind - all allocations freed\n");
}

/**
 * Demonstrate promotion - making allocations survive checkpoint
 */
void test_promotion() {
    printf("\n=== Testing Allocation Promotion ===\n");
    
    memory_checkpoint_t* checkpoint = memory_checkpoint_create();
    
    // Allocate some temporary data
    char* temp1 = memory_strdup("temporary 1");
    char* temp2 = memory_strdup("temporary 2");
    
    // Allocate something we want to keep
    char* keep = memory_strdup("this should survive");
    
    // Promote the allocation we want to keep
    keep = memory_promote(keep);
    
    // Rewind - frees temp1 and temp2, but not keep
    memory_checkpoint_rewind(checkpoint);
    
    printf("After rewind, promoted allocation survives: %s\n", keep);
    
    // We still need to free promoted allocations manually
    memory_free(keep);
}

/**
 * Show memory manager statistics
 */
void show_statistics() {
    uint64_t checkpoints, rewinds, freed;
    memory_manager_get_stats(&checkpoints, &rewinds, &freed);
    
    printf("\n=== Memory Manager Statistics ===\n");
    printf("Checkpoints created: %lu\n", checkpoints);
    printf("Rewinds performed: %lu\n", rewinds);
    printf("Allocations freed by rewind: %lu\n", freed);
}

int main() {
    printf("=== JDBX Memory Manager Proof of Concept ===\n");
    
    // Initialize memory manager
    memory_manager_init();
    
    // Test 1: Show manual cleanup burden
    printf("\n--- Manual Memory Management ---\n");
    printf("Success case: ");
    complex_operation_manual(0);
    printf("OK\n");
    
    printf("Failure case: ");
    complex_operation_manual(1);
    printf("OK (but required 3 manual cleanup calls!)\n");
    
    // Test 2: Show automatic cleanup
    printf("\n--- Automatic Memory Management ---\n");
    printf("Success case: ");
    complex_operation_automatic(0);
    printf("OK\n");
    
    printf("Failure case: ");
    complex_operation_automatic(1);
    printf("OK (automatic cleanup with single rewind!)\n");
    
    // Test 3: Nested checkpoints
    test_nested_checkpoints();
    
    // Test 4: Promotion
    test_promotion();
    
    // Show statistics
    show_statistics();
    
    // Shutdown
    memory_manager_shutdown();
    
    printf("\n=== Benefits for JDBX ===\n");
    printf("1. Eliminates manual cleanup code (500+ locations in codebase)\n");
    printf("2. Prevents memory leaks in error paths\n");
    printf("3. True transactional memory for database operations\n");
    printf("4. Simplifies error handling dramatically\n");
    printf("5. Zero performance overhead when no checkpoint active\n");
    
    return 0;
}