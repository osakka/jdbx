# JDBX API Module Tests Makefile

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -g -I../../src/include -I. -DTOOLS_BUILD
LDFLAGS = -pthread -lm

# Source directories
SRC_DIR = ../../src
API_DIR = $(SRC_DIR)/components/api
UTILS_DIR = $(SRC_DIR)/components/utils
CORE_DIR = $(SRC_DIR)/components/core

# Test framework
TEST_FRAMEWORK = test_framework.c

# Common dependencies
COMMON_DEPS = \
    $(UTILS_DIR)/buffer_pool.c \
    $(UTILS_DIR)/json.c \
    $(UTILS_DIR)/json_deep_copy.c \
    $(UTILS_DIR)/memory_manager.c \
    $(UTILS_DIR)/metrics.c \
    $(UTILS_DIR)/logger.c \
    $(CORE_DIR)/http_response.c

# Test targets
TEST_TARGETS = \
    test_api_metrics \
    test_api_documents \
    test_api_auth \
    test_api_rbac \
    test_api_integration \
    test_performance_benchmarks \
    test_load_testing

# Default target
all: $(TEST_TARGETS)

# Test framework object
test_framework.o: test_framework.c test_framework.h
	$(CC) $(CFLAGS) -c test_framework.c -o test_framework.o

# API Metrics tests
test_api_metrics: test_api_metrics.c test_framework.o $(API_DIR)/api_metrics.c $(COMMON_DEPS)
	$(CC) $(CFLAGS) -o $@ test_api_metrics.c test_framework.o \
		$(API_DIR)/api_metrics.c \
		$(COMMON_DEPS) \
		$(LDFLAGS)

# API Documents tests (placeholder)
test_api_documents: test_api_documents.c test_framework.o $(API_DIR)/api_documents.c $(COMMON_DEPS)
	$(CC) $(CFLAGS) -o $@ test_api_documents.c test_framework.o \
		$(API_DIR)/api_documents.c \
		$(COMMON_DEPS) \
		$(LDFLAGS)

# API Auth tests (placeholder)
test_api_auth: test_api_auth.c test_framework.o $(API_DIR)/api_auth.c $(COMMON_DEPS)
	$(CC) $(CFLAGS) -o $@ test_api_auth.c test_framework.o \
		$(API_DIR)/api_auth.c \
		$(COMMON_DEPS) \
		$(LDFLAGS)

# API RBAC tests (placeholder)
test_api_rbac: test_api_rbac.c test_framework.o $(API_DIR)/api_rbac.c $(COMMON_DEPS)
	$(CC) $(CFLAGS) -o $@ test_api_rbac.c test_framework.o \
		$(API_DIR)/api_rbac.c \
		$(COMMON_DEPS) \
		$(LDFLAGS)

# Integration tests
test_api_integration: test_api_integration.c test_framework.o $(COMMON_DEPS)
	$(CC) $(CFLAGS) -o $@ test_api_integration.c test_framework.o \
		$(COMMON_DEPS) \
		$(LDFLAGS)

# Performance benchmarks
test_performance_benchmarks: test_performance_benchmarks.c test_framework.o $(COMMON_DEPS)
	$(CC) $(CFLAGS) -o $@ test_performance_benchmarks.c test_framework.o \
		$(COMMON_DEPS) \
		$(LDFLAGS)

# Load testing
test_load_testing: test_load_testing.c test_framework.o $(COMMON_DEPS)
	$(CC) $(CFLAGS) -o $@ test_load_testing.c test_framework.o \
		$(COMMON_DEPS) \
		$(LDFLAGS)

# Run all tests
test: $(TEST_TARGETS)
	@echo "Running API Module Tests..."
	@echo "========================="
	@for test in $(TEST_TARGETS); do \
		if [ -f $$test ]; then \
			echo "\nRunning $$test..."; \
			./$$test || exit 1; \
		fi; \
	done
	@echo "\nAll tests completed!"

# Run specific test
run-%: test_%
	./test_$*

# Run with verbose output
verbose-%: test_%
	./test_$* -v

# Performance tests only
perf: $(TEST_TARGETS)
	@echo "Running Performance Tests..."
	@for test in $(TEST_TARGETS); do \
		if [ -f $$test ]; then \
			./$$test -p 2>/dev/null || true; \
		fi; \
	done

# Clean build artifacts
clean:
	rm -f $(TEST_TARGETS) *.o

# Help
help:
	@echo "JDBX API Module Tests"
	@echo "===================="
	@echo "Targets:"
	@echo "  all              - Build all tests"
	@echo "  test             - Run all tests"
	@echo "  run-<module>     - Run specific module test"
	@echo "  verbose-<module> - Run with verbose output"
	@echo "  perf             - Run performance tests only"
	@echo "  clean            - Remove build artifacts"
	@echo ""
	@echo "Examples:"
	@echo "  make test_api_metrics"
	@echo "  make run-api_metrics"
	@echo "  make verbose-api_metrics"

.PHONY: all test clean help perf