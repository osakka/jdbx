#!/bin/bash

# JSONdb JavaScript Examples Installation Script
# This script installs example JavaScript validators, transformers, and functions
# into the JSONdb system using the API.

set -e

# Configuration
JSONDB_HOST="${JSONDB_HOST:-localhost}"
JSONDB_PORT="${JSONDB_PORT:-5000}"
BASE_URL="http://${JSONDB_HOST}:${JSONDB_PORT}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if JSONdb server is running
check_server() {
    print_status "Checking if JSONdb server is running..."
    if curl -s "${BASE_URL}/api/health" > /dev/null; then
        print_success "JSONdb server is running at ${BASE_URL}"
    else
        print_error "JSONdb server is not accessible at ${BASE_URL}"
        print_error "Please ensure the server is running and check JSONDB_HOST and JSONDB_PORT"
        exit 1
    fi
}

# Function to install a script document
install_script() {
    local collection=$1
    local script_json=$2
    local script_name=$3
    
    print_status "Installing ${script_name} to ${collection}..."
    
    response=$(curl -s -X POST "${BASE_URL}/api/collections/${collection}" \
        -H "Content-Type: application/json" \
        -d "$script_json")
    
    if echo "$response" | grep -q '"success".*true\|"id"'; then
        print_success "Successfully installed ${script_name}"
    else
        print_warning "Failed to install ${script_name}: $response"
    fi
}

# Email Validator
install_email_validator() {
    local script_json='{
        "name": "Email Validator Example",
        "description": "Validates email format using regex pattern",
        "type": "validator",
        "tags": ["users", "contacts"],
        "enabled": true,
        "code": "function validate(document, context) {\n    const errors = [];\n    const warnings = [];\n    \n    if (!document.email) {\n        errors.push('\''Email is required'\'');\n    } else {\n        const emailRegex = /^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$/;\n        if (!emailRegex.test(document.email)) {\n            errors.push('\''Invalid email format'\'');\n        }\n    }\n    \n    return {\n        valid: errors.length === 0,\n        errors,\n        warnings\n    };\n}",
        "version": "1.0.0",
        "author": "JSONdb Examples"
    }'
    
    install_script "_validators" "$script_json" "Email Validator"
}

# User Data Normalizer
install_user_normalizer() {
    local script_json='{
        "name": "User Data Normalizer Example",
        "description": "Normalizes user data - lowercase email, title case names",
        "type": "transformer",
        "tags": ["users", "contacts"],
        "enabled": true,
        "code": "function transform(document, context) {\n    const result = { ...document };\n    \n    // Normalize email to lowercase\n    if (result.email) {\n        result.email = result.email.toLowerCase().trim();\n    }\n    \n    // Title case for names\n    ['\''firstName'\'', '\''lastName'\'', '\''name'\''].forEach(field => {\n        if (result[field]) {\n            result[field] = result[field]\n                .toLowerCase()\n                .split('\'' '\'')\n                .map(word => word.charAt(0).toUpperCase() + word.slice(1))\n                .join('\'' '\'')\n                .trim();\n        }\n    });\n    \n    return result;\n}",
        "version": "1.0.0",
        "author": "JSONdb Examples"
    }'
    
    install_script "_transformers" "$script_json" "User Data Normalizer"
}

# Statistics Calculator Function
install_stats_function() {
    local script_json='{
        "name": "Statistics Calculator Example",
        "description": "Calculates mean, median, mode for numeric arrays",
        "type": "function",
        "tags": ["analytics", "math"],
        "enabled": true,
        "code": "function execute(input, context) {\n    const { data, field } = input;\n    \n    if (!Array.isArray(data)) {\n        throw new Error('\''Data must be an array'\'');\n    }\n    \n    let numbers;\n    if (field) {\n        numbers = data.map(item => parseFloat(item[field])).filter(n => !isNaN(n));\n    } else {\n        numbers = data.map(n => parseFloat(n)).filter(n => !isNaN(n));\n    }\n    \n    if (numbers.length === 0) {\n        return { error: '\''No valid numeric data found'\'' };\n    }\n    \n    // Calculate mean\n    const mean = numbers.reduce((sum, n) => sum + n, 0) / numbers.length;\n    \n    // Calculate median\n    const sorted = [...numbers].sort((a, b) => a - b);\n    const median = sorted.length % 2 === 0\n        ? (sorted[sorted.length / 2 - 1] + sorted[sorted.length / 2]) / 2\n        : sorted[Math.floor(sorted.length / 2)];\n    \n    return {\n        count: numbers.length,\n        mean: Math.round(mean * 100) / 100,\n        median: Math.round(median * 100) / 100,\n        min: Math.min(...numbers),\n        max: Math.max(...numbers)\n    };\n}",
        "version": "1.0.0",
        "author": "JSONdb Examples"
    }'
    
    install_script "_functions" "$script_json" "Statistics Calculator"
}

# Main installation function
main() {
    echo ""
    print_status "JSONdb JavaScript Examples Installation"
    print_status "======================================"
    echo ""
    
    # Check server connectivity
    check_server
    echo ""
    
    # Install examples
    print_status "Installing example scripts..."
    echo ""
    
    install_email_validator
    install_user_normalizer  
    install_stats_function
    
    echo ""
    print_success "Installation complete!"
    echo ""
    print_status "Next steps:"
    echo "  1. Open JSONdb browser interface at ${BASE_URL}"
    echo "  2. Navigate to _validators, _transformers, or _functions collections"
    echo "  3. View the installed example scripts"
    echo "  4. Test validators by creating documents in collections like 'users'"
    echo "  5. Test transformers using the transformation preview feature"
    echo "  6. Execute functions using the /api/js/native/execute endpoint"
    echo ""
    print_status "For more examples and documentation, see:"
    echo "  - comprehensive_examples.js (complete examples library)"
    echo "  - README.md (usage guide)"
    echo "  - /opt/jsondb/docs/guides/javascript-development-guide.md"
    echo ""
}

# Run main function
main "$@"