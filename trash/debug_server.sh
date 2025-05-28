#!/bin/bash
# Debug wrapper for JSONdb server

# Enable core dumps
ulimit -c unlimited

# Set core dump pattern
echo "Core dumps enabled at: /tmp/core.jsondb.%p"

# Run with strace to trace system calls
echo "Starting JSONdb server with system call tracing..."
echo "Trace output will be in: /opt/jsondb/build/var/jsondb_trace.log"

# Start server in foreground with strace
cd /opt/jsondb
strace -f -o /opt/jsondb/build/var/jsondb_trace.log -e trace=signal,exit_group,kill build/bin/jsondb_server \
    --port 5000 \
    --host 0.0.0.0 \
    --database /opt/jsondb/build/var/database.jdb \
    --rbac-file /opt/jsondb/build/var/rbac.json \
    --log-file /opt/jsondb/build/var/jsondb.log \
    --log-level debug \
    --web-root /opt/jsondb/share/htdocs \
    --pid-file /opt/jsondb/build/var/jsondb.pid \
    --validators-dir /opt/jsondb/build/var/validators \
    --transforms-dir /opt/jsondb/build/var/transforms \
    --metrics-dir /opt/jsondb/build/var/metrics \
    --foreground