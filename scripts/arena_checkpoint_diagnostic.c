/*
 * Arena Checkpoint Diagnostic Tool
 * Check if Arena creation in checkpoints is failing
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* Simulate the Arena checkpoint creation issue */

/* CRITICAL DISCOVERY SIMULATION */
void arena_checkpoint_diagnostic() {
    printf("🔬 Arena Checkpoint Diagnostic\n");
    printf("==============================\n");
    
    printf("🧪 Simulating Arena checkpoint creation...\n");
    
    /* Test 1: Arena creation with 4MB */
    printf("📋 Test 1: arena_create(4MB)...\n");
    void* arena = malloc(4 * 1024 * 1024);  /* Simulate arena creation */
    if (arena) {
        printf("   ✅ Arena memory allocated: %p\n", arena);
    } else {
        printf("   ❌ Arena memory allocation FAILED\n");
        return;
    }
    
    /* Test 2: Check available memory */
    printf("📋 Test 2: System memory check...\n");
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[256];
        while (fgets(line, sizeof(line), meminfo)) {
            if (strncmp(line, "MemAvailable:", 13) == 0) {
                printf("   📊 %s", line);
                break;
            }
        }
        fclose(meminfo);
    }
    
    /* Test 3: Multiple 4MB arena allocations */
    printf("📋 Test 3: Multiple arena allocations...\n");
    void* arenas[10];
    int successful = 0;
    
    for (int i = 0; i < 10; i++) {
        arenas[i] = malloc(4 * 1024 * 1024);
        if (arenas[i]) {
            successful++;
            printf("   ✅ Arena %d allocated: %p\n", i+1, arenas[i]);
        } else {
            printf("   ❌ Arena %d allocation FAILED\n", i+1);
            break;
        }
    }
    
    printf("📊 Successfully allocated %d/10 arenas\n", successful);
    
    /* Test 4: Aligned allocation (as used in arena_allocator.c) */
    printf("📋 Test 4: Aligned allocation test...\n");
    size_t total_size = sizeof(void*) + (4 * 1024 * 1024);  /* Simulate arena_t + memory */
    size_t aligned_total = (total_size + 64 - 1) & ~(64 - 1);
    
    void* aligned_arena = aligned_alloc(64, aligned_total);
    if (aligned_arena) {
        printf("   ✅ Aligned arena allocated: %p (size: %zu)\n", aligned_arena, aligned_total);
        free(aligned_arena);
    } else {
        printf("   ❌ Aligned arena allocation FAILED\n");
    }
    
    /* Cleanup */
    free(arena);
    for (int i = 0; i < successful; i++) {
        free(arenas[i]);
    }
    
    printf("\n🎯 CRITICAL HYPOTHESIS:\n");
    printf("   If aligned_alloc() fails in arena_create(), then:\n");
    printf("   → checkpoint->arena = NULL\n");
    printf("   → should_use_arena_allocator() returns false\n");
    printf("   → Arena allocator never actually gets used\n");
    printf("   → But no error is reported!\n");
    
    printf("\n✅ Arena checkpoint diagnostic complete\n");
}

int main() {
    arena_checkpoint_diagnostic();
    return 0;
}