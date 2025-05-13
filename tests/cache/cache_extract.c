#include <stdio.h>
#include <stdlib.h>

/* Extract of the db_enable_cache function to see what it does */

/* Simplified database structure */
typedef struct {
    void* cache;
    int cache_enabled;
} database_t;

/* Enable document cache - extracted directly from database.c */
int db_enable_cache(database_t* db, int capacity, int ttl) {
    if (!db) {
        return 0;
    }

    /* COMPLETELY DISABLE CACHE CREATION TO RESOLVE DEADLOCKS */
    printf("Cache creation requested but PERMANENTLY DISABLED to prevent deadlocks\n");

    /* No cache creation at all */
    db->cache = NULL;
    db->cache_enabled = 0;

    /* Log the status for debugging */
    printf("Cache is completely disabled for stability. Using simplified database operations.\n");

    (void)capacity; /* Suppress unused parameter warning */
    (void)ttl;      /* Suppress unused parameter warning */

    /* Return success */
    return 1;
}

int main() {
    printf("Testing extracted db_enable_cache function\n");
    
    /* Create a simple database structure */
    database_t db;
    db.cache_enabled = 1;
    db.cache = (void*)0x12345678;
    
    printf("Before: cache_enabled=%d, cache=%p\n", db.cache_enabled, db.cache);
    
    /* Call the enable cache function */
    int result = db_enable_cache(&db, 1000, 60);
    
    printf("Result: %d\n", result);
    printf("After: cache_enabled=%d, cache=%p\n", db.cache_enabled, db.cache);
    
    if (db.cache_enabled == 0 && db.cache == NULL) {
        printf("SUCCESS: Cache is disabled correctly!\n");
    } else {
        printf("ERROR: Cache is not being disabled!\n");
    }
    
    return 0;
}