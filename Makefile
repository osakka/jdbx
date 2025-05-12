# JSONdb Main Makefile
# This Makefile coordinates the build process for the entire JSONdb project

# Directories
SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = build/obj
BIN_DIR = build/bin
LIB_DIR = build/lib
TEST_DIR = tests

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -I$(INCLUDE_DIR)
LDFLAGS = -lm -lpthread

# QuickJS support
QUICKJS_FLAGS = -DUSE_QUICKJS
QUICKJS_LIBS = -lquickjs

# Source files (excluding tests)
CORE_SRCS = $(wildcard $(SRC_DIR)/core/*.c)
API_SRCS = $(wildcard $(SRC_DIR)/api/*.c)
DB_SRCS = $(wildcard $(SRC_DIR)/database/*.c)
QUERY_SRCS = $(wildcard $(SRC_DIR)/query/*.c)
TRANS_SRCS = $(wildcard $(SRC_DIR)/transaction/*.c)
RBAC_SRCS = $(wildcard $(SRC_DIR)/rbac/*.c)
UTILS_SRCS = $(wildcard $(SRC_DIR)/utils/*.c) $(wildcard $(SRC_DIR)/utils/memory/*.c)
TOOLS_SRCS = $(wildcard $(SRC_DIR)/tools/*.c)
JS_SRCS = $(wildcard $(SRC_DIR)/js/*.c) $(wildcard $(SRC_DIR)/js/utils/*.c)

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
       $(RBAC_OBJS) $(UTILS_OBJS) $(TOOLS_OBJS)

# JavaScript support (default enabled, can be disabled)
ifndef DISABLE_QUICKJS
    CFLAGS += $(QUICKJS_FLAGS)
    LDFLAGS += $(QUICKJS_LIBS)
    OBJS += $(JS_OBJS)
endif

# Main targets
ifdef DISABLE_QUICKJS
all: dirs libjsondb server-without-js tests
else
all: dirs libjsondb server tests
endif

# Create build directories
dirs:
	@mkdir -p $(OBJ_DIR)/core $(OBJ_DIR)/api $(OBJ_DIR)/database \
	         $(OBJ_DIR)/query $(OBJ_DIR)/transaction $(OBJ_DIR)/rbac \
	         $(OBJ_DIR)/utils $(OBJ_DIR)/utils/memory $(OBJ_DIR)/tools \
	         $(OBJ_DIR)/js $(OBJ_DIR)/js/utils \
	         $(BIN_DIR) $(LIB_DIR)

# Static library
libjsondb: dirs $(OBJS)
	ar rcs $(LIB_DIR)/libjsondb.a $(OBJS)

# Server executable
server: dirs libjsondb
	$(CC) $(CFLAGS) -o $(BIN_DIR)/jsondb_server $(SRC_DIR)/core/main.c $(LIB_DIR)/libjsondb.a $(LDFLAGS)

# Server executable without JS support
server-without-js: dirs libjsondb
	$(CC) $(CFLAGS) -DDISABLE_JS -o $(BIN_DIR)/jsondb_server $(SRC_DIR)/core/main.c $(LIB_DIR)/libjsondb.a $(LDFLAGS)

# Tests
tests: dirs libjsondb
	@$(MAKE) -C $(TEST_DIR)

# Clean build files
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LIB_DIR)
	@$(MAKE) -C $(TEST_DIR) clean

# Install
install: all
	@echo "Installing JSONdb..."
	@mkdir -p $(DESTDIR)/usr/local/bin
	@mkdir -p $(DESTDIR)/usr/local/lib
	@mkdir -p $(DESTDIR)/usr/local/include/jsondb
	@cp $(BIN_DIR)/jsondb_server $(DESTDIR)/usr/local/bin/
	@cp $(LIB_DIR)/libjsondb.a $(DESTDIR)/usr/local/lib/
	@cp -r $(INCLUDE_DIR)/* $(DESTDIR)/usr/local/include/

# Uninstall
uninstall:
	@echo "Uninstalling JSONdb..."
	@rm -f $(DESTDIR)/usr/local/bin/jsondb_server
	@rm -f $(DESTDIR)/usr/local/lib/libjsondb.a
	@rm -rf $(DESTDIR)/usr/local/include/jsondb
	@rm -f $(DESTDIR)/usr/local/include/jsondb.h

# JavaScript tests
js-tests: dirs libjsondb
	@$(MAKE) -C $(TEST_DIR) js-tests

# Performance tests
performance: dirs libjsondb
	@$(MAKE) -C $(TEST_DIR) performance

# Object file compilation pattern
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all dirs libjsondb server tests clean install uninstall js-tests performance