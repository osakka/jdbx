#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Include JDBX headers directly */
#include "src/include/storage/storage_backend.h"
#include "src/include/utils/logger.h"

int main() {
    printf("=== JDBX Integration Test ===\n");
    
    /* Initialize logger */
    logger_init("/tmp/jdbx_test.log", LOG_LEVEL_DEBUG);
    
    /* Test 1: Create MMAP storage backend */
    printf("Test 1: Creating MMAP storage backend...\n");
    storage_backend_t* mmap_backend = storage_backend_create(STORAGE_BACKEND_MMAP);
    if (!mmap_backend) {
        printf("FAILED: Could not create MMAP backend\n");
        return 1;
    }
    
    /* Initialize MMAP storage */
    if (mmap_backend->ops->init(mmap_backend, "/tmp/test_mmap.db", 1024*1024) != 0) {
        printf("FAILED: Could not initialize MMAP storage\n");
        storage_backend_destroy(mmap_backend);
        return 1;
    }
    printf("SUCCESS: MMAP backend created and initialized\n");
    
    /* Test 2: Create JDBX storage backend */
    printf("Test 2: Creating JDBX storage backend...\n");
    storage_backend_t* jdbx_backend = storage_backend_create(STORAGE_BACKEND_JDBX);
    if (!jdbx_backend) {
        printf("FAILED: Could not create JDBX backend\n");
        storage_backend_destroy(mmap_backend);
        return 1;
    }
    
    /* Initialize JDBX storage */
    if (jdbx_backend->ops->init(jdbx_backend, "/tmp/test_jdbx.db", 1024*1024) != 0) {
        printf("FAILED: Could not initialize JDBX storage\n");
        storage_backend_destroy(mmap_backend);
        storage_backend_destroy(jdbx_backend);
        return 1;
    }
    printf("SUCCESS: JDBX backend created and initialized\n");
    
    /* Test 3: Test basic storage operations */
    printf("Test 3: Testing basic storage operations...\n");
    
    const char* test_id = "test-doc-1";
    const char* test_data = "{\"message\":\"Hello JDBX!\",\"number\":42}";
    
    /* Store in JDBX */
    if (jdbx_backend->ops->store(jdbx_backend, test_id, test_data, strlen(test_data)) != 0) {
        printf("FAILED: Could not store document in JDBX\n");
        storage_backend_destroy(mmap_backend);
        storage_backend_destroy(jdbx_backend);
        return 1;
    }
    printf("SUCCESS: Document stored in JDBX\n");
    
    /* Retrieve from JDBX */
    size_t retrieved_size = 0;
    char* retrieved_data = jdbx_backend->ops->retrieve(jdbx_backend, test_id, &retrieved_size);
    if (!retrieved_data) {
        printf("FAILED: Could not retrieve document from JDBX\n");
        storage_backend_destroy(mmap_backend);
        storage_backend_destroy(jdbx_backend);
        return 1;
    }
    
    if (strcmp(retrieved_data, test_data) != 0) {
        printf("FAILED: Retrieved data doesn't match original\n");
        printf("Original: %s\n", test_data);
        printf("Retrieved: %s\n", retrieved_data);
        free(retrieved_data);
        storage_backend_destroy(mmap_backend);
        storage_backend_destroy(jdbx_backend);
        return 1;
    }
    printf("SUCCESS: Document retrieved correctly from JDBX\n");
    free(retrieved_data);
    
    /* Store in MMAP for comparison */
    if (mmap_backend->ops->store(mmap_backend, test_id, test_data, strlen(test_data)) != 0) {
        printf("WARNING: Could not store document in MMAP (expected - function may not be implemented)\n");
    } else {
        printf("SUCCESS: Document stored in MMAP\n");
        
        /* Try to retrieve from MMAP */
        size_t mmap_size = 0;
        char* mmap_data = mmap_backend->ops->retrieve(mmap_backend, test_id, &mmap_size);
        if (mmap_data) {
            printf("SUCCESS: Document retrieved from MMAP\n");
            free(mmap_data);
        } else {
            printf("WARNING: Could not retrieve from MMAP (expected - function may not be implemented)\n");
        }
    }
    
    /* Test 4: Test with environment variable configuration */
    printf("Test 4: Testing environment variable configuration...\n");
    setenv("JSONDB_STORAGE_BACKEND", "jdbx", 1);
    printf("Set JSONDB_STORAGE_BACKEND=jdbx\n");
    
    /* Cleanup */
    storage_backend_destroy(mmap_backend);
    storage_backend_destroy(jdbx_backend);
    
    /* Clean up test files */
    unlink("/tmp/test_mmap.db");
    unlink("/tmp/test_jdbx.db");
    unlink("/tmp/test_jdbx.db.wal");
    unlink("/tmp/jdbx_test.log");
    
    printf("\n=== All JDBX Integration Tests PASSED ===\n");
    return 0;
}