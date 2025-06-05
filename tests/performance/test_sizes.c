#include <stdio.h>
#include <stddef.h>
#include "index/hash_index.h"

int main() {
    printf("sizeof(hash_bucket_t) = %zu\n", sizeof(hash_bucket_t));
    printf("offsetof(hash_bucket_t, entry_offsets) = %zu\n", offsetof(hash_bucket_t, entry_offsets));
    printf("sizeof(uint32_t) = %zu\n", sizeof(uint32_t));
    printf("sizeof(hash_entry_t) = %zu\n", sizeof(hash_entry_t));
    return 0;
}
