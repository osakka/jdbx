# JavaScript Conditional Compilation

This document explains how JavaScript support can be conditionally compiled in the JSONdb project.

## Overview

The JSONdb project supports building with or without JavaScript functionality. This allows for:

1. Reduced binary size when JavaScript is not needed
2. Reduced external dependencies (no need for QuickJS)
3. Simplified deployment in environments where JavaScript is not required

## How to Build

### Build with JavaScript (default)

```bash
make
```

### Build without JavaScript

```bash
make CFLAGS=-DDISABLE_JS
```

## Implementation Details

The conditional compilation is implemented using the `DISABLE_JS` preprocessor macro. When this macro is defined, the JavaScript functionality is disabled and stub implementations are provided for API compatibility.

Example:

```c
#ifndef DISABLE_JS
    // JavaScript functionality
    js_engine_t* engine = js_engine_init(db);
    // ...
#else /* JavaScript functionality disabled */
    // Stub implementation
    fprintf(stderr, "JavaScript support is not available in this build\n");
#endif /* DISABLE_JS */
```

## Files with Conditional Compilation

The following files implement JavaScript conditional compilation:

1. `src/core/main.c` - Main entry point with JavaScript initialization
2. `src/js/js_api.c` - JavaScript API implementation with HTTP handlers
3. `src/js/js_engine.c` - JavaScript engine implementation using QuickJS
4. `src/utils/js_file_utils.c` - JavaScript file utility functions
5. `src/utils/config_loader.c` - Configuration loader with JavaScript settings

## Testing Conditional Compilation

Two scripts are provided to test the JavaScript conditional compilation:

1. `scripts/test_js_compilation_modes.sh` - Tests the compilation of a simple file with both modes
2. `scripts/build_test_js_modes.sh` - Tests building the entire project with both modes

## Best Practices

When adding new JavaScript-related functionality, follow these guidelines:

1. Always wrap JavaScript-specific code with `#ifndef DISABLE_JS` / `#endif /* DISABLE_JS */` blocks
2. Provide stub implementations for functions when JavaScript is disabled
3. Use meaningful error messages in stub implementations to indicate JavaScript is disabled
4. Test the code with both JavaScript enabled and disabled to ensure it compiles and works correctly

## Internal Implementation

For most JavaScript functionality, we provide two implementations:

1. The full implementation when JavaScript is enabled
2. A stub implementation when JavaScript is disabled

The stub implementations typically return error codes or error messages indicating that JavaScript is not available in the current build.

## HTTP API Handlers

All JavaScript-related HTTP API handlers (`api_handle_js_*`) follow this pattern:

```c
http_response_t* api_handle_js_function(api_context_t* ctx, http_request_t* request) {
#ifndef DISABLE_JS
    // Full implementation
    // ...
#else /* JavaScript functionality disabled */
    // Return an error response
    return create_http_response(HTTP_NOT_IMPLEMENTED,
                             "{\"error\":\"JavaScript functionality is not available in this build\"}",
                             "application/json");
#endif /* DISABLE_JS */
}
```

This ensures that the API contract is maintained regardless of the build configuration.