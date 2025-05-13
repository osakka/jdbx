# JSONdb Source Code Structure

This directory contains the complete source code for the JSONdb server.

## Directory Organization

- **components/** - Implementation files (.c)
  - Each subdirectory represents a logical component of the system
  - Main entry point is in `components/main.c`

- **include/** - Header files (.h)
  - Mirrors the structure of the components directory
  - Public API interfaces and type definitions

## Build System

The project uses a single Makefile (`src/Makefile`) to build the entire system. The output is a single executable at `bin/jsondb_server`.

To build:
```
make
```

To verify the codebase organization:
```
make verify-organization
```

## Code Organization Principles

1. **Single Source of Truth**: Each component has exactly one implementation location
2. **Clean Separation**: Headers in `include/`, implementation in `components/`
3. **Component-based Design**: Functionality is organized into logical components