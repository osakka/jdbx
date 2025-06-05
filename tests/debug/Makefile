# Makefile for B+tree and Hash Index debugging

CC = gcc
CFLAGS = -g -O0 -Wall -Wextra -I../../src/include
LDFLAGS = -L../../build/lib -ljsondb -lpthread -lm
SANITIZE = -fsanitize=address -fno-omit-frame-pointer

# Check if AddressSanitizer is available
HAS_ASAN := $(shell $(CC) -fsanitize=address -x c -E /dev/null >/dev/null 2>&1 && echo "yes")

ifeq ($(HAS_ASAN),yes)
    CFLAGS += $(SANITIZE)
    LDFLAGS += $(SANITIZE)
else
    $(warning AddressSanitizer not available, using regular debug build)
endif

all: test_btree_hash_debug test_minimal

test_btree_hash_debug: test_btree_hash_debug.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Minimal test that doesn't need full library
test_minimal: test_minimal.c
	$(CC) $(CFLAGS) -o $@ $< -lpthread

test_minimal.c:
	@echo "Creating minimal test..."
	@echo '#include <stdio.h>' > test_minimal.c
	@echo '#include <stdlib.h>' >> test_minimal.c
	@echo '#include <string.h>' >> test_minimal.c
	@echo '' >> test_minimal.c
	@echo 'int main() {' >> test_minimal.c
	@echo '    printf("Testing index headers...\n");' >> test_minimal.c
	@echo '    #if __has_include("index/btree_disk.h")' >> test_minimal.c
	@echo '        printf("✓ btree_disk.h found\n");' >> test_minimal.c
	@echo '    #else' >> test_minimal.c
	@echo '        printf("✗ btree_disk.h NOT found\n");' >> test_minimal.c
	@echo '    #endif' >> test_minimal.c
	@echo '    return 0;' >> test_minimal.c
	@echo '}' >> test_minimal.c

# Run tests with different configurations
test: all
	@echo "=== Running minimal test ==="
	./test_minimal
	@echo ""
	@echo "=== Running B+tree test ==="
	-./test_btree_hash_debug btree
	@echo ""
	@echo "=== Running hash test ==="
	-./test_btree_hash_debug hash
	@echo ""
	@echo "=== Running small stress test ==="
	-./test_btree_hash_debug stress 10

# Run with GDB
debug-btree: test_btree_hash_debug
	gdb -ex "run btree" -ex "bt" -ex "quit" ./test_btree_hash_debug

debug-hash: test_btree_hash_debug
	gdb -ex "run hash" -ex "bt" -ex "quit" ./test_btree_hash_debug

# Run with Valgrind
valgrind-btree: test_btree_hash_debug
	valgrind --leak-check=full --track-origins=yes ./test_btree_hash_debug btree

valgrind-hash: test_btree_hash_debug
	valgrind --leak-check=full --track-origins=yes ./test_btree_hash_debug hash

# Clean up
clean:
	rm -f test_btree_hash_debug test_minimal test_minimal.c
	rm -f /tmp/test_debug.btree /tmp/test_debug.hash /tmp/stress.btree

.PHONY: all test debug-btree debug-hash valgrind-btree valgrind-hash clean