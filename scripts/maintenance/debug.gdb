
set logging file gdb.log
set logging on
set pagination off
run --db-dir=/opt/jsondb/build/var --log-file=/opt/jsondb/build/var/jsondb_debug.log --log-level=trace --no-ssl

