# Tests Makefile
# Integrates all test targets for the JSON database project

CC = gcc
CFLAGS = -Wall -I../src/include -DTOOLS_BUILD -DUSE_QUICKJS -g -O0 -DDEBUG

# Libraries
LDFLAGS = -pthread -lm

# Quickjs check
ifneq ($(wildcard /opt/qjs/lib/quickjs/libquickjs.*),)
    CFLAGS += -DUSE_QUICKJS
    LDFLAGS += -L/opt/qjs/lib/quickjs -lquickjs
    $(info QuickJS found, enabling JavaScript tests)
    JS_ENABLED = 1
else
    $(info QuickJS not found, JavaScript tests will be skipped)
    JS_ENABLED = 0
endif

# Common source directories
SRC_DIR = ../src/components
OBJ_DIR = obj

# Create directory for object files
$(shell mkdir -p $(OBJ_DIR))
$(shell mkdir -p $(OBJ_DIR)/utils)
$(shell mkdir -p $(OBJ_DIR)/utils/memory)
$(shell mkdir -p $(OBJ_DIR)/database)
$(shell mkdir -p $(OBJ_DIR)/js)

# Common object files
COMMON_OBJS = $(OBJ_DIR)/utils/json.o \
              $(OBJ_DIR)/utils/json_helpers.o \
              $(OBJ_DIR)/utils/logger.o \
              $(OBJ_DIR)/utils/cache.o \
              $(OBJ_DIR)/utils/memory/ref_counter.o \
              $(OBJ_DIR)/utils/memory/ref_json.o

# Database test objects
DB_OBJS = $(COMMON_OBJS) \
          $(OBJ_DIR)/database/database.o \
          $(OBJ_DIR)/database/index.o \
          $(OBJ_DIR)/database/schema.o \
          $(OBJ_DIR)/database/lock_manager.o

# JavaScript test objects (conditionally included)
ifeq ($(JS_ENABLED), 1)
JS_OBJS = $(DB_OBJS) \
          $(OBJ_DIR)/utils/js_file_utils.o \
          $(OBJ_DIR)/js/js_engine.o \
          $(OBJ_DIR)/js/js_api.o
endif

# All test targets
ALL_TESTS = test_json_helpers_simple \
            test_refcount_simple

# Add JavaScript tests if JS is enabled
ifeq ($(JS_ENABLED), 1)
ALL_TESTS += test_js_integration
endif

# Default target - builds all tests
all: prepare $(ALL_TESTS)
	@echo "All tests built successfully!"

# Prepare directories needed for tests
prepare:
	@mkdir -p ../var/log/jsondb
	@mkdir -p ../var/data/jsondb
	@mkdir -p ../functions

# Simple JSON helpers test
test_json_helpers_simple: test_json_helpers_simple.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Simple reference counting test
test_refcount_simple: test_refcount_simple.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# JavaScript integration test
ifeq ($(JS_ENABLED), 1)
test_js_integration: test_js_integration.c $(JS_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
endif

# Object file compilation
$(OBJ_DIR)/utils/%.o: $(SRC_DIR)/utils/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/utils/memory/%.o: $(SRC_DIR)/utils/memory/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/database/%.o: $(SRC_DIR)/database/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/js/%.o: $(SRC_DIR)/js/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Run all tests
run: all
	@echo "=== Running JSON Helpers Test ==="
	./test_json_helpers_simple
	@echo ""
	@echo "=== Running RefCount Test ==="
	./test_refcount_simple
ifeq ($(JS_ENABLED), 1)
	@echo ""
	@echo "=== Running JavaScript Integration Test ==="
	./test_js_integration
endif

# Clean up
clean:
	rm -f $(ALL_TESTS) $(OBJ_DIR)/*.o $(OBJ_DIR)/utils/*.o $(OBJ_DIR)/utils/memory/*.o $(OBJ_DIR)/database/*.o $(OBJ_DIR)/js/*.o

.PHONY: all clean run prepare