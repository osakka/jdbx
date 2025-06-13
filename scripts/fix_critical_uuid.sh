#!/bin/bash

# Fix critical UUID references - Updated for JDBX v4.0.0
echo "Fixing critical UUID references in JDBX codebase..."

# 1. Fix api_auth_sliding.c
echo "Fixing api_auth_sliding.c..."
if [ -f "/opt/jdbx/src/components/core/api_auth_sliding.c" ]; then
    sed -i.bak '
    s/json_object_get(session, "_id")/json_object_get(session, "uuid");\n  if (!session_id_val) session_id_val = json_object_get(session, "_id")/g
    ' /opt/jdbx/src/components/core/api_auth_sliding.c
    echo "  ✓ Updated api_auth_sliding.c"
else
    echo "  ⚠ api_auth_sliding.c not found, skipping"
fi

# 2. Fix binary_format.c  
echo "Fixing binary_format.c..."
if [ -f "/opt/jdbx/src/components/binary/binary_format.c" ]; then
    sed -i.bak '
    s/json_object_get(document, "_id")/json_object_get(document, "uuid");\n      if (!id_val) id_val = json_object_get(document, "_id")/g
    ' /opt/jdbx/src/components/binary/binary_format.c
    echo "  ✓ Updated binary_format.c"
else
    echo "  ⚠ binary_format.c not found, skipping"
fi

# 3. Fix query_language.c
echo "Fixing query_language.c..."
if [ -f "/opt/jdbx/src/components/query/query_language.c" ]; then
    sed -i.bak '
    s/strcmp(proj_key, "_id")/strcmp(proj_key, "uuid") == 0 || strcmp(proj_key, "_id")/g
    s/json_object_get(projection, "_id")/json_object_get(projection, "uuid");\n  if (!id_proj) id_proj = json_object_get(projection, "_id")/g
    s/json_object_get(document, "_id")/json_object_get(document, "uuid");\n    if (!id_value) id_value = json_object_get(document, "_id")/g
    ' /opt/jdbx/src/components/query/query_language.c
    echo "  ✓ Updated query_language.c"
else
    echo "  ⚠ query_language.c not found, skipping"
fi

# 4. Fix operations.c (legacy database operations)
echo "Fixing operations.c..."
if [ -f "/opt/jdbx/src/components/database/operations.c" ]; then
    sed -i.bak '
    s/json_object_get(doc, "_id")/json_object_get(doc, "uuid");\n    if (!doc_id) doc_id = json_object_get(doc, "_id")/g
    ' /opt/jdbx/src/components/database/operations.c
    echo "  ✓ Updated operations.c"
else
    echo "  ⚠ operations.c not found, skipping"
fi

# 5. Fix main database.c (JDBX implementation)
echo "Fixing main database.c..."
if [ -f "/opt/jdbx/src/components/database/database.c" ]; then
    sed -i.bak '
    s/json_object_get(document, "_id")/json_object_get(document, "uuid");\n    if (!id_val) id_val = json_object_get(document, "_id")/g
    ' /opt/jdbx/src/components/database/database.c
    echo "  ✓ Updated main database.c"
else
    echo "  ⚠ main database.c not found, skipping"
fi

echo "Critical files updated. Please review and compile."