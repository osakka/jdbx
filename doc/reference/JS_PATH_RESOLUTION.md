# JavaScript File Path Resolution

This document explains how JavaScript file paths are resolved in the JSON Database Server.

## Overview

The server provides enhanced JavaScript file path resolution to simplify the usage of JavaScript files. When you specify a JavaScript file using the `-js` flag or in API calls, the server will automatically search for the file in several locations, making it easier to use relative paths and helping you organize your JavaScript code.

## Path Resolution Process

When you specify a JavaScript file, the server will look for it in the following order:

1. **Cache Check**: First checks if this path was resolved previously (persistent cache)
2. **Direct Path**: Checks if the file exists exactly as specified
3. **Current Directory**: Looks in the current working directory
4. **Auto Extension**: Adds `.js` extension if not present and tries again
5. **Common JS Directories**: Searches in standard JS directories:
   - `functions/`
   - `validators/`
   - `transforms/`
   - `tests/js/`
   - `examples/js_extensions/`
6. **Project Root Path**: Tries to resolve the path relative to the project root directory

## Path Resolution Cache

For performance and consistency, the server maintains a cache of resolved JavaScript file paths. This cache is:

- **Persistent**: Saved to disk between server runs
- **Location**: Stored in `var/data/jsondb/js_path_cache.json`
- **Duration**: Cache entries expire after 24 hours
- **Validation**: Cached paths are validated before use to ensure the file still exists

## Debugging Path Resolution

If a JavaScript file cannot be found, the server will log detailed information about:

1. The original path that was requested
2. All locations that were searched
3. The current working directory

These logs can be found in the server log file and will also be output to the console when running in foreground mode.

## Best Practices

For most reliable operation:

1. **Absolute Paths**: Use absolute paths for critical scripts
2. **Standard Locations**: Store JavaScript files in the standard directories
3. **Extensions**: Always include the `.js` extension in filenames
4. **Validation**: Use the log output to verify path resolution when troubleshooting

## API Reference

### `js_file_find(filename, resolved_path, path_size)`

Core function used internally to resolve JavaScript file paths.

### `js_file_set_cache_path(path)`

Sets the location of the JavaScript path cache file.

### `js_file_save_cache()`

Explicitly saves the cache to disk (automatically called on server shutdown).

## Examples

Running a JavaScript file from different locations:

```bash
# Direct file path
bin/jsondb_server -js /home/user/myfile.js

# Relative path (will search in common directories)
bin/jsondb_server -js myfile.js

# Without extension (will add .js automatically)
bin/jsondb_server -js myfile
```

## Troubleshooting

If you're having issues with JavaScript file resolution:

1. Check the server log for detailed search paths
2. Try using an absolute path to the file
3. Verify file permissions (the server process must have read access)
4. Run the server with `-log-level debug` for more detailed logging
5. Clear the cache file if you suspect cache-related issues