#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

/* Simulate checkpoint and arena structures */
typedef struct arena_t {
    char* memory;
    size_t size;
    char* current;
    struct arena_t* next;
} arena_t;

typedef struct memory_checkpoint_t {
    struct memory_checkpoint_t* parent;
    void* first_alloc;
    void* last_alloc;
    time_t created_at;
    size_t allocation_count;
    size_t total_size;
    int committed;
    arena_t* arena;
    uint32_t arena_checkpoint_id;
    pthread_spinlock_t lock;
} memory_checkpoint_t;

/* Arena configuration */
#define ARENA_DEFAULT_SIZE      (4 * 1024 * 1024)  /* 4MB default */
#define ARENA_ALIGNMENT         16                   /* 16-byte alignment */

/* Utility: align pointer */
static inline char* align_ptr(char* ptr, size_t alignment) {
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return (char*)aligned;
}

/* Simplified arena creation */
arena_t* arena_create(size_t size) {
    if (size < ARENA_DEFAULT_SIZE) {
        size = ARENA_DEFAULT_SIZE;
    }
    
    printf("   🏗️  Creating arena with size: %zu bytes (%.2f MB)\n", size, size / (1024.0 * 1024.0));
    
    /* Allocate arena structure and memory together with proper alignment */
    size_t total_size = sizeof(arena_t) + size;
    size_t aligned_total = (total_size + 64 - 1) & ~(64 - 1);  /* Align to 64-byte boundary */
    
    printf("   📐 Total size calculation:\n");
    printf("       struct size: %zu\n", sizeof(arena_t));
    printf("       requested: %zu\n", size);
    printf("       total: %zu\n", total_size);
    printf("       aligned: %zu\n", aligned_total);
    
    arena_t* arena = (arena_t*)aligned_alloc(64, aligned_total);
    if (!arena) {
        printf("   ❌ ARENA CREATION FAILED: aligned_alloc returned NULL\n");
        printf("       Error: %s\n", strerror(errno));
        return NULL;
    }
    
    printf("   ✅ Arena allocated at: %p\n", arena);
    
    /* Initialize arena */
    memset(arena, 0, sizeof(arena_t));
    char* memory_start = (char*)arena + sizeof(arena_t);
    arena->memory = align_ptr(memory_start, ARENA_ALIGNMENT);
    
    /* Calculate usable size correctly accounting for structure and alignment */
    size_t structure_overhead = arena->memory - (char*)arena;
    if (structure_overhead >= aligned_total) {
        printf("   ❌ CRITICAL ERROR: Structure overhead (%zu) >= aligned_total (%zu)\n", 
               structure_overhead, aligned_total);
        free(arena);
        return NULL;
    }
    
    arena->size = aligned_total - structure_overhead;
    arena->current = arena->memory;
    
    printf("   📊 Arena metrics:\n");
    printf("       structure overhead: %zu bytes\n", structure_overhead);
    printf("       usable size: %zu bytes (%.2f MB)\n", arena->size, arena->size / (1024.0 * 1024.0));
    printf("       memory region: %p - %p\n", arena->memory, arena->memory + arena->size);
    
    return arena;
}

/* Simplified checkpoint creation with arena */
memory_checkpoint_t* memory_checkpoint_create() {
    printf("🔄 Creating memory checkpoint...\n");
    
    /* Allocate checkpoint */
    memory_checkpoint_t* checkpoint = (memory_checkpoint_t*)aligned_alloc(_Alignof(max_align_t), sizeof(memory_checkpoint_t));
    if (!checkpoint) {
        printf("❌ CHECKPOINT CREATION FAILED: Cannot allocate checkpoint structure\n");
        return NULL;
    }
    
    printf("✅ Checkpoint allocated at: %p\n", checkpoint);
    
    /* Initialize spinlock */
    if (pthread_spin_init(&checkpoint->lock, PTHREAD_PROCESS_PRIVATE) != 0) {
        printf("❌ CHECKPOINT CREATION FAILED: Cannot initialize spinlock\n");
        free(checkpoint);
        return NULL;
    }
    
    /* Initialize checkpoint */
    checkpoint->parent = NULL;
    checkpoint->first_alloc = NULL;
    checkpoint->last_alloc = NULL;
    checkpoint->created_at = time(NULL);
    checkpoint->allocation_count = 0;
    checkpoint->total_size = 0;
    checkpoint->committed = 0;
    
    /* Create arena for checkpoint allocations */
    checkpoint->arena = NULL;
    checkpoint->arena_checkpoint_id = 0;
    
    /* Create arena (4MB) */
    printf("🏗️  Creating arena for checkpoint...\n");
    checkpoint->arena = arena_create(4 * 1024 * 1024);
    
    if (checkpoint->arena) {
        checkpoint->arena_checkpoint_id = 1;  // Simplified
        printf("✅ Arena successfully created and attached to checkpoint\n");
        printf("🎯 Checkpoint %p now has arena %p ready for allocations\n", 
               checkpoint, checkpoint->arena);
        
        /* Test a small allocation */
        printf("🧪 Testing small allocation (64 bytes)...\n");
        char* aligned = align_ptr(checkpoint->arena->current, ARENA_ALIGNMENT);
        if (aligned + 64 <= checkpoint->arena->memory + checkpoint->arena->size) {
            checkpoint->arena->current = aligned + 64;
            printf("✅ Test allocation successful, arena is working!\n");
        } else {
            printf("❌ Test allocation failed - arena bounds check failed\n");
        }
    } else {
        printf("❌ ARENA CREATION FAILED - checkpoint will fallback to system malloc\n");
    }
    
    return checkpoint;
}

int main() {
    printf("🔬 Checkpoint Arena Creation Debug Test\n");
    printf("=======================================\n");
    
    /* Test checkpoint creation with arena */
    memory_checkpoint_t* checkpoint = memory_checkpoint_create();
    
    if (!checkpoint) {
        printf("\n💥 CRITICAL FAILURE: Could not create checkpoint\n");
        return 1;
    }
    
    if (!checkpoint->arena) {
        printf("\n💥 CRITICAL FAILURE: Checkpoint created but arena is NULL\n");
        printf("   This explains why Arena allocations never happen!\n");
        free(checkpoint);
        return 1;
    }
    
    printf("\n🎉 SUCCESS: Checkpoint with Arena created successfully!\n");
    printf("   Checkpoint: %p\n", checkpoint);
    printf("   Arena: %p\n", checkpoint->arena);
    printf("   Arena size: %zu bytes (%.2f MB)\n", 
           checkpoint->arena->size, checkpoint->arena->size / (1024.0 * 1024.0));
    
    /* Cleanup */
    if (checkpoint->arena) {
        free(checkpoint->arena);
    }
    pthread_spin_destroy(&checkpoint->lock);
    free(checkpoint);
    
    printf("\n✅ Test completed successfully - Arena creation works!\n");
    return 0;
}