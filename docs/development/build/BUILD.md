# Build Guide for JSON Database Server

This document provides detailed instructions for building the JSON Database Server from source, including all dependencies and configuration options.

## Prerequisites

### Required Dependencies

- **TCC** (Tiny C Compiler) >= 0.9.27
- **libuuid** >= 2.36
- **libssl** >= 1.1.1
- **libcrypto** >= 1.1.1
- **QuickJS** >= 2021-03-27

### Development Dependencies

- **Make** >= 4.0
- **pkg-config** >= 0.29
- **gcc** >= 8.0.0 (alternative to TCC)

### Installing Dependencies

#### On Debian/Ubuntu

```bash
# Install primary dependencies
sudo apt-get update
sudo apt-get install -y tcc libuuid-dev libssl-dev

# Install QuickJS (may require building from source)
wget https://bellard.org/quickjs/quickjs-2021-03-27.tar.xz
tar xf quickjs-2021-03-27.tar.xz
cd quickjs-2021-03-27
make
sudo make install
```

#### On RHEL/CentOS

```bash
# Install primary dependencies
sudo yum install -y tcc uuid-devel openssl-devel

# Install QuickJS (may require building from source)
wget https://bellard.org/quickjs/quickjs-2021-03-27.tar.xz
tar xf quickjs-2021-03-27.tar.xz
cd quickjs-2021-03-27
make
sudo make install
```

#### On macOS

```bash
# Install primary dependencies
brew install tcc ossp-uuid openssl

# Install QuickJS
brew install quickjs
```

## QuickJS Integration

The project requires the QuickJS JavaScript engine. You need to set up the QuickJS integration by following these steps:

### Option 1: Using a system-wide QuickJS installation (recommended)

If QuickJS is installed in a system directory (like `/opt/qjs`, `/usr/local`, or `/usr`):

1. Create a `js` directory in the `src/include` folder if it doesn't exist already:
   ```bash
   mkdir -p src/include/js
   ```
2. Create symbolic links to the QuickJS header files:
   ```bash
   # If QuickJS is installed in /opt/qjs
   ln -sf /opt/qjs/include/quickjs/*.h src/include/js/
   
   # Or if QuickJS is in /usr/local
   # ln -sf /usr/local/include/quickjs/*.h src/include/js/
   ```
3. Update the Makefile to include the QuickJS paths:
   ```bash
   # Add to CFLAGS
   -I/opt/qjs/include
   
   # Add to LDFLAGS
   -L/opt/qjs/lib -lquickjs
   ```

### Option 2: Using a local QuickJS installation

If you prefer to bundle QuickJS with the project:

1. Download QuickJS from https://bellard.org/quickjs/
2. Create a `js` directory in the `src/include` folder if it doesn't exist already:
   ```bash
   mkdir -p src/include/js
   ```
3. Copy the necessary header files to the `src/include/js` directory:
   ```bash
   cp /path/to/quickjs/quickjs.h src/include/js/
   cp /path/to/quickjs/quickjs-libc.h src/include/js/
   ```
4. Copy the library files to the `lib` directory:
   ```bash
   mkdir -p lib
   cp /path/to/quickjs/libquickjs.a lib/
   ```
5. Update the Makefile to link with the local library:
   ```bash
   # Change LDFLAGS to use the local library
   LDFLAGS = -pthread -lm -L./lib -lquickjs
   ```

## Building the Project

### Basic Build

To build the entire project (server and tools):

```bash
make
```

This will create the executable in the `bin` directory.

### Building Specific Components

To build only the server:

```bash
make server
```

To build only the tools:

```bash
make tools
```

### Clean Build

To clean the build artifacts and start fresh:

```bash
make clean
make
```

## Directory Structure

