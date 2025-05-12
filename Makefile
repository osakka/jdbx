# JSON Database Server Makefile
# For new reorganized source code structure

CC = gcc
CFLAGS = -Wall -I./src/include -DTOOLS_BUILD

# Project directories
# Relative to position.
SRC_DIR = src/components
INCLUDE_DIR = src/include
OBJ_DIR = obj
BIN_DIR = bin
VAR_DIR = var
RUN_DIR = $(VAR_DIR)/run
LOG_DIR = $(VAR_DIR)/log
DATA_DIR = $(VAR_DIR)/data
ETC_DIR = etc

# Build modes
# make - Regular debug build
# make release - Optimized release build
# make asan - Build with Address Sanitizer
# make memcheck - Build for Valgrind memory checking

# Default to debug build
ifeq ($(MAKECMDGOALS),release)
    CFLAGS += -O2 -DNDEBUG
    $(info Building in release mode with optimizations)
else ifeq ($(MAKECMDGOALS),asan)
    CFLAGS += -g -fsanitize=address -fno-omit-frame-pointer
    $(info Building with Address Sanitizer)
else ifeq ($(MAKECMDGOALS),memcheck)
    CFLAGS += -g -O0
    $(info Building for Valgrind memory checking)
else
    CFLAGS += -g -O0 -DDEBUG
    $(info Building in debug mode)
endif

# Check if QuickJS is available
ifneq ($(wildcard /opt/qjs/lib/quickjs/libquickjs.*),)
    CFLAGS += -DUSE_QUICKJS
    LDFLAGS = -pthread -lm -L/opt/qjs/lib/quickjs -lquickjs
    $(info QuickJS found, enabling JavaScript support)
else
    LDFLAGS = -pthread -lm
    $(info QuickJS not found, JavaScript support will be disabled)
endif

# Add Address Sanitizer to linker flags if requested
ifeq ($(MAKECMDGOALS),asan)
    LDFLAGS += -fsanitize=address
endif

# Create directory structure for object files
$(shell mkdir -p $(OBJ_DIR)/api)
$(shell mkdir -p $(OBJ_DIR)/core)
$(shell mkdir -p $(OBJ_DIR)/database)
$(shell mkdir -p $(OBJ_DIR)/js)
$(shell mkdir -p $(OBJ_DIR)/rbac)
$(shell mkdir -p $(OBJ_DIR)/query)
$(shell mkdir -p $(OBJ_DIR)/transaction)
$(shell mkdir -p $(OBJ_DIR)/tools)
$(shell mkdir -p $(OBJ_DIR)/utils/memory)

# Create runtime directories
$(shell mkdir -p $(BIN_DIR))
$(shell mkdir -p $(RUN_DIR))
$(shell mkdir -p $(LOG_DIR))
$(shell mkdir -p $(DATA_DIR))

# Explicit file lists rather than wildcards to avoid duplicates
# API sources
API_FILES = admin_api.c backup_api.c cache_api.c index_api.c index_query_api.c \
           schema_api.c system_api.c transaction_api.c transaction_log_api.c \
           transaction_status_api.c visualization_api.c
API_SRC = $(addprefix $(SRC_DIR)/api/, $(API_FILES))
API_OBJ = $(patsubst $(SRC_DIR)/api/%.c, $(OBJ_DIR)/api/%.o, $(API_SRC))

# Core sources
CORE_FILES = main.c server.c ssl.c cors.c api.c
CORE_SRC = $(addprefix $(SRC_DIR)/core/, $(CORE_FILES))
CORE_OBJ = $(patsubst $(SRC_DIR)/core/%.c, $(OBJ_DIR)/core/%.o, $(CORE_SRC))

# Database sources
DATABASE_FILES = database.c index.c lock_manager.c schema.c
DATABASE_SRC = $(addprefix $(SRC_DIR)/database/, $(DATABASE_FILES))
DATABASE_OBJ = $(patsubst $(SRC_DIR)/database/%.c, $(OBJ_DIR)/database/%.o, $(DATABASE_SRC))

