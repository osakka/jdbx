# Makefile for memory optimization tests

CC = gcc
CFLAGS = -Wall -Wextra -g -O2 -I../../src/include -I../../src -DUSE_QUICKJS
LDFLAGS = -L../../build/lib -ljsondb -lpthread -lm

# Test programs
TESTS = test_memory_optimizations

# Object files directory
OBJDIR = ../../build/obj/tests/memory

# Create object directory
$(shell mkdir -p $(OBJDIR))

all: $(TESTS)

test_memory_optimizations: $(OBJDIR)/test_memory_optimizations.o
	$(CC) $< -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TESTS) $(OBJDIR)/*.o

# Run tests
test: all
	@echo "Running memory optimization tests..."
	./test_memory_optimizations

.PHONY: all clean test