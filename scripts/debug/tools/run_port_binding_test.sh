#!/bin/bash

# Default port
PORT=5000

# Check if port argument is provided
if [ $# -gt 0 ]; then
    PORT=$1
fi

# Run the test
"$(dirname "$0")/test_port_binding" $PORT

# Display instructions
echo
echo "Port binding test complete. Check the log file at /tmp/port_binding_test_${PORT}.log"
echo "To connect to the server manually, use: curl http://localhost:${PORT}"
echo
