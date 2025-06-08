#!/bin/bash

# Fix the database.c warnings properly by placing (void) casts inside function bodies

echo "Fixing database.c warnings properly..."

# First, remove all the misplaced (void) statements
sed -i '/^[[:space:]]*(void).*;[[:space:]]*\/\/ Currently unused$/d' src/components/database/database.c

# Now add them in the correct places
# Fix db_get_document (line ~565)
sed -i '/^json_value_t\* db_get_document(database_t\* db, const char\* collection_name,/{n;s/const char\* id) {/const char* id) {\n    (void)db; \/\/ Currently unused/}' src/components/database/database.c

# Fix db_rebuild_indices (line ~1086)
sed -i '/^int db_rebuild_indices(database_t\* db) {/a\    (void)db; // Currently unused' src/components/database/database.c

# Fix db_drop_collection (line ~1107)
sed -i '/^int db_drop_collection(database_t\* db, const char\* name) {/a\    (void)db; // Currently unused' src/components/database/database.c

# Fix db_save (line ~1155)
sed -i '/^int db_save(database_t\* db) {/a\    (void)db; // Currently unused' src/components/database/database.c

# Fix db_load (line ~1172)
sed -i '/^int db_load(database_t\* db) {/a\    (void)db; // Currently unused' src/components/database/database.c

# Fix db_enable_cache - this one has 3 params
sed -i '/^int db_enable_cache(database_t\* db, int capacity, int ttl) {/a\    (void)db; (void)capacity; (void)ttl; // Currently unused' src/components/database/database.c

# Fix db_disable_cache (line ~1212)
sed -i '/^int db_disable_cache(database_t\* db) {/a\    (void)db; // Currently unused' src/components/database/database.c

# Fix db_clear_cache (line ~1217)
sed -i '/^int db_clear_cache(database_t\* db) {/a\    (void)db; // Currently unused' src/components/database/database.c

# Fix db_query_documents (line ~1227)
sed -i '/^json_value_t\* db_query_documents(database_t\* db, const char\* collection_name,/{n;s/json_value_t\* query, int offset, int limit) {/json_value_t* query, int offset, int limit) {\n    (void)db; \/\/ Currently unused/}' src/components/database/database.c

# Fix db_get_collections (line ~1265)
sed -i '/^json_value_t\* db_get_collections(database_t\* db) {/a\    (void)db; // Currently unused' src/components/database/database.c

# Fix db_find_with_mmap (line ~1458)
sed -i '/^json_value_t\* db_find_with_mmap(database_t\* db, const char\* collection_name,/{n;s/json_value_t\* query) {/json_value_t* query) {\n    (void)db; \/\/ Currently unused/}' src/components/database/database.c

# Fix db_drop_index (line ~1469)
sed -i '/^int db_drop_index(database_t\* db, const char\* collection_name, const char\* name) {/a\    (void)db; // Currently unused' src/components/database/database.c

echo "Done fixing database.c warnings"