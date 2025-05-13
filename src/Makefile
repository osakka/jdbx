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
CFLAGS = -Wall -Wextra -I$(INCLUDE_DIR) -I$(SRC_DIR)
LDFLAGS = -lm -lpthread

# QuickJS support
QUICKJS_LIBS = -L/opt/qjs/lib/quickjs -lquickjs

# Source files (excluding tests)
CORE_SRCS = $(wildcard $(SRC_DIR)/components/core/*.c)
API_SRCS = $(wildcard $(SRC_DIR)/components/api/*.c)
DB_SRCS = $(wildcard $(SRC_DIR)/components/database/*.c)
QUERY_SRCS = $(wildcard $(SRC_DIR)/components/query/*.c)
TRANS_SRCS = $(wildcard $(SRC_DIR)/components/transaction/*.c)
RBAC_SRCS = $(wildcard $(SRC_DIR)/components/rbac/*.c)
UTILS_SRCS = $(wildcard $(SRC_DIR)/components/utils/*.c) $(wildcard $(SRC_DIR)/components/utils/memory/*.c)
TOOLS_SRCS = $(wildcard $(SRC_DIR)/components/tools/*.c)
JS_SRCS = $(wildcard $(SRC_DIR)/components/js/*.c) $(wildcard $(SRC_DIR)/components/js/utils/*.c)
MAIN_SRC = $(SRC_DIR)/components/main.c

# Object files
CORE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
API_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(API_SRCS))
DB_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(DB_SRCS))
QUERY_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(QUERY_SRCS))
TRANS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(TRANS_SRCS))
RBAC_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(RBAC_SRCS))
UTILS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(UTILS_SRCS))
TOOLS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(TOOLS_SRCS))
JS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(JS_SRCS))

# All objects
OBJS = $(CORE_OBJS) $(API_OBJS) $(DB_OBJS) $(QUERY_OBJS) $(TRANS_OBJS) \
       $(RBAC_OBJS) $(UTILS_OBJS) $(TOOLS_OBJS) $(JS_OBJS)

# JavaScript support handling
ifdef DISABLE_JS
    # JavaScript is explicitly disabled with -DDISABLE_JS
    SERVER_LDFLAGS = $(LDFLAGS)
else
    # JavaScript is enabled by default
    SERVER_LDFLAGS = $(LDFLAGS) $(QUICKJS_LIBS) -DENABLE_JS
    CFLAGS += -DENABLE_JS
endif

# Default target
all: directories libjsondb server tools

# Create build directories
directories:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/components/core
	@mkdir -p $(OBJ_DIR)/components/api
	@mkdir -p $(OBJ_DIR)/components/database
	@mkdir -p $(OBJ_DIR)/components/query
	@mkdir -p $(OBJ_DIR)/components/transaction
	@mkdir -p $(OBJ_DIR)/components/rbac
	@mkdir -p $(OBJ_DIR)/components/utils/memory
	@mkdir -p $(OBJ_DIR)/components/tools
	@mkdir -p $(OBJ_DIR)/components/js/utils
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(LIB_DIR)

# Build static library
libjsondb: $(OBJS)
	ar rcs $(LIB_DIR)/libjsondb.a $(OBJS)

# Build server
server: $(OBJ_DIR)/components/main.o libjsondb
	$(CC) $(OBJ_DIR)/components/main.o -o $(BIN_DIR)/jsondb_server $(SERVER_LDFLAGS) -L$(LIB_DIR) -ljsondb

# Build tools
tools: libjsondb $(OBJ_DIR)/components/tools/jsondb_tools.o
	$(CC) $(OBJ_DIR)/components/tools/jsondb_tools.o -o $(BIN_DIR)/jsondb_tools $(LDFLAGS) -L$(LIB_DIR) -ljsondb

# Compile rule for .c files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/* $(LIB_DIR)/*

# Run tests
test:
	cd $(TEST_DIR) && $(MAKE) -f Makefile.unified

# Phony targets
.PHONY: all directories libjsondb server tools clean test

# Special targets for building with different flags
js-disabled:
	$(MAKE) DISABLE_JS=1

js-enabled:
	$(MAKE) ENABLE_JS=1

metrics:
	$(MAKE) CFLAGS="$(CFLAGS) -DENABLE_METRICS"

debug:
	$(MAKE) CFLAGS="$(CFLAGS) -g -DDEBUG"

release:
	$(MAKE) CFLAGS="$(CFLAGS) -O3 -DNDEBUG"

# Include dependencies
-include $(OBJS:.o=.d)