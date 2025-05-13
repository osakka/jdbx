# integration Tests

This directory contains tests for the integration components of JSONdb.

## Test Files

- `health_server.c`
- `test_deadlock_detection.c`
- `test_js_integration.c`
- `test_server.c`
- `test_server_api.c`

## Makefiles

- `Makefile.deadlock_fix`
- `Makefile.deadlock_free`
- `Makefile.deadlock_test`

## Running Tests

Use the appropriate Makefile to run these tests, for example:

```
make -f Makefile.<testname>
```
