#!/bin/bash

# Standardize on UUID field - Remove _id support entirely
echo "Standardizing on 'uuid' field and removing '_id' support..."

# 1. Fix api_auth_sliding.c
echo "Fixing api_auth_sliding.c..."
if [ -f "/opt/jdbx/src/components/core/api_auth_sliding.c" ]; then
    sed -i.bak 's/json_object_get(session, "_id")/json_object_get(session, "uuid")/g' /opt/jdbx/src/components/core/api_auth_sliding.c
    echo "  ✓ Updated api_auth_sliding.c"
else
    echo "  ⚠ api_auth_sliding.c not found, skipping"
fi

# 2. Fix binary_format.c  
echo "Fixing binary_format.c..."
if [ -f "/opt/jdbx/src/components/binary/binary_format.c" ]; then
    sed -i.bak 's/json_object_get(document, "_id")/json_object_get(document, "uuid")/g' /opt/jdbx/src/components/binary/binary_format.c
    echo "  ✓ Updated binary_format.c"
else
    echo "  ⚠ binary_format.c not found, skipping"
fi

# 3. Fix query_language.c
echo "Fixing query_language.c..."
if [ -f "/opt/jdbx/src/components/query/query_language.c" ]; then
    sed -i.bak 's/strcmp(proj_key, "_id")/strcmp(proj_key, "uuid")/g; s/json_object_get(projection, "_id")/json_object_get(projection, "uuid")/g; s/json_object_get(document, "_id")/json_object_get(document, "uuid")/g' /opt/jdbx/src/components/query/query_language.c
    echo "  ✓ Updated query_language.c"
else
    echo "  ⚠ query_language.c not found, skipping"
fi

# 4. Fix operations.c (legacy database operations)
echo "Fixing operations.c..."
if [ -f "/opt/jdbx/src/components/database/operations.c" ]; then
    sed -i.bak 's/json_object_get(doc, "_id")/json_object_get(doc, "uuid")/g' /opt/jdbx/src/components/database/operations.c
    echo "  ✓ Updated operations.c"
else
    echo "  ⚠ operations.c not found, skipping"
fi

# 5. Fix main database.c (JDBX implementation) - already fixed manually
echo "Main database.c already updated to use 'uuid' exclusively"

echo "Critical files updated. Please review and compile."