# Root Makefile for JSONdb
# This Makefile forwards commands to the actual Makefile in the src directory

# Default target
all:
	$(MAKE) -C src all

# Clean build files
clean:
	$(MAKE) -C src clean

# Run tests
test:
	$(MAKE) -C src test

# Special targets for building with different flags
js-disabled:
	$(MAKE) -C src js-disabled

js-enabled:
	$(MAKE) -C src js-enabled

metrics:
	$(MAKE) -C src metrics

debug:
	$(MAKE) -C src debug

release:
	$(MAKE) -C src release

# Phony targets
.PHONY: all clean test js-disabled js-enabled metrics debug release