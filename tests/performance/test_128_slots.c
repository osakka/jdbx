#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t depth;
    uint16_t count;
    uint32_t next_split;
    uint32_t flags;
    uint32_t entry_offsets[1];  /* Flexible array member */
} hash_bucket_t;

int main() {
    size_t header_size = offsetof(hash_bucket_t, entry_offsets);
    printf("Header size: %zu bytes\n", header_size);
    
    size_t size_with_128_slots = header_size + 128 * sizeof(uint32_t);
    printf("Size with 128 slots: %zu bytes\n", size_with_128_slots);
    
    printf("\nOffset 272 is %s the reserved area for 128 slots\n",
           272 < size_with_128_slots ? "within" : "outside");
    
    if (272 >= header_size) {
        size_t slot_index = (272 - header_size) / sizeof(uint32_t);
        printf("Offset 272 would be slot index %zu\n", slot_index);
    }
    
    return 0;
}
