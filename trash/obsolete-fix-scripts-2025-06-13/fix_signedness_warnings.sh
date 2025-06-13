#!/bin/bash
#
# fix_signedness_warnings.sh
# Fixes signedness comparison warnings in the JSONdb codebase
#

echo "Starting to fix signedness comparison warnings..."

# Query language file - has several array size comparison warnings
QUERY_FILE="src/query/query_language.c"

if [ -f "$QUERY_FILE" ]; then
    echo "Processing $QUERY_FILE..."
    
    # Fix array size comparison warnings - fix json_array_size() comparisons
    sed -i 's/for (int i = 0; i < json_array_size(\([^)]*\)); i++)/for (size_t i = 0; i < json_array_size(\1); i++)/g' "$QUERY_FILE"
    sed -i 's/for (int j = 0; j < json_array_size(\([^)]*\)); j++)/for (size_t j = 0; j < json_array_size(\1); j++)/g' "$QUERY_FILE"
    
    # Fix array index comparison - for the case in query_extract_field
    sed -i 's/index >= 0 && index < json_array_size(\([^)]*\))/index < json_array_size(\1)/g' "$QUERY_FILE"
    
    # Fix type comparison in line 702
    sed -i 's/field_value->type == type_val/field_value->type == (json_type_t)type_val/g' "$QUERY_FILE"
    
    # Fix the ?: expression in query_execute
    sed -i 's/documents ? documents->type : -1/documents ? documents->type : (json_type_t)-1/g' "$QUERY_FILE"
    
    echo "Fixed signedness warnings in $QUERY_FILE"
else
    echo "Error: $QUERY_FILE not found!"
fi

# Index query API file
INDEX_QUERY_FILE="src/api/index_query_api.c"
if [ -f "$INDEX_QUERY_FILE" ]; then
    echo "Processing $INDEX_QUERY_FILE..."
    
    # Fix comparison between size_t and int
    sed -i 's/i < skip + limit/i < (size_t)(skip + limit)/g' "$INDEX_QUERY_FILE"
    
    echo "Fixed signedness warnings in $INDEX_QUERY_FILE"
else
    echo "Error: $INDEX_QUERY_FILE not found!"
fi

# Transaction API file
TRANSACTION_API_FILE="src/api/transaction_api.c"
if [ -f "$TRANSACTION_API_FILE" ]; then
    echo "Processing $TRANSACTION_API_FILE..."
    
    # Fix isolation level comparison
    sed -i 's/isolation == ISOLATION_INVALID/isolation == (isolation_level_t)ISOLATION_INVALID/g' "$TRANSACTION_API_FILE"
    
    echo "Fixed signedness warnings in $TRANSACTION_API_FILE"
else
    echo "Error: $TRANSACTION_API_FILE not found!"
fi

# Transaction visualization enhanced file
TRANS_VIZ_FILE="src/transaction/transaction_visualization_enhanced.c"
if [ -f "$TRANS_VIZ_FILE" ]; then
    echo "Processing $TRANS_VIZ_FILE..."
    
    # Fix array size comparison warnings
    sed -i 's/for (int i = 0; i < json_array_size(\([^)]*\)); i++)/for (size_t i = 0; i < json_array_size(\1); i++)/g' "$TRANS_VIZ_FILE"
    sed -i 's/for (int j = 0; j < json_array_size(\([^)]*\)); j++)/for (size_t j = 0; j < json_array_size(\1); j++)/g' "$TRANS_VIZ_FILE"
    
    # Fix sizeof comparison
    sed -i 's/k < sizeof(keys) \/ sizeof(keys\[0\])/k < (int)(sizeof(keys) \/ sizeof(keys[0]))/g' "$TRANS_VIZ_FILE"
    
    echo "Fixed signedness warnings in $TRANS_VIZ_FILE"
else
    echo "Error: $TRANS_VIZ_FILE not found!"
fi

# Schema file
SCHEMA_FILE="src/database/schema.c"
if [ -f "$SCHEMA_FILE" ]; then
    echo "Processing $SCHEMA_FILE..."
    
    # Fix type comparison
    sed -i 's/value->type != rule->params.type_value/value->type != (json_type_t)rule->params.type_value/g' "$SCHEMA_FILE"
    
    echo "Fixed signedness warnings in $SCHEMA_FILE"
else
    echo "Error: $SCHEMA_FILE not found!"
fi

# Config loader file
CONFIG_FILE="src/utils/config_loader.c"
if [ -f "$CONFIG_FILE" ]; then
    echo "Processing $CONFIG_FILE..."
    
    # Fix size_t and long comparison
    sed -i 's/read_size != file_size/(long)read_size != file_size/g' "$CONFIG_FILE"
    
    echo "Fixed signedness warnings in $CONFIG_FILE"
else
    echo "Error: $CONFIG_FILE not found!"
fi

echo "Completed fixing signedness comparison warnings."