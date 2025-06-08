#!/bin/bash
# Fix unused parameter warnings in database.c

FILE="/opt/jsondb/src/components/database/database.c"

# Add (void) casts for unused parameters
sed -i '/^int db_collection_exists(database_t\* db, const char\* name) {$/a\    (void)db; /* Using global database */' "$FILE"
sed -i '/^int db_update_document(database_t\* db, const char\* collection_name,$/,/^{$/s/{$/{\n    (void)db; \/* Using global database *\//' "$FILE" 
sed -i '/^int db_delete_document(database_t\* db, const char\* collection_name,$/,/^{$/s/{$/{\n    (void)db; \/* Using global database *\//' "$FILE"
sed -i '/^json_value_t\* db_query_documents(database_t\* db, const char\* collection_name,$/,/^{$/s/{$/{\n    (void)db; \/* Using global database *\//' "$FILE"
sed -i '/^json_value_t\* db_list_collections(database_t\* db) {$/a\    (void)db; /* Using global database */' "$FILE"
sed -i '/^json_value_t\* db_list_collections_with_info(database_t\* db) {$/a\    (void)db; /* Using global database */' "$FILE"
sed -i '/^int db_drop_collection(database_t\* db, const char\* name) {$/a\    (void)db; /* Using global database */' "$FILE"
sed -i '/^int db_save(database_t\* db) {$/a\    (void)db; /* Using global database */' "$FILE"
sed -i '/^int db_load(database_t\* db) {$/a\    (void)db; /* Using global database */' "$FILE"
sed -i '/^int db_enable_cache(database_t\* db, size_t capacity, uint64_t ttl) {$/a\    (void)db; /* Using global database */\n    (void)capacity; /* Not used */\n    (void)ttl; /* Not used */' "$FILE"
sed -i '/^int db_disable_cache(database_t\* db) {$/a\    (void)db; /* Using global database */' "$FILE"
sed -i '/^int db_clear_cache(database_t\* db) {$/a\    (void)db; /* Using global database */' "$FILE"

# Comment out unused variables
sed -i 's/set_t\* seen_ids = set_create/\/\/ set_t\* seen_ids = set_create/' "$FILE"

echo "Fixed database.c warnings"