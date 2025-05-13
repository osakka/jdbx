# Script and Test Reorganization

This document outlines the reorganization of scripts and tests in the JSONdb project to improve maintainability and organization.

## Overview

We have reorganized all scripts and tests into logical directories based on their purpose and the components they relate to. This makes it easier to find relevant files and reduces clutter in the project root.

## New Directory Structure

### Scripts

All scripts are now organized in the `/scripts` directory:

- **`/scripts/build/`**: Build-related scripts
- **`/scripts/testing/`**: Testing scripts
- **`/scripts/maintenance/`**: Code maintenance scripts
- **`/scripts/js/`**: JavaScript-related scripts

### Tests

All tests are now organized in the `/tests` directory:

- **`/tests/unit/`**: Unit tests for individual components
- **`/tests/integration/`**: Integration tests
- **`/tests/performance/`**: Performance and stress tests
- **`/tests/js/`**: JavaScript-related tests
- **`/tests/database/`**: Database component tests
- **`/tests/cache/`**: Cache component tests
- **`/tests/scripts/`**: Test utility scripts

## Running Tests

We've created a unified Makefile (`/tests/Makefile.unified`) that provides a centralized way to run all tests. This Makefile includes targets for running specific categories of tests:

```bash
# Run all tests
make -f Makefile.unified

# Run specific test categories
make -f Makefile.unified unit-test
make -f Makefile.unified integration-test
make -f Makefile.unified performance-test
# etc.
```

## Adding New Files

When adding new files:

1. Place scripts in the appropriate subdirectory under `/scripts`
2. Place tests in the appropriate subdirectory under `/tests`
3. Update the relevant README.md in the directory with information about the new file
4. Update the Makefiles if necessary

## Migration Note

This reorganization was performed to improve code organization and maintainability. No functionality has been modified; only the file locations have changed.