#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/production_config.h"

int main() {
    logger_init("/tmp/test_prod_config.log", LOG_LEVEL_DEBUG);
    
    printf("Testing Production Configuration\n");
    printf("================================\n\n");
    
    /* Test different config levels */
    const char* levels[] = {"development", "small", "medium", "large"};
    
    for (int i = 0; i < 4; i++) {
        printf("Testing config level: %s\n", levels[i]);
        
        /* Set environment variable */
        setenv("JSONDB_CONFIG_LEVEL", levels[i], 1);
        
        /* Initialize production config */
        production_config_init(levels[i]);
        
        /* Get config and print values */
        const production_config_t* config = production_config_get();
        printf("  MMAP size: %s\n", format_bytes(config->mmap_size_per_collection));
        printf("  Cache size: %s\n", format_bytes(config->cache_size_total));
        printf("  Buffer pool: %s\n", format_bytes(config->buffer_pool_size));
        printf("  Index batch size: %u\n", config->index_batch_size);
        printf("  Lazy indexing: %s\n", config->lazy_indexing ? "enabled" : "disabled");
        printf("\n");
        
        /* Initialize database with this config */
        char db_path[256];
        snprintf(db_path, sizeof(db_path), "/tmp/test_db_%s", levels[i]);
        
        database_t* db = db_init(db_path);
        if (db) {
            /* Create a test collection */
            db_create_collection(db, "test_collection");
            
            /* Insert a few documents */
            double start = (double)clock() / CLOCKS_PER_SEC;
            for (int j = 0; j < 100; j++) {
                json_value_t* doc = json_create_object();
                json_object_set(doc, "id", json_create_integer(j));
                json_object_set(doc, "value", json_create_string("test"));
                
                json_value_t* result = db_insert_document(db, "test_collection", doc);
                if (result) json_free(result);
                json_free(doc);
            }
            double end = (double)clock() / CLOCKS_PER_SEC;
            
            printf("  Inserted 100 documents in %.3f seconds\n", end - start);
            
            db_close(db);
        } else {
            printf("  Failed to initialize database\n");
        }
        
        printf("  ---\n\n");
    }
    
    logger_close();
    return 0;
}