- **bin/**: Output directory for compiled binaries
- **obj/**: Object files generated during compilation
- **lib/**: External libraries and dependencies
- **include/**: Header files
- **src/**: Source code
  - **tools/**: Utility tools
  - **utils/**: Utility functions

## Build Customization

### Using a Different Compiler

The default compiler is TCC (Tiny C Compiler). To use GCC instead:

```bash
make CC=gcc
```

### Customizing Build Flags

You can customize the compiler flags:

```bash
make CFLAGS="-Wall -O2 -I./include"
```

### Customizing Linker Flags

You can customize the linker flags:

```bash
make LDFLAGS="-pthread -lm -luuid -lssl -lcrypto"
```

## Running the Server

After building, you can run the server using:

```bash
./bin/jsondb
```

Or using the make shortcut:

```bash
make run
```

## Troubleshooting Common Build Issues

### Missing QuickJS Headers

If you encounter an error about missing QuickJS headers:

1. Ensure you've properly installed QuickJS
2. Check that the header files are correctly placed in `include/quickjs/`
3. Verify that the library files are in the `lib/` directory

### Compiler Compatibility Issues

The project currently has some compatibility issues with different compilers:

#### TCC (Tiny C Compiler) Issues

When building with TCC, you might encounter errors related to regex.h:

```
/usr/include/regex.h:682: error: '__nmatch' undeclared
```

This is a known compatibility issue with TCC and the regex library.

#### GCC Issues

When building with GCC, you might encounter errors related to undefined types or functions:

```
unknown type name 'api_context_t'
'api_handle_transaction_logs_configure' undeclared
```

These issues indicate that some parts of the codebase are still under development or not properly integrated.

### Building without QuickJS

If you don't have QuickJS installed, you can build using the mock QuickJS implementation for **development and testing purposes only**:

1. Update the Makefile to use the mock implementation:

```
CC = gcc
CFLAGS = -Wall -I./include -DMOCK_QUICKJS -DTOOLS_BUILD
LDFLAGS = -pthread -lm
```

2. The mock implementation provides stub functions that fulfill the QuickJS API contracts but don't actually execute JavaScript code.

3. Build the project:

```bash
make clean && make
```

**⚠️ IMPORTANT WARNING ⚠️**

The mock QuickJS build has serious limitations:

- JavaScript functionality will be **completely non-functional** - all JS operations will silently fail
- Custom queries that use JavaScript will return empty results
- Document validation using JavaScript will be bypassed (all documents will pass validation)
- Document transformations will return unmodified documents
- Custom JavaScript functions will not execute

**This build should ONLY be used for:**
- Development of non-JavaScript features
- Testing basic database functionality
- CI/CD pipelines where JavaScript functionality is not being tested
- Building the project in environments where installing QuickJS is difficult

**For production use or when JavaScript functionality is needed, you MUST build with the actual QuickJS library.**

The database will start and function for basic operations, but any operations that depend on JavaScript execution will silently fail or return default values.

When running with the mock implementation, the server will log warnings when JavaScript functionality is attempted, and the metrics system will track these bypass events.

### SSL/TLS Support

If SSL/TLS support is not properly detected:

1. Ensure libssl-dev and libcrypto-dev are installed
2. Check that the correct library paths are being used
3. On some systems, you may need to specify the OpenSSL directory:
   ```bash
   make CFLAGS="-I/usr/local/opt/openssl/include" LDFLAGS="-L/usr/local/opt/openssl/lib"
   ```

### UUID Generation

If you encounter issues with UUID generation:

1. Verify libuuid is installed
2. Check the include and library paths for UUID
3. On some systems, you may need to use a different UUID library

## Advanced Build Options

### Debug Build

To build with debug symbols:

```bash
make CFLAGS="-Wall -g -I./include"
```

### Release Build

To build an optimized release version:

```bash
make CFLAGS="-Wall -O3 -DNDEBUG -I./include"
```

### Static Linking

To create a statically linked executable:

```bash
make LDFLAGS="-static -pthread -lm -luuid -lssl -lcrypto"
```

## Building for Specific Environments

### Building for Production

```bash
make CFLAGS="-Wall -O3 -DNDEBUG -I./include" LDFLAGS="-pthread -lm -luuid -lssl -lcrypto"
```

### Building for Development

```bash
make CFLAGS="-Wall -g -DDEBUG -I./include" LDFLAGS="-pthread -lm -luuid -lssl -lcrypto"
```

## Integration with CI/CD

The build process can be easily integrated with CI/CD systems:

```bash
# Example CI/CD script
set -e
make clean
make
make test  # If tests are available
```

## Next Steps

After building successfully:

1. Configure the server by creating a `config.json` file (see `config.json.example`)
2. Run the server with `./bin/jsondb` or `make run`
3. Access the API at `http://localhost:5000` (default)
4. Access the admin interface at `http://localhost:5000/admin`