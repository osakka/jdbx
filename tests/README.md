# Tests Directory

This directory contains all tests for the JDBX project, organized by component and type.

## Directory Structure

- **unit/**: Unit tests for individual components
  - JSON parser and formatter tests
  - Logger tests
  - Reference counting tests
  - Memory management tests

- **integration/**: Integration tests between components
  - Server API tests
  - Deadlock detection tests
  - Full server test suite

- **performance/**: Performance and stress tests
  - Multi-threaded operation tests
  - Database performance tests
  - API performance tests

- **js/**: JavaScript integration tests
  - JS engine tests
  - JS API tests
  - JS file utils tests

- **database/**: Database component tests
  - Database initialization tests
  - CRUD operation tests

- **discovery/**: System discovery and edge case tests
  - Next improvement opportunity identification
  - Edge case discovery tests

## Recent Test Additions

### HTTP Protocol Compliance Tests
- **test_n1_comprehensive.py**: Comprehensive N-1 byte buffer fix validation
- **test_n1_fix.py**: Focused N-1 byte issue testing
- **test_curl_n1.sh**: Curl-specific N-1 byte testing

### Discovery Tests
- **discovery/next_discovery_test.sh**: Automated discovery of next improvement opportunities
  - Query execution tests
  - Database locking tests

- **cache/**: Cache component tests
  - Cache operations tests
  - Cache timeout tests
  - Query cache tests

- **scripts/**: Test utility scripts
  - Test report generation scripts
  - Test data generation scripts

## Running Tests

The main Makefile contains targets for running different test suites. Individual test directories also contain specific Makefiles for running those tests.

To run all tests:
```
make tests
```

To run specific test suites, use the appropriate Makefile in the relevant directory.

## Adding New Tests

1. Place new tests in the appropriate directory based on what they are testing
2. Create or update a Makefile for building and running the test
3. Update this README if adding a new category of tests
EOF < /dev/null
