# database Tests

This directory contains tests for the database components of JDBX.

## Test Files

- `db_cache_debug.c`
- `db_collection_test.c`
- `db_deadlock_test.c`
- `db_init_test.c`
- `db_insert_test.c`
- `db_minimal_insert.c`
- `db_timeout_test.c`
- `minimal_db_test.c`
- `minimal_insert_test.c`
- `quick_insert.c`
- `simple_insert_test.c`

## Makefiles

- `Makefile.db_collection`
- `Makefile.db_deadlock`
- `Makefile.db_debug`
- `Makefile.db_init`
- `Makefile.db_insert`
- `Makefile.insert_cache`
- `Makefile.insert_debug`
- `Makefile.minimal_db`
- `Makefile.minimal_insert`
- `Makefile.simple_insert`

## Running Tests

Use the appropriate Makefile to run these tests, for example:

```
make -f Makefile.<testname>
```