# JavaScript sources
JS_FILES = js_api.c js_engine.c
JS_SRC = $(addprefix $(SRC_DIR)/js/, $(JS_FILES))
JS_OBJ = $(patsubst $(SRC_DIR)/js/%.c, $(OBJ_DIR)/js/%.o, $(JS_SRC))

# RBAC sources
RBAC_FILES = rbac.c rbac_refcount.c jwt.c
RBAC_SRC = $(addprefix $(SRC_DIR)/rbac/, $(RBAC_FILES))
RBAC_OBJ = $(patsubst $(SRC_DIR)/rbac/%.c, $(OBJ_DIR)/rbac/%.o, $(RBAC_SRC))

# Query sources
QUERY_FILES = query_language.c
QUERY_SRC = $(addprefix $(SRC_DIR)/query/, $(QUERY_FILES))
QUERY_OBJ = $(patsubst $(SRC_DIR)/query/%.c, $(OBJ_DIR)/query/%.o, $(QUERY_SRC))

# Transaction sources
TRANSACTION_FILES = transaction.c transaction_log.c transaction_retry.c \
                   transaction_visualization.c transaction_performance_metrics.c \
                   transaction_visualization_enhanced.c
TRANSACTION_SRC = $(addprefix $(SRC_DIR)/transaction/, $(TRANSACTION_FILES))
TRANSACTION_OBJ = $(patsubst $(SRC_DIR)/transaction/%.c, $(OBJ_DIR)/transaction/%.o, $(TRANSACTION_SRC))

# Tools sources - These are built separately to avoid multiple main() issues
# Placeholder for empty tools object so other references compile
TOOLS_OBJ =

# Utility sources
UTILS_FILES = cache.c import_export.c json.c json_helpers.c logger.c metrics.c config_loader.c js_file_utils.c
UTILS_SRC = $(addprefix $(SRC_DIR)/utils/, $(UTILS_FILES))
UTILS_OBJ = $(patsubst $(SRC_DIR)/utils/%.c, $(OBJ_DIR)/utils/%.o, $(UTILS_SRC))

# Memory management sources
MEMORY_FILES = ref_counter.c ref_json.c
MEMORY_SRC = $(addprefix $(SRC_DIR)/utils/memory/, $(MEMORY_FILES))
MEMORY_OBJ = $(patsubst $(SRC_DIR)/utils/memory/%.c, $(OBJ_DIR)/utils/memory/%.o, $(MEMORY_SRC))

# All object files for linking (excluding tools with their own main)
SERVER_OBJ = $(CORE_OBJ) $(DATABASE_OBJ) $(API_OBJ) $(JS_OBJ) $(RBAC_OBJ) $(QUERY_OBJ) \
            $(TRANSACTION_OBJ) $(UTILS_OBJ) $(MEMORY_OBJ)

# Binary names
SERVER_EXEC = $(BIN_DIR)/jsondb_server
METRICS_EXEC = $(BIN_DIR)/jsondb_metrics
TOOLS_EXEC = $(BIN_DIR)/jsondb_tools

# PID file and log locations
PID_FILE = $(RUN_DIR)/jsondb_server.pid
LOG_FILE = $(LOG_DIR)/server.log

# Default target
all: info $(BIN_DIR) $(SERVER_EXEC) link_configs
	@echo ""
	@echo "Build completed successfully!"
	@echo "Run with: $(SERVER_EXEC)"
	@echo "  Daemon mode: $(SERVER_EXEC) -daemon"
	@echo "  Stop daemon: $(SERVER_EXEC) -stop"
	@echo "  Status check: $(SERVER_EXEC) -status"

# Build targets for different modes
release: all
	@echo "Release build completed."

asan: all
	@echo "AddressSanitizer build completed."

memcheck: all
	@echo "Valgrind-ready build completed."

# Tools are built separately and have their own build targets
tools: $(BIN_DIR) $(METRICS_EXEC) $(TOOLS_EXEC)
	@echo "Tools build completed."

