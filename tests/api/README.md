# JDBX API Module Testing Framework

## Overview

This directory contains comprehensive unit and integration tests for the JDBX API modules that were extracted during Phase 2 (API Monolith Decomposition). The testing framework provides isolation testing for each module as well as integration testing across modules.

## Test Framework Features

### Core Capabilities
- **Modular Testing**: Each API module can be tested in isolation
- **Mock Infrastructure**: Mock HTTP requests, API contexts, and database operations
- **Assertion Library**: Comprehensive assertion macros for all test scenarios
- **Performance Testing**: Built-in performance measurement and benchmarking
- **JSON Validation**: Deep JSON comparison and validation utilities

### Test Organization

```
tests/api/
├── test_framework.h      # Testing framework header
├── test_framework.c      # Framework implementation
├── test_api_metrics.c    # Metrics module tests
├── test_api_documents.c  # Documents module tests
├── test_api_auth.c       # Authentication module tests
├── test_api_rbac.c       # RBAC module tests
├── test_api_integration.c # Cross-module integration tests
├── Makefile              # Build configuration
└── README.md             # This file
```

## Building Tests

### Build All Tests
```bash
make all
```

### Build Specific Module Test
```bash
make test_api_metrics
make test_api_documents
make test_api_auth
make test_api_rbac
```

## Running Tests

### Run All Tests
```bash
make test
```

### Run Specific Module Test
```bash
make run-api_metrics
make run-api_documents
```

### Run with Verbose Output
```bash
make verbose-api_metrics
./test_api_metrics -v
```

### Run Performance Tests Only
```bash
make perf
```

## Writing New Tests

### Basic Test Structure

```c
#include "test_framework.h"
#include "api/api_module.h"

static int test_feature_basic() {
    // Setup
    api_context_t* ctx = create_mock_api_context(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    
    http_request_t* request = create_mock_request(HTTP_GET, "/api/endpoint", NULL, NULL);
    TEST_ASSERT_NOT_NULL(request);
    
    // Execute
    http_response_t* response = api_handle_endpoint(ctx, request);
    
    // Verify
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQ(HTTP_OK, response->status_code);
    
    // Cleanup
    destroy_http_response(response);
    destroy_mock_request(request);
    destroy_mock_api_context(ctx);
    
    return 0; // Success
}

void run_module_tests() {
    RUN_TEST(test_feature_basic);
    RUN_TEST(test_feature_advanced);
}

int main(int argc, char* argv[]) {
    TEST_INIT();
    RUN_TEST_SUITE("Module Tests", run_module_tests);
    TEST_SUMMARY();
    return g_test_results.failed_tests > 0 ? 1 : 0;
}
```

### Available Assertions

```c
TEST_ASSERT(condition)                    // Basic assertion
TEST_ASSERT_MSG(condition, "message")     // Assertion with message
TEST_ASSERT_EQ(expected, actual)          // Equality check
TEST_ASSERT_STR_EQ(expected, actual)      // String equality
TEST_ASSERT_NULL(ptr)                     // NULL check
TEST_ASSERT_NOT_NULL(ptr)                 // Non-NULL check
```

### Mock Helpers

#### Create Mock HTTP Request
```c
http_request_t* request = create_mock_request(
    HTTP_POST,                          // Method
    "/api/documents",                   // Path
    "{\"title\":\"Test\"}",             // Body (optional)
    "Bearer mock-jwt-token"             // Auth header (optional)
);
```

#### Create Mock API Context
```c
// Unauthenticated context
api_context_t* ctx = create_mock_api_context(NULL);

// Authenticated context
mock_auth_context_t auth = {
    .username = "testuser",
    .token = "mock-jwt-token",
    .library = "default",
    .is_admin = 0
};
api_context_t* ctx = create_mock_api_context(&auth);
```

### Performance Testing

```c
static int perf_test_helper() {
    // Test code to benchmark
    return 0;
}

static int test_performance() {
    perf_test_t perf = {
        .name = "Operation name",
        .iterations = 1000,
        .target_ms = 100.0,  // Target: 0.1ms per operation
    };
    
    run_performance_test(&perf, perf_test_helper);
    return perf.passed ? 0 : 1;
}
```

## Test Coverage Goals

### Phase 3.1: Test Framework Foundation ✓
- [x] Test framework infrastructure
- [x] Mock utilities and helpers
- [x] Basic test structure
- [x] Build system integration

### Phase 3.2: API Module Unit Tests (In Progress)
- [x] api_metrics.c - Complete unit test coverage
- [ ] api_documents.c - Document CRUD operations
- [ ] api_auth.c - Authentication flows
- [ ] api_rbac.c - User and role management

### Phase 3.3: Integration Tests
- [ ] Cross-module interactions
- [ ] Authentication + RBAC integration
- [ ] Document + Metrics integration
- [ ] End-to-end workflows

### Phase 3.4: Performance Benchmarks
- [ ] Baseline performance metrics
- [ ] Module-specific benchmarks
- [ ] Memory usage profiling
- [ ] Concurrency testing

### Phase 3.5: Load Testing
- [ ] High-concurrency scenarios
- [ ] Stress test infrastructure
- [ ] Resource exhaustion testing
- [ ] Stability under load

## Best Practices

1. **Test Isolation**: Each test should be independent and not rely on other tests
2. **Resource Cleanup**: Always clean up allocated resources in tests
3. **Mock External Dependencies**: Use mocks for database, RBAC, and other external systems
4. **Meaningful Assertions**: Use descriptive assertion messages for debugging
5. **Performance Awareness**: Include performance tests for critical paths
6. **Error Cases**: Test both success and failure scenarios

## Continuous Integration

These tests are designed to be integrated into CI/CD pipelines:

```bash
# Run all tests and exit with appropriate code
make test || exit 1

# Generate test report (future enhancement)
make test > test_results.txt 2>&1
```

## Troubleshooting

### Common Issues

1. **Compilation Errors**: Ensure all include paths are correct in Makefile
2. **Linking Errors**: Check that all required source files are included
3. **Test Failures**: Run with `-v` flag for verbose output
4. **Memory Leaks**: Use valgrind for memory analysis

### Debug Mode

```bash
# Compile with debug symbols
make CFLAGS="-g -O0" test_api_metrics

# Run with debugger
gdb ./test_api_metrics
```

## Future Enhancements

1. **Code Coverage**: Integration with gcov/lcov
2. **Test Report Generation**: HTML/XML test reports
3. **Continuous Monitoring**: Performance regression detection
4. **Mocking Framework**: More sophisticated mocking capabilities
5. **Fuzzing Support**: Input fuzzing for security testing