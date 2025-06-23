# JDBX Scripts

This directory contains all scripts for testing, examples, and tools.

## Directory Structure

### `/scripts/tests/`
Testing and performance scripts:
- `analyze_db_performance.sh` - Database performance analysis
- `test_skiplist_performance.sh` - Skiplist performance testing
- `test_memory_simple.sh` - Memory usage testing
- `test_cache_performance.sh` - Cache performance benchmarks
- `compile_test_refcount.sh` - Reference counting tests
- `test_jwt_performance.sh` - JWT performance testing
- `test_js_integration.sh` - JavaScript integration tests
- `test_jwt_cache.sh` - JWT cache testing

### `/scripts/examples/`
Example scripts demonstrating JDBX usage:
- `api_test.sh` - API endpoint testing examples
- `basic_test.sh` - Basic functionality tests
- `curl_examples.sh` - cURL command examples
- `test_indexes.sh` - Index testing examples
- `visualization_examples.sh` - Data visualization examples
- `setup_welcome_panel.sh` - Welcome panel setup
- `install_examples.sh` - Install JavaScript examples

### `/scripts/tools/`
Utility and migration tools:
- `migrate_to_new_structure.sh` - Database migration helper

## Usage

All scripts should be run from the project root:

```bash
# Run a test script
./scripts/tests/test_cache_performance.sh

# Run an example
./scripts/examples/curl_examples.sh

# Run a tool
./scripts/tools/migrate_to_new_structure.sh
```

## Runtime Script

The main runtime script remains in its original location:
- `build/jdbx_runtime.sh` - Start/stop/status for JDBX daemon