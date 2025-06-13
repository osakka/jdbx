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
CFLAGS = -g -O0 -Wall -Wextra -I$(INCLUDE_DIR) -I.
LDFLAGS = -lpthread -lm -lssl -lcrypto

# QuickJS support
QUICKJS_LIBS = -L/opt/qjs/lib/quickjs -lquickjs

# Source files (excluding tests)
CORE_SRCS = $(wildcard $(SRC_DIR)/components/core/*.c)
# Include new thread-safe connection management
THREAD_SAFE_SRCS = $(SRC_DIR)/components/core/client_connection.c $(SRC_DIR)/components/core/server_thread_safe.c
API_SRCS = $(wildcard $(SRC_DIR)/components/api/*.c)
# Database sources - all files now updated for JDBX
DB_SRCS = $(wildcard $(SRC_DIR)/components/database/*.c)
# Exclude binary format files - JDBX replaces this functionality
BINARY_SRCS = 
QUERY_SRCS = $(wildcard $(SRC_DIR)/components/query/*.c)
TRANS_SRCS = $(wildcard $(SRC_DIR)/components/transaction/*.c)
RBAC_SRCS = $(wildcard $(SRC_DIR)/components/rbac/*.c)
UTILS_SRCS = $(wildcard $(SRC_DIR)/components/utils/*.c)
TOOLS_SRCS = $(wildcard $(SRC_DIR)/components/tools/*.c)
JS_SRCS = $(filter-out $(SRC_DIR)/components/js/js_file_utils.c,$(wildcard $(SRC_DIR)/components/js/*.c))
MAIN_SRC = $(SRC_DIR)/components/main.c

# Performance optimization sources - JDBX ONLY!
# Clean source lists - redundant files have been moved to trash
STORAGE_SRCS = $(wildcard $(SRC_DIR)/components/storage/*.c)
INDEX_SRCS = $(wildcard $(SRC_DIR)/components/index/*.c)
LOCKFREE_SRCS = $(wildcard $(SRC_DIR)/components/lockfree/*.c)

# Initialization components (use JDBX-only database)
INIT_SRCS = $(wildcard $(SRC_DIR)/initialize/*.c)

# Thread pool implementation
# Note: Now integrated into server.c directly
THREAD_POOL_SRC = $(SRC_DIR)/components/core/thread_pool.c
THREAD_POOL_OBJ = $(OBJ_DIR)/components/core/thread_pool.o

# Object files
CORE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
THREAD_SAFE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(THREAD_SAFE_SRCS))
API_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(API_SRCS))
DB_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(DB_SRCS))
BINARY_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(BINARY_SRCS))
QUERY_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(QUERY_SRCS))
TRANS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(TRANS_SRCS))
RBAC_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(RBAC_SRCS))
UTILS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(UTILS_SRCS))
TOOLS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(TOOLS_SRCS))
JS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(JS_SRCS))
STORAGE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(STORAGE_SRCS))
INDEX_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(INDEX_SRCS))
LOCKFREE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(LOCKFREE_SRCS))

# Initialization objects
INIT_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(INIT_SRCS))

# All objects
OBJS = $(CORE_OBJS) $(THREAD_SAFE_OBJS) ../build/obj/components/core/authentication_handler.o $(API_OBJS) $(DB_OBJS) $(BINARY_OBJS) $(QUERY_OBJS) $(TRANS_OBJS) \
       $(RBAC_OBJS) $(UTILS_OBJS) $(TOOLS_OBJS) $(JS_OBJS) $(INIT_OBJS) $(STORAGE_OBJS) $(INDEX_OBJS) $(LOCKFREE_OBJS)

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
all: directories libjdbx server tools

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
	@mkdir -p $(OBJ_DIR)/components/storage
	@mkdir -p $(OBJ_DIR)/components/index
	@mkdir -p $(OBJ_DIR)/components/lockfree
	@mkdir -p $(OBJ_DIR)/initialize
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(LIB_DIR)

# Database objects - clean source tree with single JDBX implementation
DB_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(DB_SRCS))

# Performance optimization components (Phase 2 & 3)
OPTIMIZATION_SRCS = $(SRC_DIR)/components/core/epoll_server.c $(SRC_DIR)/components/core/server_selector.c
OPTIMIZATION_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(OPTIMIZATION_SRCS))

# Define all optimized objects
OPTIMIZED_OBJS = $(CORE_OBJS) ../build/obj/components/core/authentication_handler.o $(API_OBJS) $(DB_OBJS) $(BINARY_OBJS) $(QUERY_OBJS) $(TRANS_OBJS) \
       $(RBAC_OBJS) $(UTILS_OBJS) $(TOOLS_OBJS) $(JS_OBJS) $(THREAD_POOL_OBJ) $(OPTIMIZATION_OBJS) $(INIT_OBJS) \
       $(STORAGE_OBJS) $(INDEX_OBJS) $(LOCKFREE_OBJS)

# List of objects for Makefile dependencies
ALL_OBJS = $(OBJS) $(THREAD_POOL_OBJ)

# Build static library
libjdbx: $(OPTIMIZED_OBJS)
	@echo "Creating libjdbx.a with optimized implementations..."
	ar rcs $(LIB_DIR)/libjdbx.a $(OPTIMIZED_OBJS)

# Build server with new main.c
server: $(OBJ_DIR)/components/main.o libjdbx
	$(CC) $(OBJ_DIR)/components/main.o -o $(BIN_DIR)/jdbxd -L$(LIB_DIR) -ljdbx $(SERVER_LDFLAGS) -lm

# Target to build with new modular main.c
server-modular: libjdbx
	$(CC) $(CFLAGS) -c $(SRC_DIR)/components/main.c.modular -o $(OBJ_DIR)/components/main.o
	$(CC) $(OBJ_DIR)/components/main.o -o $(BIN_DIR)/jdbxd -L$(LIB_DIR) -ljdbx $(SERVER_LDFLAGS) -lm

# Build tools
tools: libjdbx $(OBJ_DIR)/components/tools/jdbx_tools.o
	$(CC) $(OBJ_DIR)/components/tools/jdbx_tools.o -o $(BIN_DIR)/jdbx-tools -L$(LIB_DIR) -ljdbx $(SERVER_LDFLAGS)

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
	@if find . -type f -name "*.c" | grep -v "./components/" | grep -v "./initialize/" | grep -q .; then \
		echo "ERROR: Implementation files found outside components or initialize directories"; \
		find . -type f -name "*.c" | grep -v "./components/" | grep -v "./initialize/"; \
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