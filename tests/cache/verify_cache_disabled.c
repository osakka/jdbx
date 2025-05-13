#include "jsondb/database/database.h"
#include <stdio.h>
#include <stdlib.h>

/* Very minimal test to check if cache is completely disabled */
int main() {
    printf("Testing if cache is disabled in database.c...\n");
    
    /* Create a minimal database structure */
    database_t db;
    db.cache_enabled = 1;  /* Set to true initially */
    db.cache = (void*)1;   /* Set to a non-NULL value */
    
    /* Call the enable cache function with the manually created structure */
    printf("Calling db_enable_cache...\n");
    int result = db_enable_cache(&db, 1000, 60);
    
    /* Print results */
    printf("Result: %d\n", result);
    printf("db.cache_enabled: %d (should be 0)\n", db.cache_enabled);
    printf("db.cache: %p (should be NULL)\n", db.cache);
    
    if (db.cache_enabled == 0 && db.cache == NULL) {
        printf("SUCCESS: Cache is being properly disabled!\n");
    } else {
        printf("ERROR: Cache is not being disabled correctly.\n");
    }
    
    return 0;
}