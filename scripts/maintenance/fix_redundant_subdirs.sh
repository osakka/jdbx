#!/bin/bash
# Script to fix redundant subdirectories in the project
# This removes duplication in memory/ and js/utils/ directories

set -e  # Exit on error

PROJ_ROOT="/home/claude-3/project"
SRC_COMP_DIR="$PROJ_ROOT/src/components"

echo "Fixing redundant subdirectories..."

# 1. Verify the redundant files in utils/memory/ are identical to those in utils/
echo "Verifying redundant files..."
diff -q "$SRC_COMP_DIR/utils/memory/ref_counter.c" "$SRC_COMP_DIR/utils/ref_counter.c" || { echo "ERROR: ref_counter.c files differ!"; exit 1; }
diff -q "$SRC_COMP_DIR/utils/memory/ref_json.c" "$SRC_COMP_DIR/utils/ref_json.c" || { echo "ERROR: ref_json.c files differ!"; exit 1; }

# 2. Remove redundant subdirectories
echo "Removing redundant subdirectories and files..."
rm -f "$SRC_COMP_DIR/utils/memory/ref_counter.c"
rm -f "$SRC_COMP_DIR/utils/memory/ref_json.c"

# 3. Remove empty directories
rmdir "$SRC_COMP_DIR/utils/memory/" 2>/dev/null || true
rmdir "$SRC_COMP_DIR/js/utils/" 2>/dev/null || true

# 4. Update the Makefile
echo "Updating Makefile..."
sed -i 's|$(wildcard $(SRC_DIR)/components/utils/\*\.c) $(wildcard $(SRC_DIR)/components/utils/memory/\*\.c)|$(wildcard $(SRC_DIR)/components/utils/\*\.c)|g' "$PROJ_ROOT/src/Makefile"
sed -i 's|$(wildcard $(SRC_DIR)/components/js/\*\.c) $(wildcard $(SRC_DIR)/components/js/utils/\*\.c)|$(wildcard $(SRC_DIR)/components/js/\*\.c)|g' "$PROJ_ROOT/src/Makefile"

# 5. Remove directory creation lines
sed -i '/^.*mkdir -p $(OBJ_DIR)\/components\/utils\/memory/d' "$PROJ_ROOT/src/Makefile"
sed -i '/^.*mkdir -p $(OBJ_DIR)\/components\/js\/utils/d' "$PROJ_ROOT/src/Makefile"

echo "Redundant subdirectories fixed successfully."