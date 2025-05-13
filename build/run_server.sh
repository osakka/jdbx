#!/bin/bash
# Run script for JSONdb server

# Go to the build directory
cd "$(dirname "$0")"

# Add QuickJS library directory to library path if needed
if [ -d "/opt/qjs/lib/quickjs" ]; then
    export LD_LIBRARY_PATH="/opt/qjs/lib/quickjs:$LD_LIBRARY_PATH"
fi

# Run the server
./bin/jsondb_server "$@"
