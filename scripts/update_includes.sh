#\!/bin/bash

# Script to update include paths in all source files
# according to the new directory structure

set -e

echo "Updating include paths in source files..."

# Common header replacements
find src include -type f -name "*.c" -o -name "*.h"  < /dev/null |  xargs sed -i 's|#include "api\.h"|#include "api/api.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "database\.h"|#include "database/database.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "js_engine\.h"|#include "js/js_engine.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "json\.h"|#include "utils/json.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "server\.h"|#include "core/server.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "ssl\.h"|#include "core/ssl.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "transaction\.h"|#include "transaction/transaction.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "query_language\.h"|#include "query/query_language.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "rbac\.h"|#include "rbac/rbac.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "jwt\.h"|#include "rbac/jwt.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "transaction_retry\.h"|#include "transaction/transaction_retry.h"|g'

# Fix relative path includes that use ../include pattern
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/api\.h"|#include "api/api.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/database\.h"|#include "database/database.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/js_engine\.h"|#include "js/js_engine.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/json\.h"|#include "utils/json.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/server\.h"|#include "core/server.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/ssl\.h"|#include "core/ssl.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/transaction\.h"|#include "transaction/transaction.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/query_language\.h"|#include "query/query_language.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/rbac\.h"|#include "rbac/rbac.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/jwt\.h"|#include "rbac/jwt.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/transaction_retry\.h"|#include "transaction/transaction_retry.h"|g'

# Fix utils headers
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/utils/memory/ref_counter\.h"|#include "utils/memory/ref_counter.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/utils/memory/ref_json\.h"|#include "utils/memory/ref_json.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/utils/json_helpers\.h"|#include "utils/json_helpers.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/utils/metrics\.h"|#include "utils/metrics.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/utils/cache\.h"|#include "utils/cache.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/utils/debug\.h"|#include "utils/debug.h"|g'
find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i 's|#include "../include/utils/import_export\.h"|#include "utils/import_export.h"|g'

# Within same component, make paths use component names
find src/api -type f -name "*.c" | xargs sed -i 's|#include "api\.h"|#include "api/api.h"|g'
find src/database -type f -name "*.c" | xargs sed -i 's|#include "database\.h"|#include "database/database.h"|g'
find src/js -type f -name "*.c" | xargs sed -i 's|#include "js_engine\.h"|#include "js/js_engine.h"|g'
find src/utils -type f -name "*.c" | xargs sed -i 's|#include "json\.h"|#include "utils/json.h"|g'
find src/core -type f -name "*.c" | xargs sed -i 's|#include "server\.h"|#include "core/server.h"|g'
find src/core -type f -name "*.c" | xargs sed -i 's|#include "ssl\.h"|#include "core/ssl.h"|g'
find src/transaction -type f -name "*.c" | xargs sed -i 's|#include "transaction\.h"|#include "transaction/transaction.h"|g'
find src/query -type f -name "*.c" | xargs sed -i 's|#include "query_language\.h"|#include "query/query_language.h"|g'
find src/rbac -type f -name "*.c" | xargs sed -i 's|#include "rbac\.h"|#include "rbac/rbac.h"|g'
find src/rbac -type f -name "*.c" | xargs sed -i 's|#include "jwt\.h"|#include "rbac/jwt.h"|g'

echo "Include path updates completed."
echo "You should now check for build errors to identify any includes that were missed."
