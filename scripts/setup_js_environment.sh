#!/bin/bash
# Setup script for JSONdb JavaScript environment

# Define color codes
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Define directories
BASE_DIR=$(pwd)
JS_EXAMPLES_DIR="$BASE_DIR/share/examples/js-examples"
FUNCTIONS_DIR="$BASE_DIR/functions"
VALIDATORS_DIR="$BASE_DIR/validators"
TRANSFORMS_DIR="$BASE_DIR/transforms"

echo -e "${BLUE}Setting up JSONdb JavaScript environment...${NC}"

# Create directories if they don't exist
mkdir -p "$FUNCTIONS_DIR"
mkdir -p "$VALIDATORS_DIR"
mkdir -p "$TRANSFORMS_DIR"

echo -e "${GREEN}Created JavaScript directories:${NC}"
echo "- $FUNCTIONS_DIR"
echo "- $VALIDATORS_DIR"
echo "- $TRANSFORMS_DIR"

# Copy example files if available
if [ -d "$JS_EXAMPLES_DIR" ]; then
  echo -e "${BLUE}Installing example files...${NC}"
  
  # Copy validator example
  if [ -f "$JS_EXAMPLES_DIR/validator_example.js" ]; then
    cp "$JS_EXAMPLES_DIR/validator_example.js" "$VALIDATORS_DIR/example_validator.js"
    echo "Installed example validator to $VALIDATORS_DIR/example_validator.js"
  fi
  
  # Copy transformer example
  if [ -f "$JS_EXAMPLES_DIR/transformer_example.js" ]; then
    cp "$JS_EXAMPLES_DIR/transformer_example.js" "$TRANSFORMS_DIR/example_transformer.js"
    echo "Installed example transformer to $TRANSFORMS_DIR/example_transformer.js"
  fi
  
  # Copy function example
  if [ -f "$JS_EXAMPLES_DIR/function_example.js" ]; then
    cp "$JS_EXAMPLES_DIR/function_example.js" "$FUNCTIONS_DIR/example_function.js"
    echo "Installed example function to $FUNCTIONS_DIR/example_function.js"
  fi
else
  echo "Example directory not found. Skipping example installation."
fi

echo -e "${GREEN}JavaScript environment setup complete!${NC}"
echo "You can now create JavaScript files in these directories:"
echo "- Functions: $FUNCTIONS_DIR/*.js"
echo "- Validators: $VALIDATORS_DIR/<collection_name>.js"
echo "- Transformers: $TRANSFORMS_DIR/<collection_name>.js"
echo
echo "For more information, see the documentation at:"
echo "- docs/guides/javascript_functions.md"
echo "- docs/api/JAVASCRIPT_API.md"