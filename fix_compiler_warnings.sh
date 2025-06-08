#!/bin/bash

# Fix compiler warnings in JSONdb codebase

echo "Fixing compiler warnings..."

# Fix unused parameters in index_cleanup_api.c
sed -i '41s/api_context_t\* ctx, http_request_t\* request/api_context_t* ctx, http_request_t* request) {\n    (void)ctx; (void)request; \/\/ Currently unused\n    \n    if (!g_index_cleanup/' src/components/api/index_cleanup_api.c

sed -i '76s/api_context_t\* ctx/api_context_t* ctx) {\n    (void)ctx; \/\/ Currently unused/' src/components/api/index_cleanup_api.c

sed -i '135s/api_context_t\* ctx, http_request_t\* request/api_context_t* ctx, http_request_t* request) {\n    (void)ctx; (void)request; \/\/ Currently unused/' src/components/api/index_cleanup_api.c

sed -i '154s/api_context_t\* ctx/api_context_t* ctx) {\n    (void)ctx; \/\/ Currently unused/' src/components/api/index_cleanup_api.c

sed -i '247s/api_context_t\* ctx/api_context_t* ctx) {\n    (void)ctx; \/\/ Currently unused/' src/components/api/index_cleanup_api.c

# Remove unused variable 'stats' line 49
sed -i '49d' src/components/api/index_cleanup_api.c

# Fix unused parameters in database.c
sed -i '241s/database_t\* db/database_t* db) {\n    (void)db; \/\/ Currently unused/' src/components/database/database.c

sed -i '326s/database_t\* db/database_t* db) {\n    (void)db; \/\/ Currently unused/' src/components/database/database.c

sed -i '561s/database_t\* db/database_t* db) {\n    (void)db; \/\/ Currently unused/' src/components/database/database.c

# Comment out unused variable on line 863
sed -i '863s/^/\/\/ /' src/components/database/database.c

# Fix remaining unused parameters in database.c
sed -i '1081s/database_t\* db/database_t* db) {\n    (void)db; \/\/ Currently unused/' src/components/database/database.c

sed -i '1101s/database_t\* db/database_t* db) {\n    (void)db; \/\/ Currently unused/' src/components/database/database.c

sed -i '1150s/database_t\* db/database_t* db) {\n    (void)db; \/\/ Currently unused/' src/components/database/database.c

sed -i '1165s/database_t\* db/database_t* db) {\n    (void)db; \/\/ Currently unused/' src/components/database/database.c

sed -i '1197s/database_t\* db, size_t capacity, time_t ttl/database_t* db, size_t capacity, time_t ttl) {\n    (void)db; (void)capacity; (void)ttl; \/\/ Currently unused/' src/components/database/database.c

sed -i '1202s/database_t\* db/database_t* db) {\n    (void)db; \/\/ Currently unused/' src/components/database/database.c

sed -i '1207s/database_t\* db/database_t* db) {\n    (void)db; \/\/ Currently unused/' src/components/database/database.c

sed -i '1221s/database_t\* db/database_t* db) {\n    (void)db; \/\/ Currently unused/' src/components/database/database.c

sed -i '1253s/database_t\* db/database_t* db) {\n    (void)db; \/\/ Currently unused/' src/components/database/database.c

sed -i '1456s/database_t\* db/database_t* db) {\n    (void)db; \/\/ Currently unused/' src/components/database/database.c

# Comment out unused persistence_init function
sed -i '12s/^static/\/\/ static/' src/components/database/persistence.c

# Fix unused variables in environment.c
sed -i '174,175s/^/\/\/ /' src/components/utils/environment.c

# Fix unused variable in js_native_storage.c
sed -i '94s/^/\/\/ /' src/components/js/js_native_storage.c

# Fix implicit declaration in initialize/database.c - add proper include
sed -i '1a#include "database/index_cleanup.h"' src/initialize/database.c

echo "Warnings should be fixed. Rebuilding to verify..."