#!/bin/bash

# Buffer Pool Migration Script - SINGLE SOURCE OF TRUTH
# This script surgically replaces the old buffer pool with the enterprise version

set -e

echo "=== JDBX Buffer Pool Migration ==="
echo "Migrating to enterprise-grade buffer pool implementation"
echo

# Step 1: Backup current buffer pool
echo "1. Backing up current buffer pool..."
cp src/components/utils/buffer_pool.c src/components/utils/buffer_pool.c.backup
echo "   ✓ Backup created: buffer_pool.c.backup"

# Step 2: Replace with enterprise version
echo "2. Installing enterprise buffer pool..."
cp src/components/utils/buffer_pool_enterprise.c src/components/utils/buffer_pool.c
echo "   ✓ Enterprise buffer pool installed"

# Step 3: Clean any old object files
echo "3. Cleaning old build artifacts..."
rm -f build/obj/components/utils/buffer_pool.o
echo "   ✓ Old artifacts removed"

# Step 4: Rebuild the server
echo "4. Rebuilding JDBX server..."
cd src
make clean >/dev/null 2>&1
make -j$(nproc)
cd ..
echo "   ✓ Server rebuilt successfully"

# Step 5: Validate build
echo "5. Validating build..."
if [ -f build/bin/jdbxd ]; then
    echo "   ✓ Binary created successfully"
    size=$(ls -lh build/bin/jdbxd | awk '{print $5}')
    echo "   ✓ Binary size: $size"
else
    echo "   ✗ Build failed!"
    exit 1
fi

# Step 6: Run basic memory test
echo "6. Running basic memory test..."
cat > test_buffer_pool.c << 'EOF'
#include <stdio.h>
#include <string.h>

// Prototypes
void* buffer_pool_alloc_safe(size_t size, const char* file, int line, const char* func);
void buffer_pool_free_safe(void* ptr, const char* file, int line, const char* func);
char* buffer_pool_strdup(const char* str);
void buffer_pool_shutdown(void);

#define BUFFER_ALLOC(size) buffer_pool_alloc_safe(size, __FILE__, __LINE__, __func__)
#define BUFFER_FREE(ptr) buffer_pool_free_safe(ptr, __FILE__, __LINE__, __func__)
#define BUFFER_STRDUP(str) buffer_pool_strdup(str)

int main() {
    printf("Testing enterprise buffer pool...\n");
    
    // Test 1: Basic allocation
    void* ptr1 = BUFFER_ALLOC(100);
    if (!ptr1) {
        printf("FAIL: Basic allocation failed\n");
        return 1;
    }
    printf("✓ Basic allocation\n");
    
    // Test 2: String duplication
    char* str = BUFFER_STRDUP("Hello, Buffer Pool!");
    if (!str || strcmp(str, "Hello, Buffer Pool!") != 0) {
        printf("FAIL: String duplication failed\n");
        return 1;
    }
    printf("✓ String duplication\n");
    
    // Test 3: Free
    BUFFER_FREE(ptr1);
    BUFFER_FREE(str);
    printf("✓ Memory freed\n");
    
    // Test 4: Multiple allocations
    void* ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = BUFFER_ALLOC(i * 10 + 8);
        if (!ptrs[i]) {
            printf("FAIL: Allocation %d failed\n", i);
            return 1;
        }
    }
    printf("✓ 100 allocations\n");
    
    // Free them
    for (int i = 0; i < 100; i++) {
        BUFFER_FREE(ptrs[i]);
    }
    printf("✓ 100 deallocations\n");
    
    // Shutdown to check for leaks
    buffer_pool_shutdown();
    
    printf("\nAll tests passed!\n");
    return 0;
}
EOF

gcc -o test_buffer_pool test_buffer_pool.c build/obj/components/utils/buffer_pool.o -pthread
./test_buffer_pool
rm -f test_buffer_pool test_buffer_pool.c
echo "   ✓ Memory tests passed"

echo
echo "=== Migration Complete ==="
echo "The enterprise buffer pool is now active."
echo "Old implementation backed up to: buffer_pool.c.backup"
echo
echo "Next steps:"
echo "1. Run comprehensive test suite"
echo "2. Monitor for memory leaks"
echo "3. Check performance metrics"
echo
echo "To rollback:"
echo "  cp src/components/utils/buffer_pool.c.backup src/components/utils/buffer_pool.c"
echo "  cd src && make clean && make"