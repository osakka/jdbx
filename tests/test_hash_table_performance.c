#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "database/database.h"
#include "utils/logger.h"

#define NUM_COLLECTIONS 1000
#define NUM_LOOKUPS 100000

int main() {
    /* Initialize logger - set to ERROR to reduce output */
    logger_init(NULL, LOG_LEVEL_ERROR);
    
    /* Initialize database */
    database_t* db = db_init("/tmp/hash_test_db");
    if (!db) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    
    printf("Creating %d collections...\n", NUM_COLLECTIONS);
    clock_t start = clock();
    
    /* Create many collections */
    for (int i = 0; i < NUM_COLLECTIONS; i++) {
        char name[64];
        snprintf(name, sizeof(name), "test_collection_%d", i);
        if (db_create_collection(db, name) != 0) {
            fprintf(stderr, "Failed to create collection %s\n", name);
        }
    }
    
    clock_t end = clock();
    double creation_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Created %d collections in %.3f seconds\n", NUM_COLLECTIONS, creation_time);
    
    /* Test lookup performance */
    printf("\nPerforming %d random lookups...\n", NUM_LOOKUPS);
    start = clock();
    
    int found = 0;
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        char name[64];
        int idx = rand() % NUM_COLLECTIONS;
        snprintf(name, sizeof(name), "test_collection_%d", idx);
        
        if (db_get_collection(db, name) != NULL) {
            found++;
        }
    }
    
    end = clock();
    double lookup_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    double lookups_per_sec = NUM_LOOKUPS / lookup_time;
    
    printf("Completed %d lookups in %.3f seconds\n", NUM_LOOKUPS, lookup_time);
    printf("Found: %d collections\n", found);
    printf("Performance: %.0f lookups/second\n", lookups_per_sec);
    printf("Average lookup time: %.3f microseconds\n", (lookup_time * 1000000) / NUM_LOOKUPS);
    
    /* With hash table, this should be MUCH faster than O(n) linear search */
    /* Expected: > 1 million lookups/second with hash table */
    /* Without hash table: ~10,000 lookups/second with 1000 collections */
    
    if (lookups_per_sec > 100000) {
        printf("\n✓ Hash table performance confirmed! (>100K lookups/sec)\n");
    } else {
        printf("\n✗ Performance seems low for hash table implementation\n");
    }
    
    /* Cleanup */
    db_close(db);
    
    return 0;
}