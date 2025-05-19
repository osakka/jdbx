# Makefile for socket binding tests

CC = gcc
CFLAGS = -Wall -Wextra

TARGETS = socket_bind_test socket_test_simple

all: $(TARGETS)

socket_bind_test: socket_bind_test.c
	$(CC) $(CFLAGS) $< -o $@

socket_test_simple: socket_test_simple.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)

.PHONY: all clean