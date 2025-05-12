#!/bin/bash
# Build script for JSONdb server
# This script builds the server with proper dependency management

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
QJS_DIR="/opt/qjs"
QJS_LIB_DIR="${QJS_DIR}/lib/quickjs"
QJS_INCLUDE_DIR="${QJS_DIR}/include"
WITH_JAVASCRIPT=1

# Parse command line arguments
for arg in "$@"; do
  case $arg in
    --no-js)
      WITH_JAVASCRIPT=0
      shift
      ;;
    --help)
      echo "Usage: $0 [OPTIONS]"
      echo "Build the JSONdb server"
      echo ""
      echo "Options:"
      echo "  --no-js     Build without JavaScript support"
      echo "  --help      Display this help and exit"
      exit 0
      ;;
  esac
done

# Print section header
print_section() {
    echo -e "\n${BLUE}===================================="
    echo -e " $1"
    echo -e "====================================${NC}"
}

# Print success message
print_success() {
    echo -e "${GREEN}SUCCESS: $1${NC}"
}

# Print error message
print_error() {
    echo -e "${RED}ERROR: $1${NC}"
    exit 1
}

# Print info message
print_info() {
    echo -e "${YELLOW}INFO: $1${NC}"
}

# Check if directory exists and create if not
ensure_dir() {
    if [ ! -d "$1" ]; then
        mkdir -p "$1"
        print_info "Created directory: $1"
    fi
}

# Check for required tools
check_dependencies() {
    print_section "Checking dependencies"
    
    # Check for gcc
    if ! command -v gcc &> /dev/null; then
        print_error "gcc not found. Please install gcc."
    fi
    print_info "gcc found: $(gcc --version | head -n1)"
    
    # Check for make
    if ! command -v make &> /dev/null; then
        print_error "make not found. Please install make."
    fi
    print_info "make found: $(make --version | head -n1)"
    
    # Check for QuickJS if building with JavaScript
    if [ $WITH_JAVASCRIPT -eq 1 ]; then
        if [ ! -d "$QJS_DIR" ]; then
            print_info "QuickJS not found at $QJS_DIR"
            install_quickjs
        else
            print_info "QuickJS found at $QJS_DIR"
        fi
        
        # Check for QuickJS library
        if [ ! -f "${QJS_LIB_DIR}/libquickjs.a" ] && [ ! -f "${QJS_LIB_DIR}/libquickjs.so" ]; then
            print_error "QuickJS library not found at ${QJS_LIB_DIR}"
        fi
    fi
    
    print_success "All dependencies satisfied"
}

# Install QuickJS
install_quickjs() {
    print_section "Installing QuickJS"
    
    # Create a temporary directory
    TMP_DIR=$(mktemp -d)
    print_info "Created temporary directory: $TMP_DIR"
    
    # Clone QuickJS repository
    print_info "Cloning QuickJS repository..."
    git clone https://github.com/bellard/quickjs.git $TMP_DIR/quickjs
    
    # Build QuickJS
    print_info "Building QuickJS..."
    cd $TMP_DIR/quickjs
    make
    
    # Install QuickJS
    print_info "Installing QuickJS to $QJS_DIR..."
    sudo mkdir -p $QJS_DIR/bin $QJS_DIR/lib/quickjs $QJS_DIR/include
    sudo cp qjs qjsc $QJS_DIR/bin/
    sudo cp libquickjs.a libquickjs.lto.a $QJS_DIR/lib/quickjs/
    sudo cp -r quickjs*.h $QJS_DIR/include/
    
    # Create symbolic link to library directory
    sudo ln -sf $QJS_DIR/lib/quickjs /usr/local/lib/quickjs
    
    # Clean up
    cd $PROJECT_ROOT
    rm -rf $TMP_DIR
    
    print_success "QuickJS installed successfully"
}

# Create build directories
create_build_dirs() {
    print_section "Creating build directories"
    
    ensure_dir "${BUILD_DIR}"
    ensure_dir "${BUILD_DIR}/bin"
    ensure_dir "${BUILD_DIR}/lib"
    ensure_dir "${BUILD_DIR}/obj"
    
    # Create component-specific object directories
    for dir in core api database query transaction rbac utils utils/memory tools js js/utils; do
        ensure_dir "${BUILD_DIR}/obj/$dir"
    done
    
    print_success "Build directories created"
}