# JavaScript API tests
js-tests: all
	@echo "Building and running JavaScript API tests..."
	@cd tests && $(MAKE) -f Makefile run

# Build info
info:
	@echo "Building JSON Database Server"
	@echo "=============================="

# Link configuration files to appropriate locations
link_configs:
	@echo "Setting up configuration files..."
	@if [ ! -f "$(DATA_DIR)/db.json" ]; then \
		cp -f db.json $(DATA_DIR)/db.json 2>/dev/null || echo "No db.json to copy"; \
	fi
	@if [ ! -f "$(DATA_DIR)/rbac.json" ]; then \
		cp -f rbac.json $(DATA_DIR)/rbac.json 2>/dev/null || echo "No rbac.json to copy"; \
	fi

# Build rules for server
$(SERVER_EXEC): $(SERVER_OBJ)
	@echo "Linking server executable..."
	@echo "Output directory: $(BIN_DIR)"
	@mkdir -p $(BIN_DIR)
	$(CC) $(SERVER_OBJ) -o $@ $(LDFLAGS)

# Compile module source files
$(OBJ_DIR)/core/%.o: $(SRC_DIR)/core/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/database/%.o: $(SRC_DIR)/database/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/api/%.o: $(SRC_DIR)/api/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/js/%.o: $(SRC_DIR)/js/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/rbac/%.o: $(SRC_DIR)/rbac/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/query/%.o: $(SRC_DIR)/query/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/transaction/%.o: $(SRC_DIR)/transaction/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tools/%.o: $(SRC_DIR)/tools/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/utils/%.o: $(SRC_DIR)/utils/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/utils/memory/%.o: $(SRC_DIR)/utils/memory/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Create directories
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Run server in different modes
run: $(SERVER_EXEC)
	$(SERVER_EXEC)

daemon: $(SERVER_EXEC)
	$(SERVER_EXEC) -daemon -port 5000

stop:
	@if [ -f $(SERVER_EXEC) ]; then \
		$(SERVER_EXEC) -stop; \
	else \
		echo "Server executable not found. Build it first with 'make'."; \
	fi

status:
	@if [ -f $(SERVER_EXEC) ]; then \
		$(SERVER_EXEC) -status; \
	else \
		echo "Server executable not found. Build it first with 'make'."; \
	fi

# Clean
clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(BIN_DIR)

# Clean runtime files but leave directories
clean-runtime:
	rm -f $(PID_FILE)
	rm -f $(LOG_DIR)/*.log

# Rebuild all
rebuild: clean all

# Start and stop shortcuts
start: daemon

restart: stop daemon
	@echo "Server restarted"

# Additional build rules for tools
tools-build: $(SERVER_EXEC)
	@echo "Building tools in $(SRC_DIR)/tools"
	@mkdir -p $(BIN_DIR)
	$(CC) $(SRC_DIR)/tools/jsondb_metrics.c $(UTILS_OBJ) $(MEMORY_OBJ) -o $(METRICS_EXEC) $(LDFLAGS)
	$(CC) $(SRC_DIR)/tools/jsondb_tools.c $(DATABASE_OBJ) $(UTILS_OBJ) $(MEMORY_OBJ) -o $(TOOLS_EXEC) $(LDFLAGS)

# Install target for system-wide installation
install: $(SERVER_EXEC)
	@echo "Installing JSON Database Server..."
	@mkdir -p /usr/local/bin
	@mkdir -p /etc/jsondb
	@mkdir -p /var/run/jsondb
	@mkdir -p /var/log/jsondb
	@mkdir -p /var/lib/jsondb
	@install -m 755 $(SERVER_EXEC) /usr/local/bin/
	@install -m 644 $(ETC_DIR)/jsondb.conf /etc/jsondb/
	@echo "Installation complete"

.PHONY: all clean run rebuild tools-build info release asan memcheck tools daemon stop status start restart clean-runtime link_configs install js-tests
