# unit Tests

This directory contains tests for the unit components of JDBX.

## Test Files

- `json_clone_test.c`
- `logger_test.c`
- `simple_logger_test.c`
- `test_database.c`
- `test_input_validation.c`
- `test_json.c`
- `test_json_helpers_simple.c`
- `test_refcount.c`
- `test_refcount_complex.c`
- `test_refcount_debug.c`
- `test_refcount_nested.c`
- `test_refcount_shared.c`
- `test_refcount_simple.c`
- `test_refcount_simple_js.c`
- `test_transaction.c`

## Makefiles

- `Makefile.json_clone`
- `Makefile.unit_test`

## Running Tests

Use the appropriate Makefile to run these tests, for example:

```
make -f Makefile.<testname>
```
