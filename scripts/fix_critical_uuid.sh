#!/bin/bash

# Fix critical UUID references
echo "Fixing critical UUID references..."

# 1. Fix api_auth_sliding.c
echo "Fixing api_auth_sliding.c..."
sed -i.bak '
s/json_object_get(session, "_id")/json_object_get(session, "uuid");\n  if (!session_id_val) session_id_val = json_object_get(session, "_id")/g
' /opt/jsondb/src/components/core/api_auth_sliding.c

# 2. Fix binary_format.c  
echo "Fixing binary_format.c..."
sed -i.bak '
s/json_object_get(document, "_id")/json_object_get(document, "uuid");\n      if (!id_val) id_val = json_object_get(document, "_id")/g
' /opt/jsondb/src/components/binary/binary_format.c

# 3. Fix query_language.c
echo "Fixing query_language.c..."
sed -i.bak '
s/strcmp(proj_key, "_id")/strcmp(proj_key, "uuid") == 0 || strcmp(proj_key, "_id")/g
s/json_object_get(projection, "_id")/json_object_get(projection, "uuid");\n  if (!id_proj) id_proj = json_object_get(projection, "_id")/g
s/json_object_get(document, "_id")/json_object_get(document, "uuid");\n    if (!id_value) id_value = json_object_get(document, "_id")/g
' /opt/jsondb/src/components/query/query_language.c

# 4. Fix operations.c
echo "Fixing operations.c..."
sed -i.bak '
s/json_object_get(doc, "_id")/json_object_get(doc, "uuid");\n    if (!doc_id) doc_id = json_object_get(doc, "_id")/g
' /opt/jsondb/src/components/database/operations.c

echo "Critical files updated. Please review and compile."