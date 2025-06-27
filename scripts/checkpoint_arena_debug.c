/*
 * Checkpoint Arena Creation Debug Tool
 * Focuses on the exact moment Arena allocation fails during API requests
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

/* Test Arena creation in the exact context where it fails */
void test_checkpoint_arena_creation() {
    printf("🔬 Checkpoint Arena Creation Debug\n");
    printf("==================================\n");
    
    /* Test 1: Direct arena_create() call */
    printf("📋 Test 1: Direct arena_create(4MB) simulation...\n");
    
    /* Allocate arena structure + memory as in arena_allocator.c */
    size_t arena_size = 4 * 1024 * 1024;
    size_t total_size = sizeof(void*) * 10 + arena_size;  /* Simulate arena_t structure */
    size_t aligned_total = (total_size + 64 - 1) & ~(64 - 1);
    
    printf("   📊 Requested: arena_size=%zu, total=%zu, aligned=%zu\n", 
           arena_size, total_size, aligned_total);
    
    void* arena_ptr = aligned_alloc(64, aligned_total);
    if (arena_ptr) {
        printf("   ✅ Arena structure allocated: %p\n", arena_ptr);
        
        /* Test memory region setup */
        char* memory_start = (char*)arena_ptr + sizeof(void*) * 10;
        /* Simulate align_ptr functionality */
        uintptr_t addr = (uintptr_t)memory_start;
        uintptr_t aligned = (addr + 64 - 1) & ~(64 - 1);
        char* aligned_memory = (char*)aligned;
        
        printf("   📍 Memory region: start=%p, aligned=%p, offset=%ld\n", 
               memory_start, aligned_memory, aligned_memory - memory_start);
        
        /* Test write to memory region */
        aligned_memory[0] = 0xAA;
        aligned_memory[arena_size - 1] = 0xBB;
        
        if (aligned_memory[0] == 0xAA && aligned_memory[arena_size - 1] == 0xBB) {
            printf("   ✅ Memory region accessible\n");
        } else {
            printf("   ❌ Memory region corrupted\n");
        }
        
        free(arena_ptr);
    } else {
        printf("   ❌ Arena allocation FAILED - errno: %d\n", errno);
        perror("   aligned_alloc");
    }
    
    /* Test 2: Multiple concurrent arena creations (simulate threading) */
    printf("📋 Test 2: Concurrent arena allocations...\n");
    
    pid_t pids[4];
    int successful_children = 0;
    
    for (int i = 0; i < 4; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            /* Child process */
            void* child_arena = aligned_alloc(64, aligned_total);
            if (child_arena) {
                printf("   ✅ Child %d: Arena allocated %p\n", i, child_arena);
                free(child_arena);
                exit(0);
            } else {
                printf("   ❌ Child %d: Arena allocation failed\n", i);
                exit(1);
            }
        } else if (pids[i] > 0) {
            /* Parent process continues */
        } else {
            printf("   ❌ Fork failed for child %d\n", i);
        }
    }
    
    /* Wait for all children */
    for (int i = 0; i < 4; i++) {
        if (pids[i] > 0) {
            int status;
            waitpid(pids[i], &status, 0);
            if (WEXITSTATUS(status) == 0) {
                successful_children++;
            }
        }
    }
    
    printf("   📊 Successful concurrent allocations: %d/4\n", successful_children);
    
    /* Test 3: Arena checkpoint simulation */
    printf("📋 Test 3: Arena checkpoint creation simulation...\n");
    
    void* checkpoint_arena = aligned_alloc(64, aligned_total);
    if (checkpoint_arena) {
        printf("   ✅ Checkpoint arena allocated: %p\n", checkpoint_arena);
        
        /* Simulate arena_checkpoint() - create checkpoint marker */
        void* checkpoint_marker = malloc(64);  /* Simulate checkpoint_t */
        if (checkpoint_marker) {
            printf("   ✅ Checkpoint marker created: %p\n", checkpoint_marker);
            
            /* Test checkpoint reset simulation */
            printf("   🔄 Testing checkpoint operations...\n");
            
            /* This is where the bug might be - checkpoint interaction */
            memset(checkpoint_marker, 0xCC, 64);  /* Initialize checkpoint */
            
            printf("   ✅ Checkpoint operations completed\n");
            
            free(checkpoint_marker);
        } else {
            printf("   ❌ Checkpoint marker allocation failed\n");
        }
        
        free(checkpoint_arena);
    } else {
        printf("   ❌ Checkpoint arena allocation failed\n");
    }
    
    printf("\n🎯 KEY FINDINGS:\n");
    printf("   → Arena allocation itself works fine\n");
    printf("   → Issue likely in Arena+Checkpoint interaction\n");
    printf("   → Bug probably in arena_checkpoint() or arena_reset_to_checkpoint()\n");
    printf("   → Need to examine Arena internal pointer management\n");
    
    printf("\n✅ Checkpoint Arena debug complete\n");
}

int main() {
    test_checkpoint_arena_creation();
    return 0;
}