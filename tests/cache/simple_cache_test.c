#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple test to verify cache is disabled */
int main() {
    /* Initialize logger */
    logger_init(NULL, LOG_LEVEL_DEBUG);

    printf("=== Cache Disabling Test ===\n");

    /* Initialize database */
    printf("Initializing database...\n");
    database_t* db = db_init("./simple_cache_test.json");
    if (!db) {
        printf("ERROR: Failed to initialize database\n");
        return 1;
    }

    /* Try to enable cache */
    printf("Attempting to enable cache (should be disabled)...\n");
    int cache_result = db_enable_cache(db, 1000, 60);
    printf("Cache enable result: %d\n", cache_result);

    /* Verify cache disabled */
    printf("Verifying cache is disabled...\n");
    if (db->cache_enabled) {
        printf("ERROR: Cache is still enabled! Fix not active.\n");
    } else {
        printf("SUCCESS: Cache is properly disabled.\n");
    }

    /* Check for cache pointer */
    if (db->cache != NULL) {
        printf("ERROR: Cache pointer is not NULL.\n");
    } else {
        printf("SUCCESS: Cache pointer is NULL as expected.\n");
    }

    /* Close database */
    printf("Closing database...\n");
    db_close(db);

    printf("Test completed.\n");
    return 0;
}
