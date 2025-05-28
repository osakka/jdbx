#!/bin/bash
# Debug runner for JSONdb that monitors for crashes

echo "Starting JSONdb server with crash monitoring..."
echo "Server will run normally. When it crashes, we'll see the backtrace."
echo ""
echo "To reproduce the crash:"
echo "1. Browse the UI normally"
echo "2. Click on different menu items"
echo "3. View different collections"
echo ""

# Enable core dumps
ulimit -c unlimited

# Clean up any stale PID
rm -f /opt/jsondb/build/var/jsondb.pid

# Start the server in background with monitoring
cd /opt/jsondb

# Create a monitoring script
cat > /tmp/monitor_jsondb.sh << 'EOF'
#!/bin/bash
PID=$1
echo "Monitoring PID $PID for crashes..."

while kill -0 $PID 2>/dev/null; do
    sleep 1
done

echo ""
echo "=== SERVER CRASHED ==="
echo "Checking for core dump..."

# Look for core dump
CORE=$(find /tmp -name "core.$PID" -o -name "core" | head -1)
if [ -n "$CORE" ]; then
    echo "Core dump found: $CORE"
    echo "Analyzing with gdb..."
    gdb -batch -ex "bt full" -ex "info registers" -ex "info threads" build/bin/jsondb_server $CORE
else
    echo "No core dump found. Checking dmesg..."
    dmesg | tail -20 | grep -i jsondb || echo "No kernel messages found"
fi
EOF

chmod +x /tmp/monitor_jsondb.sh

# Start server normally
echo "Starting server..."
build/bin/jsondb_server \
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
    --metrics-dir /opt/jsondb/build/var/metrics &

SERVER_PID=$!
echo "Server started with PID: $SERVER_PID"

# Monitor for crash
/tmp/monitor_jsondb.sh $SERVER_PID