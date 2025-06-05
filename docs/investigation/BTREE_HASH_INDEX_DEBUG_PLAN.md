# B+Tree and Hash Index Segmentation Fault Debugging Plan

## Analysis Summary

Based on the code examination, I've identified several potential crash points in both the B+tree and hash index implementations:

### Common Issues Found

1. **Missing Implementation Files**
   - The test file includes headers for `btree_disk.h` and `hash_index.h`
   - These implementations depend on:
     - `mmap_storage.h` (exists)
     - `skiplist.h` (needs verification)
     - `generic_cache.h` (needs verification)

2. **Null Pointer Dereferences**
   - B+tree: `load_node()` can return NULL but not all callers check
   - Hash index: Directory initialization could fail silently
   - Both: Memory allocation failures not consistently checked

3. **Buffer Overflow Risks**
   - B+tree: Key copying in `split_node()` doesn't validate buffer sizes
   - Hash index: Entry redistribution in `split_bucket()` lacks bounds checking

4. **Race Conditions**
   - B+tree: Write buffer access not fully protected
   - Hash index: Directory resizing has potential race window

## Specific Crash Points

### B+Tree Implementation (`btree_disk.c`)

1. **Line 54**: `btree_node_t* node = (btree_node_t*)((char*)tree->storage->base_addr + page_id);`
   - No validation that `page_id` is within mapped region
   - Could cause segfault if page_id is corrupted

2. **Line 213**: `void* node_key = btree_node_get_key(node, mid);`
   - `btree_node_get_key()` can return NULL but result is used without checking

3. **Line 286**: `btree_node_t* child = load_node(tree, node->pointers[index]);`
   - No NULL check before using child in line 291

4. **Line 314**: `tree->storage = mmap_storage_create(path, 1024 * 1024 * 1024);`
   - If mmap_storage_create fails to map 1GB, could return invalid pointer

### Hash Index Implementation (`hash_index.c`)

1. **Line 26**: `hash_bucket_t* bucket = (hash_bucket_t*)((char*)index->storage->base_addr + offset);`
   - No bounds checking on offset calculation

2. **Line 41**: `hash_bucket_t* old_bucket = (hash_bucket_t*)((char*)index->storage->base_addr + dir_entry->bucket_offset);`
   - Directory entry could contain invalid offset

3. **Line 107**: `hash_entry_t* entry = (hash_entry_t*)((char*)old_bucket + offset);`
   - Offset could exceed bucket size

4. **Line 169**: `index->storage = mmap_storage_create(path, 256 * 1024 * 1024);`
   - 256MB initial allocation could fail on memory-constrained systems

## Debugging Steps

### 1. Add Safety Checks

```c
// Add bounds checking for page access
static btree_node_t* load_node(btree_disk_t* tree, uint64_t page_id) {
    if (page_id == 0) return NULL;
    
    // Add bounds check
    if (page_id + BTREE_PAGE_SIZE > tree->storage->mapped_size) {
        LOG_ERROR("Page ID %lu exceeds mapped region size %zu", 
                  page_id, tree->storage->mapped_size);
        return NULL;
    }
    
    // Rest of function...
}
```

### 2. Add Null Checks

```c
// Example for btree insert
btree_node_t* child = load_node(tree, node->pointers[index]);
if (!child) {
    LOG_ERROR("Failed to load child node at index %u", index);
    return -1;
}
```

### 3. Validate Memory Allocations

```c
// Check all malloc/calloc results
write_buffer_entry_t* entry = malloc(sizeof(write_buffer_entry_t));
if (!entry) {
    LOG_ERROR("Memory allocation failed for buffer entry");
    return -1;
}
```

### 4. Add Debug Logging

```c
// Add detailed logging at crash points
LOG_DEBUG("Splitting node: parent=%p, index=%u, child=%p (page_id=%lu)", 
          parent, index, child, child ? child->page_id : 0);
```

### 5. Use Valgrind/AddressSanitizer

```bash
# Compile with AddressSanitizer
cd /opt/jsondb/src
make clean
make CFLAGS="-g -O0 -fsanitize=address -fno-omit-frame-pointer"

# Run test with AddressSanitizer
cd /opt/jsondb/tests/performance
./test_indexes

# Or use Valgrind
valgrind --leak-check=full --track-origins=yes ./test_indexes
```

### 6. Create Minimal Test Case

```c
// Minimal test to isolate crash
#include <stdio.h>
#include "index/btree_disk.h"

int main() {
    printf("Creating B+tree...\n");
    btree_disk_t* btree = btree_disk_create("/tmp/test.btree", 50, NULL);
    if (!btree) {
        fprintf(stderr, "Failed to create B+tree\n");
        return 1;
    }
    
    printf("Inserting single key...\n");
    if (btree_disk_insert(btree, "test", 4, 1000) != 0) {
        fprintf(stderr, "Insert failed\n");
    }
    
    printf("Destroying B+tree...\n");
    btree_disk_destroy(btree);
    
    return 0;
}
```

## Immediate Actions

1. **Verify Dependencies**: Check if skiplist.h and generic_cache.h implementations exist
2. **Add Null Checks**: Add null pointer checks at all identified locations
3. **Add Bounds Checking**: Validate all offset calculations before memory access
4. **Enable Debug Mode**: Compile with debug symbols and sanitizers
5. **Create Test Framework**: Build incremental tests to isolate crash point

## Expected Outcomes

After implementing these fixes:
- Segmentation faults should be replaced with proper error messages
- Memory corruption issues will be caught by sanitizers
- Debug logs will show exact failure points
- Tests will run without crashes (though may fail with errors)