# Build the library
build_library() {
    print_section "Building JSONdb library"
    
    # Set compiler flags based on JavaScript support
    if [ $WITH_JAVASCRIPT -eq 1 ]; then
        print_info "Building with JavaScript support"
        CFLAGS="-Wall -Wextra -I${PROJECT_ROOT}/include -I${QJS_INCLUDE_DIR}"
    else
        print_info "Building with JavaScript disabled"
        CFLAGS="-Wall -Wextra -I${PROJECT_ROOT}/include -DDISABLE_JS"
    fi
    
    # Add debug symbols in development
    CFLAGS="$CFLAGS -g"
    
    # Component directories
    COMPONENTS=(
        "core"
        "api"
        "database"
        "query"
        "transaction"
        "rbac"
        "utils"
        "utils/memory"
        "tools"
        "js"
        "js/utils"
    )
    
    # Compile each component
    for component in "${COMPONENTS[@]}"; do
        print_info "Compiling component: $component"
        
        # Get source files
        SRC_FILES=$(find "${PROJECT_ROOT}/src/${component}" -name "*.c")
        
        # Compile each source file
        for src in $SRC_FILES; do
            obj_file="${BUILD_DIR}/obj/${component}/$(basename ${src%.c}.o)"
            print_info "Compiling: $src -> $obj_file"
            
            gcc $CFLAGS -c "$src" -o "$obj_file"
            
            if [ $? -ne 0 ]; then
                print_error "Failed to compile $src"
            fi
        done
    done
    
    # Create a list of all object files
    OBJ_FILES=$(find "${BUILD_DIR}/obj" -name "*.o")
    
    # Create the static library
    print_info "Creating static library: ${BUILD_DIR}/lib/libjsondb.a"
    ar rcs "${BUILD_DIR}/lib/libjsondb.a" $OBJ_FILES
    
    if [ $? -ne 0 ]; then
        print_error "Failed to create static library"
    fi
    
    print_success "JSONdb library built successfully"
}

# Build the server
build_server() {
    print_section "Building JSONdb server"
    
    # Set linker flags based on JavaScript support
    if [ $WITH_JAVASCRIPT -eq 1 ]; then
        LDFLAGS="-L${QJS_LIB_DIR} -lquickjs -lm -lpthread"
        # Add QuickJS library directory to library path
        export LD_LIBRARY_PATH="${QJS_LIB_DIR}:$LD_LIBRARY_PATH"
    else
        LDFLAGS="-lm -lpthread"
    fi
    
    # Set compiler flags
    if [ $WITH_JAVASCRIPT -eq 1 ]; then
        CFLAGS="-Wall -Wextra -I${PROJECT_ROOT}/include -I${QJS_INCLUDE_DIR}"
    else
        CFLAGS="-Wall -Wextra -I${PROJECT_ROOT}/include -DDISABLE_JS"
    fi
    
    # Add debug symbols in development
    CFLAGS="$CFLAGS -g"
    
    # Build the server
    print_info "Compiling server..."
    gcc $CFLAGS -o "${BUILD_DIR}/bin/jsondb_server" "${PROJECT_ROOT}/src/core/main.c" \
        "${BUILD_DIR}/lib/libjsondb.a" $LDFLAGS
    
    if [ $? -ne 0 ]; then
        print_error "Failed to build server"
    fi
    
    print_success "JSONdb server built successfully"
    print_info "Server binary: ${BUILD_DIR}/bin/jsondb_server"
}

# Copy configuration files
copy_config_files() {
    print_section "Copying configuration files"
    
    # Create config directory
    ensure_dir "${BUILD_DIR}/etc"
    
    # Copy example configuration files
    if [ -f "${PROJECT_ROOT}/config.json.example" ]; then
        cp "${PROJECT_ROOT}/config.json.example" "${BUILD_DIR}/etc/config.json"
        print_info "Copied config.json"
    fi
    
    if [ -f "${PROJECT_ROOT}/auth.json.example" ]; then
        cp "${PROJECT_ROOT}/auth.json.example" "${BUILD_DIR}/etc/auth.json"
        print_info "Copied auth.json"
    fi
    
    if [ -f "${PROJECT_ROOT}/rbac.json.example" ]; then
        cp "${PROJECT_ROOT}/rbac.json.example" "${BUILD_DIR}/etc/rbac.json"
        print_info "Copied rbac.json"
    fi
    
    if [ -f "${PROJECT_ROOT}/db.json.example" ]; then
        cp "${PROJECT_ROOT}/db.json.example" "${BUILD_DIR}/etc/db.json"
        print_info "Copied db.json"
    fi
    
    print_success "Configuration files copied"
}

# Create run script to simplify execution
create_run_script() {
    print_section "Creating run script"
    
    RUN_SCRIPT="${BUILD_DIR}/run_server.sh"
    
    cat > $RUN_SCRIPT << EOL
#!/bin/bash
# Run script for JSONdb server

# Go to the build directory
cd "\$(dirname "\$0")"

# Add QuickJS library directory to library path if needed
if [ -d "${QJS_LIB_DIR}" ]; then
    export LD_LIBRARY_PATH="${QJS_LIB_DIR}:\$LD_LIBRARY_PATH"
fi

# Run the server
./bin/jsondb_server "\$@"
EOL
    
    chmod +x $RUN_SCRIPT
    
    print_success "Run script created: $RUN_SCRIPT"
}

# Main function
main() {
    print_section "Building JSONdb Server"
    
    if [ $WITH_JAVASCRIPT -eq 1 ]; then
        print_info "Building with JavaScript support"
    else
        print_info "Building with JavaScript disabled"
    fi
    
    # Change to project root
    cd $PROJECT_ROOT
    
    # Check dependencies
    check_dependencies
    
    # Create build directories
    create_build_dirs
    
    # Build the library
    build_library
    
    # Build the server
    build_server
    
    # Copy configuration files
    copy_config_files
    
    # Create run script
    create_run_script
    
    print_success "Build completed successfully!"
    echo -e "\nTo run the server, use: ${BUILD_DIR}/run_server.sh"
}

# Execute the main function
main