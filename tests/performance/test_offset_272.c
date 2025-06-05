#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint16_t depth;
    uint16_t count;
    uint32_t next_split;
    uint32_t flags;
    uint32_t entry_offsets[1];  /* Flexible array member for entry offsets */
    /* Entries follow after all offset slots */
} hash_bucket_t;

int main() {
    printf("Hash bucket offset calculations:\n\n");
    
    // Size of bucket header without flexible array
    size_t header_size = offsetof(hash_bucket_t, entry_offsets);
    printf("Header size (before entry_offsets): %zu bytes\n", header_size);
    
    // With 64 offset slots
    size_t size_with_64_slots = header_size + 64 * sizeof(uint32_t);
    printf("Size with 64 offset slots: %zu bytes\n", size_with_64_slots);
    
    // Calculate which slot would be at offset 272
    size_t offset_272 = 272;
    if (offset_272 >= header_size) {
        size_t array_offset = offset_272 - header_size;
        size_t slot_index = array_offset / sizeof(uint32_t);
        printf("\nOffset 272 corresponds to:\n");
        printf("  Array offset: %zu\n", array_offset);
        printf("  Slot index: %zu\n", slot_index);
        printf("  This would be entry_offsets[%zu]\n", slot_index);
    }
    
    // Calculate where entry 68 would be placed
    printf("\nWith current 64 slots:\n");
    printf("  First data entry starts at: %zu\n", size_with_64_slots);
    printf("  Offset 272 is %s the data area\n", 
           272 >= size_with_64_slots ? "in" : "before");
    
    // Calculate minimum slots needed to avoid offset 272
    size_t needed_for_272 = (272 - header_size) / sizeof(uint32_t) + 1;
    printf("\nTo keep offset 272 in the offset array, we need at least %zu slots\n", 
           needed_for_272);
    
    return 0;
}
