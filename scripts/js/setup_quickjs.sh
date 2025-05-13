#!/bin/bash
#
# QuickJS Setup Script
#
# This script checks if QuickJS is already installed in /opt/qjs
# If not, it downloads, builds, and installs QuickJS to that location
#
# Usage: ./setup_quickjs.sh
#        ./setup_quickjs.sh [quickjs_version]
#
# Default QuickJS version: 2021-03-27

set -e

# Default QuickJS version
QUICKJS_VERSION=${1:-"2021-03-27"}
QUICKJS_URL="https://bellard.org/quickjs/quickjs-${QUICKJS_VERSION}.tar.xz"
QUICKJS_INSTALL_DIR="/opt/qjs"
TEMP_DIR="/tmp/quickjs-build"

echo "QuickJS Setup Script"
echo "================================"
echo "Version: ${QUICKJS_VERSION}"
echo "Installation directory: ${QUICKJS_INSTALL_DIR}"
echo

# Check if QuickJS is already installed
if [ -f "${QUICKJS_INSTALL_DIR}/lib/quickjs/libquickjs.a" ] && [ -f "${QUICKJS_INSTALL_DIR}/include/quickjs/quickjs.h" ]; then
    echo "QuickJS is already installed at ${QUICKJS_INSTALL_DIR}"
    echo "If you want to reinstall, remove the directory first with:"
    echo "sudo rm -rf ${QUICKJS_INSTALL_DIR}"
    exit 0
fi

# Check if we have sudo rights
if [ "$(id -u)" -ne 0 ]; then
    echo "This script requires sudo privileges to install to ${QUICKJS_INSTALL_DIR}"
    echo "Please enter your password when prompted"
    
    # Test sudo access
    sudo -v
    if [ $? -ne 0 ]; then
        echo "Error: sudo privileges are required for this script"
        exit 1
    fi
fi

# Create temporary build directory
echo "Creating temporary build directory..."
mkdir -p "${TEMP_DIR}"
cd "${TEMP_DIR}"

# Download QuickJS
echo "Downloading QuickJS ${QUICKJS_VERSION}..."
if command -v wget > /dev/null; then
    wget -q "${QUICKJS_URL}" -O "quickjs-${QUICKJS_VERSION}.tar.xz"
elif command -v curl > /dev/null; then
    curl -s "${QUICKJS_URL}" -o "quickjs-${QUICKJS_VERSION}.tar.xz"
else
    echo "Error: Neither wget nor curl is available. Please install one of them."
    exit 1
fi

# Check if download was successful
if [ ! -f "quickjs-${QUICKJS_VERSION}.tar.xz" ]; then
    echo "Error: Failed to download QuickJS. Please check your internet connection and the URL:"
    echo "${QUICKJS_URL}"
    exit 1
fi

# Extract the tarball
echo "Extracting QuickJS..."
tar -xf "quickjs-${QUICKJS_VERSION}.tar.xz"
cd "quickjs-${QUICKJS_VERSION}"

# Build QuickJS
echo "Building QuickJS..."
make -j$(nproc)

# Create installation directory
echo "Creating installation directory..."
sudo mkdir -p "${QUICKJS_INSTALL_DIR}/bin"
sudo mkdir -p "${QUICKJS_INSTALL_DIR}/lib/quickjs"
sudo mkdir -p "${QUICKJS_INSTALL_DIR}/include/quickjs"
sudo mkdir -p "${QUICKJS_INSTALL_DIR}/src"

# Install QuickJS binaries
echo "Installing QuickJS binaries..."
sudo cp qjs qjsc "${QUICKJS_INSTALL_DIR}/bin/"

# Install QuickJS library
echo "Installing QuickJS library..."
sudo cp libquickjs.a "${QUICKJS_INSTALL_DIR}/lib/quickjs/"

# Install QuickJS headers
echo "Installing QuickJS headers..."
sudo cp quickjs.h quickjs-libc.h "${QUICKJS_INSTALL_DIR}/include/quickjs/"

# Copy source files for reference
echo "Copying source files for reference..."
sudo cp -r . "${QUICKJS_INSTALL_DIR}/src/"

# Clean up
echo "Cleaning up..."
cd "${TEMP_DIR}/.."
rm -rf "${TEMP_DIR}"

echo
echo "QuickJS ${QUICKJS_VERSION} has been successfully installed to ${QUICKJS_INSTALL_DIR}"
echo
echo "To use QuickJS in your Makefile, add the following flags:"
echo "CFLAGS += -I${QUICKJS_INSTALL_DIR}/include"
echo "LDFLAGS += -L${QUICKJS_INSTALL_DIR}/lib/quickjs -lquickjs"
echo