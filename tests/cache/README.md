# cache Tests

This directory contains tests for the cache components of JDBX.

## Test Files

- `cache_extract.c`
- `cache_insert_test.c`
- `cache_operation_test.c`
- `cache_timeout_test.c`
- `simple_cache_test.c`
- `test_cache_disabled.c`
- `verify_cache_disabled.c`

## Makefiles

- `Makefile.cache_disabled`
- `Makefile.cache_insert`
- `Makefile.cache_ops`
- `Makefile.cache_timeout`
- `Makefile.simple_cache`

## Running Tests

Use the appropriate Makefile to run these tests, for example:

```
make -f Makefile.<testname>
```
