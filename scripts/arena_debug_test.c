#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

/* Arena constants */
#define ARENA_DEFAULT_SIZE      (4 * 1024 * 1024)  /* 4MB default */
#define ARENA_ALIGNMENT         16                   /* 16-byte alignment */

/* Arena structure (simplified) */
typedef struct arena_t {
    char* memory;
    size_t size;
    char* current;
    struct arena_t* next;
} arena_t;

/* Utility: align pointer */
static inline char* align_ptr(char* ptr, size_t alignment) {
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return (char*)aligned;
}

/* Test arena creation step by step */
int main() {
    printf("🔬 Arena Allocator Debug Test\n");
    printf("=============================\n");
    
    size_t size = ARENA_DEFAULT_SIZE;
    printf("1. Requested arena size: %zu bytes (%.2f MB)\n", size, size / (1024.0 * 1024.0));
    
    /* Test step 1: Calculate sizes */
    size_t total_size = sizeof(arena_t) + size;
    printf("2. Total size (struct + memory): %zu bytes\n", total_size);
    
    size_t aligned_total = (total_size + 64 - 1) & ~(64 - 1);
    printf("3. 64-byte aligned total: %zu bytes\n", aligned_total);
    
    /* Test step 2: Try aligned_alloc */
    printf("4. Attempting aligned_alloc(64, %zu)...\n", aligned_total);
    arena_t* arena = (arena_t*)aligned_alloc(64, aligned_total);
    if (!arena) {
        printf("❌ FAILED: aligned_alloc returned NULL\n");
        printf("   Error: %s\n", strerror(errno));
        return 1;
    }
    printf("✅ SUCCESS: aligned_alloc returned %p\n", arena);
    
    /* Test step 3: Initialize arena */
    printf("5. Initializing arena structure...\n");
    memset(arena, 0, sizeof(arena_t));
    
    char* memory_start = (char*)arena + sizeof(arena_t);
    printf("6. Memory start (before alignment): %p\n", memory_start);
    
    arena->memory = align_ptr(memory_start, ARENA_ALIGNMENT);
    printf("7. Memory start (after alignment): %p\n", arena->memory);
    
    /* Test step 4: Calculate usable size */
    size_t structure_overhead = arena->memory - (char*)arena;
    printf("8. Structure overhead: %zu bytes\n", structure_overhead);
    
    if (structure_overhead >= aligned_total) {
        printf("❌ CRITICAL ERROR: Structure overhead (%zu) >= aligned_total (%zu)\n", 
               structure_overhead, aligned_total);
        free(arena);
        return 1;
    }
    
    arena->size = aligned_total - structure_overhead;
    printf("9. Calculated usable size: %zu bytes (%.2f MB)\n", 
           arena->size, arena->size / (1024.0 * 1024.0));
    
    arena->current = arena->memory;
    printf("10. Current position: %p\n", arena->current);
    
    /* Test step 5: Try a small allocation */
    printf("11. Testing small allocation (64 bytes)...\n");
    char* aligned = align_ptr(arena->current, ARENA_ALIGNMENT);
    
    if (aligned + 64 > arena->memory + arena->size) {
        printf("❌ ALLOCATION WOULD EXCEED ARENA BOUNDS\n");
        printf("   aligned: %p\n", aligned);
        printf("   aligned + 64: %p\n", aligned + 64);
        printf("   arena->memory + arena->size: %p\n", arena->memory + arena->size);
        free(arena);
        return 1;
    }
    
    arena->current = aligned + 64;
    printf("✅ SUCCESS: Small allocation works, new current: %p\n", arena->current);
    
    /* Cleanup */
    free(arena);
    printf("\n🎉 Arena creation and basic allocation test PASSED!\n");
    return 0;
}