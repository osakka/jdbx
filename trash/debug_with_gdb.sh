#\!/bin/bash
cd /opt/jsondb

echo "Starting JSONdb under GDB..."

# Create gdb commands
cat > gdb_commands.txt << 'GDBEOF'
set pagination off
set logging on
catch signal SIGSEGV
catch signal SIGABRT
catch signal SIGBUS
set environment DB_PATH=/opt/jsondb/build/var/database.jdb
set environment RBAC_FILE=/opt/jsondb/build/var/rbac.json
set environment LOG_FILE=/opt/jsondb/build/var/jsondb.log
set environment PID_FILE=/opt/jsondb/build/var/jsondb.pid
set environment WEB_ROOT=/opt/jsondb/share/htdocs
set environment PORT=5000
set environment HOST=0.0.0.0
set environment LOG_LEVEL=debug
set environment VALIDATORS_DIR=/opt/jsondb/build/var/validators
set environment TRANSFORMS_DIR=/opt/jsondb/build/var/transforms
set environment METRICS_DIR=/opt/jsondb/build/var/metrics
run -d
continue
bt full
info registers
info locals
quit
GDBEOF

gdb -x gdb_commands.txt build/bin/jsondb_server
