
#\!/bin/bash
cd /opt/jsondb/src
build/bin/jsondb_server --db-dir=/opt/jsondb/build/var --log-file=/opt/jsondb/build/var/jsondb_debug.log --log-level=trace --no-ssl &
PID=$\!
echo "Server PID: $PID"
sleep 2
echo "Attaching GDB to PID $PID"
gdb -p $PID -batch -ex 'set pagination off' -ex 'bt' -ex 'info threads' -ex 'detach'
wait $PID

