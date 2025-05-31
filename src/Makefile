# JSONdb Main Makefile
# This Makefile coordinates the build process for the entire JSONdb project

# Directories
SRC_DIR = .
INCLUDE_DIR = include
OBJ_DIR = ../build/obj
BIN_DIR = ../build/bin
LIB_DIR = ../build/lib
TEST_DIR = ../tests

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -I$(INCLUDE_DIR) -I.
LDFLAGS = -lpthread -lm

# QuickJS support
QUICKJS_LIBS = -L/opt/qjs/lib/quickjs -lquickjs

# Source files (excluding tests)
CORE_SRCS = $(wildcard $(SRC_DIR)/components/core/*.c)
API_SRCS = $(wildcard $(SRC_DIR)/components/api/*.c)
DB_SRCS = $(wildcard $(SRC_DIR)/components/database/*.c)
BINARY_SRCS = $(wildcard $(SRC_DIR)/components/binary/*.c)
QUERY_SRCS = $(wildcard $(SRC_DIR)/components/query/*.c)
TRANS_SRCS = $(wildcard $(SRC_DIR)/components/transaction/*.c)
RBAC_SRCS = $(wildcard $(SRC_DIR)/components/rbac/*.c)
UTILS_SRCS = $(wildcard $(SRC_DIR)/components/utils/*.c)
TOOLS_SRCS = $(wildcard $(SRC_DIR)/components/tools/*.c)
JS_SRCS = $(filter-out $(SRC_DIR)/components/js/js_file_utils.c,$(wildcard $(SRC_DIR)/components/js/*.c))
MAIN_SRC = $(SRC_DIR)/components/main.c

# Initialization components
INIT_SRCS = $(wildcard $(SRC_DIR)/initialize/*.c)

# Thread pool implementation
# Note: Now integrated into server.c directly
THREAD_POOL_SRC = $(SRC_DIR)/components/core/thread_pool.c
THREAD_POOL_OBJ = $(OBJ_DIR)/components/core/thread_pool.o

# Object files
CORE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
API_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(API_SRCS))
DB_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(DB_SRCS))
BINARY_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(BINARY_SRCS))
QUERY_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(QUERY_SRCS))
TRANS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(TRANS_SRCS))
RBAC_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(RBAC_SRCS))
UTILS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(UTILS_SRCS))
TOOLS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(TOOLS_SRCS))
JS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(JS_SRCS))

# Initialization objects
INIT_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(INIT_SRCS))

# All objects
OBJS = $(CORE_OBJS) ../build/obj/components/core/authentication_handler.o $(API_OBJS) $(DB_OBJS) $(BINARY_OBJS) $(QUERY_OBJS) $(TRANS_OBJS) \
       $(RBAC_OBJS) $(UTILS_OBJS) $(TOOLS_OBJS) $(JS_OBJS) $(INIT_OBJS)

# JavaScript support handling
ifdef DISABLE_JS
    # JavaScript is explicitly disabled with -DDISABLE_JS
    SERVER_LDFLAGS = $(LDFLAGS)
else
    # JavaScript is enabled by default
    SERVER_LDFLAGS = $(LDFLAGS) $(QUICKJS_LIBS) -DUSE_QUICKJS
    CFLAGS += -DUSE_QUICKJS
endif

# Default target
all: directories libjsondb server tools

# Create build directories
directories:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/components/core
	@mkdir -p $(OBJ_DIR)/components/api
	@mkdir -p $(OBJ_DIR)/components/database
	@mkdir -p $(OBJ_DIR)/components/binary
	@mkdir -p $(OBJ_DIR)/components/query
	@mkdir -p $(OBJ_DIR)/components/transaction
	@mkdir -p $(OBJ_DIR)/components/rbac
	@mkdir -p $(OBJ_DIR)/components/utils
	@mkdir -p $(OBJ_DIR)/components/tools
	@mkdir -p $(OBJ_DIR)/components/js
	@mkdir -p $(OBJ_DIR)/initialize
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(LIB_DIR)

# Define custom database objects using only the implementations we want
# Filter out both old database implementations and use only binary format
CUSTOM_DB_SRCS = $(filter-out $(SRC_DIR)/components/database/database_operations.c, $(filter-out $(SRC_DIR)/components/database/database.c, $(filter-out $(SRC_DIR)/components/database/database_with_optimizations.c, $(DB_SRCS))))
CUSTOM_DB_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(CUSTOM_DB_SRCS))

# Use only the binary format implementation
FINAL_DB_OBJS = $(CUSTOM_DB_OBJS)

# Performance optimization components (Phase 2 & 3)
OPTIMIZATION_SRCS = $(SRC_DIR)/components/core/epoll_server.c $(SRC_DIR)/components/core/server_selector.c
OPTIMIZATION_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(OPTIMIZATION_SRCS))

# Define all optimized objects
OPTIMIZED_OBJS = $(CORE_OBJS) ../build/obj/components/core/authentication_handler.o $(API_OBJS) $(FINAL_DB_OBJS) $(BINARY_OBJS) $(QUERY_OBJS) $(TRANS_OBJS) \
       $(RBAC_OBJS) $(UTILS_OBJS) $(TOOLS_OBJS) $(JS_OBJS) $(THREAD_POOL_OBJ) $(OPTIMIZATION_OBJS) $(INIT_OBJS)

# List of objects for Makefile dependencies
ALL_OBJS = $(filter-out $(OBJ_DIR)/components/database/database.o $(OBJ_DIR)/components/database/database_operations.o, $(OBJS)) \
           $(THREAD_POOL_OBJ)

# Build static library
libjsondb: $(ALL_OBJS)
	@echo "Creating libjsondb.a with optimized implementations..."
	ar rcs $(LIB_DIR)/libjsondb.a $(OPTIMIZED_OBJS)

# Build server with new main.c
server: $(OBJ_DIR)/components/main.o libjsondb
	$(CC) $(OBJ_DIR)/components/main.o -o $(BIN_DIR)/jsondb_server -L$(LIB_DIR) -ljsondb $(SERVER_LDFLAGS) -lm

# Target to build with new modular main.c
server-modular: libjsondb
	$(CC) $(CFLAGS) -c $(SRC_DIR)/components/main.c.modular -o $(OBJ_DIR)/components/main.o
	$(CC) $(OBJ_DIR)/components/main.o -o $(BIN_DIR)/jsondb_server -L$(LIB_DIR) -ljsondb $(SERVER_LDFLAGS) -lm

# Build tools
tools: libjsondb $(OBJ_DIR)/components/tools/jsondb_tools.o
	$(CC) $(OBJ_DIR)/components/tools/jsondb_tools.o -o $(BIN_DIR)/jsondb_tools -L$(LIB_DIR) -ljsondb $(SERVER_LDFLAGS)

# Compile rule for .c files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/* $(LIB_DIR)/*

# Run tests
test:
	cd $(TEST_DIR) && $(MAKE) -f Makefile.unified

# Organization verification target
verify-organization:
	@echo "Verifying project organization..."
	@# Check for backup files
	@if find . -name "*.bak" -o -name "*.backup" -o -name "*.orig" | grep -q .; then \
		echo "ERROR: Backup files found in source tree"; \
		find . -name "*.bak" -o -name "*.backup" -o -name "*.orig"; \
		exit 1; \
	fi
	@# Check for implementation files in wrong locations
	@if find . -type f -name "*.c" | grep -v "./components/" | grep -q .; then \
		echo "ERROR: Implementation files found outside components directory"; \
		find . -type f -name "*.c" | grep -v "./components/"; \
		exit 1; \
	fi
	@# Check for header files in wrong locations
	@if find ./components -name "*.h" | grep -q .; then \
		echo "ERROR: Header files found in components directory (should be in include/)"; \
		find ./components -name "*.h"; \
		exit 1; \
	fi
	@# Check for duplicate main.c
	@if [ $$(find . -name "main.c" | wc -l) -gt 1 ]; then \
		echo "ERROR: Multiple main.c files found"; \
		find . -name "main.c"; \
		exit 1; \
	fi
	@# Ensure components and include directories match (with exceptions)
	@for dir in $(shell find ./include -mindepth 1 -maxdepth 1 -type d | sed 's|./include/||'); do \
		if [ ! -d "./components/$$dir" ] && [ "$$dir" != "quickjs" ]; then \
			echo "ERROR: Component directory missing: components/$$dir"; \
			exit 1; \
		fi; \
	done
	@for dir in $(shell find ./components -mindepth 1 -maxdepth 1 -type d | sed 's|./components/||'); do \
		if [ ! -d "./include/$$dir" ]; then \
			echo "ERROR: Include directory missing: include/$$dir"; \
			exit 1; \
		fi; \
	done
	@# Check for redundant nested utils directories 
	@if [ -d "./components/js/utils" ]; then \
		echo "ERROR: js/utils directory should not exist - move files to js/"; \
		exit 1; \
	fi
	@if [ -d "./include/utils/memory" ]; then \
		echo "ERROR: utils/memory directory should not exist - move files to utils/"; \
		exit 1; \
	fi
	@echo "Organization verification passed"

# Phony targets
.PHONY: all directories libjsondb server server-modular tools clean test verify-organization

# Special targets for building with different flags
js-disabled:
	$(MAKE) DISABLE_JS=1

js-enabled:
	$(MAKE) USE_QUICKJS=1

metrics:
	$(MAKE) CFLAGS="$(CFLAGS) -DENABLE_METRICS"

debug:
	$(MAKE) CFLAGS="$(CFLAGS) -g -DDEBUG"

release:
	$(MAKE) CFLAGS="$(CFLAGS) -O3 -DNDEBUG"

# Include dependencies
-include $(OBJS:.o=.d)