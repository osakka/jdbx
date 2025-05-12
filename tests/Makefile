# JSONdb Tests Makefile
# This Makefile coordinates the testing for JSONdb

# Directories
TEST_SRC_DIR = .
UNIT_DIR = unit
INTEGRATION_DIR = integration
PERF_DIR = performance
JS_DIR = js
ROOT_DIR = ..
OBJ_DIR = $(ROOT_DIR)/build/obj/tests
BIN_DIR = $(ROOT_DIR)/build/bin/tests
LIB_DIR = $(ROOT_DIR)/build/lib
INCLUDE_DIR = $(ROOT_DIR)/include

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -I$(INCLUDE_DIR) -DTOOLS_BUILD -g -O0 -DDEBUG
LDFLAGS = -lm -lpthread

# QuickJS support
QUICKJS_FLAGS = -DUSE_QUICKJS
QUICKJS_LIBS = -lquickjs

# Library
JSONDB_LIB = $(LIB_DIR)/libjsondb.a

# Check for QuickJS
ifneq ($(wildcard /opt/qjs/lib/quickjs/libquickjs.*),)
    CFLAGS += $(QUICKJS_FLAGS)
    LDFLAGS += -L/opt/qjs/lib/quickjs $(QUICKJS_LIBS)
    $(info QuickJS found, enabling JavaScript tests)
    JS_ENABLED = 1
else
    $(info QuickJS not found, JavaScript tests will be skipped)
    JS_ENABLED = 0
endif

# Unit test sources and objects
UNIT_TEST_SRCS = $(wildcard $(UNIT_DIR)/*.c)
UNIT_TEST_OBJS = $(patsubst $(UNIT_DIR)/%.c,$(OBJ_DIR)/unit/%.o,$(UNIT_TEST_SRCS))
UNIT_TEST_BINS = $(patsubst $(UNIT_DIR)/%.c,$(BIN_DIR)/unit/%,$(UNIT_TEST_SRCS))

# Integration test sources and objects
INTEG_TEST_SRCS = $(wildcard $(INTEGRATION_DIR)/*.c)
INTEG_TEST_OBJS = $(patsubst $(INTEGRATION_DIR)/%.c,$(OBJ_DIR)/integration/%.o,$(INTEG_TEST_SRCS))
INTEG_TEST_BINS = $(patsubst $(INTEGRATION_DIR)/%.c,$(BIN_DIR)/integration/%,$(INTEG_TEST_SRCS))

# All tests
ALL_TEST_BINS = $(UNIT_TEST_BINS) $(INTEG_TEST_BINS)

# JavaScript tests
JS_TEST_FILES = $(wildcard $(JS_DIR)/*.js)

# Main targets
all: dirs $(ALL_TEST_BINS)

# Create build directories
dirs:
	@mkdir -p $(OBJ_DIR)/unit $(OBJ_DIR)/integration $(BIN_DIR)/unit $(BIN_DIR)/integration
	@mkdir -p ../var/log/jsondb
	@mkdir -p ../var/data/jsondb

# Unit tests
unit-tests: dirs $(UNIT_TEST_BINS)
	@echo "Running unit tests..."
	@for test in $(UNIT_TEST_BINS); do \
		echo "Running $$test"; \
		$$test; \
	done

# Integration tests
integration-tests: dirs $(INTEG_TEST_BINS)
	@echo "Running integration tests..."
	@for test in $(INTEG_TEST_BINS); do \
		echo "Running $$test"; \
		$$test; \
	done

# JavaScript tests
js-tests:
ifeq ($(JS_ENABLED), 1)
	@echo "Running JavaScript tests..."
	@for test in $(JS_TEST_FILES); do \
		echo "Running $$test"; \
		$(BIN_DIR)/js_test $$test; \
	done
else
	@echo "JavaScript support not enabled, skipping JS tests"
endif

# Performance tests
performance:
	@echo "Running performance tests..."
	@cd $(PERF_DIR) && ./run_performance_tests.sh

# Clean test files
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Special compatibility targets
test_json_helpers_simple: dirs
	@mkdir -p $(UNIT_DIR)
	cp test_json_helpers_simple.c $(UNIT_DIR)/
	make unit-tests

test_refcount_simple: dirs
	@mkdir -p $(UNIT_DIR)
	cp test_refcount_simple.c $(UNIT_DIR)/
	make unit-tests

test_js_integration: dirs
ifeq ($(JS_ENABLED), 1)
	@mkdir -p $(INTEGRATION_DIR)
	cp test_js_integration.c $(INTEGRATION_DIR)/
	make integration-tests
endif

# Pattern rules for building tests
$(OBJ_DIR)/unit/%.o: $(UNIT_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/unit/%: $(OBJ_DIR)/unit/%.o $(JSONDB_LIB)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $< $(JSONDB_LIB) $(LDFLAGS)

$(OBJ_DIR)/integration/%.o: $(INTEGRATION_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/integration/%: $(OBJ_DIR)/integration/%.o $(JSONDB_LIB)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $< $(JSONDB_LIB) $(LDFLAGS)

# Run all tests
run: all unit-tests integration-tests
ifeq ($(JS_ENABLED), 1)
	make js-tests
endif
	make performance

.PHONY: all dirs unit-tests integration-tests js-tests performance clean run