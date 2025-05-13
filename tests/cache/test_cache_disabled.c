#include "jsondb/database/database.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    /* Initialize logger */
    logger_init(NULL, LOG_LEVEL_DEBUG); /* NULL means output to stderr */
    
    /* Initialize database */
    const char* db_path = "./test_cache_disabled.json";
    LOG_INFO("Creating database at %s", db_path);
    
    database_t* db = db_init(db_path);
    if (!db) {
        LOG_ERROR("Failed to initialize database");
        return 1;
    }
    
    /* Create collection */
    LOG_INFO("Creating collection 'test'");
    if (!db_create_collection(db, "test")) {
        LOG_ERROR("Failed to create collection");
        db_close(db);
        return 1;
    }
    
    /* Try to enable cache - this should be disabled */
    LOG_INFO("Attempting to enable cache - should be disabled");
    int cache_result = db_enable_cache(db, 1000, 60);
    
    /* Check if cache is really disabled */
    LOG_INFO("Checking cache status");
    json_value_t* stats = db_get_cache_stats(db);
    
    /* Print cache stats */
    char* stats_str = json_stringify(stats);
    LOG_INFO("Cache stats: %s", stats_str);
    free(stats_str);
    
    /* Check if cache is disabled */
    if (db->cache_enabled) {
        LOG_ERROR("Cache is still enabled! Deadlock fix not active.");
    } else {
        LOG_INFO("SUCCESS: Cache is disabled. Deadlock fix is active.");
    }
    
    /* Cleanup */
    json_free(stats);
    db_close(db);
    
    /* Return success */
    return 0;
}