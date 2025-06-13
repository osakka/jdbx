# JDBX Development Documentation

> Resources for building, testing, and contributing to JDBX

## Development Overview

This section contains everything you need to develop JDBX, contribute to the project, or build extensions.

## Developer Guides

### Getting Started
- **[Building from Source](building.md)** - Compilation and build instructions
- **[Development Environment](dev-environment.md)** - Setting up your workspace
- **[Project Structure](project-structure.md)** - Understanding the codebase

### Contributing
- **[Contributing Guide](contributing.md)** - How to contribute to JDBX
- **[Code Style Guide](code-style.md)** - Coding standards and conventions
- **[Git Workflow](git-workflow.md)** - Branching and commit guidelines

### Testing
- **[Testing Guide](testing.md)** - Running and writing tests
- **[Test Coverage](test-coverage.md)** - Coverage requirements and reports
- **[Performance Testing](performance-testing.md)** - Benchmarking guidelines

### Documentation
- **[Documentation Standards](documentation-standards.md)** - Writing documentation
- **[Style Guide](style-guide.md)** - Documentation style and formatting
- **[API Documentation](api-documentation.md)** - Documenting APIs

### Implementation
- **[Implementation Checklist](implementation-checklist.md)** - Feature development guide
- **[Debugging Guide](debugging.md)** - Debugging techniques and tools
- **[Memory Management](memory-management.md)** - Memory handling best practices

## Quick Start for Contributors

### 1. Fork and Clone
```bash
git clone https://github.com/yourusername/jdbx.git
cd jdbx
git remote add upstream https://github.com/jdbx/jdbx.git
```

### 2. Build
```bash
cd src
make clean && make
```

### 3. Run Tests
```bash
cd tests
make test
```

### 4. Make Changes
```bash
git checkout -b feature/your-feature
# Make your changes
git add .
git commit -m "feat: add your feature"
```

### 5. Submit PR
```bash
git push origin feature/your-feature
# Open PR on GitHub
```

## Development Principles

### Code Quality
1. **Zero Warnings**: Compile with `-Wall -Wextra`
2. **Memory Safety**: No leaks, proper cleanup
3. **Thread Safety**: Proper synchronization
4. **Error Handling**: Check all returns
5. **Documentation**: Comment complex logic

### Performance
1. **Measure First**: Profile before optimizing
2. **Cache Friendly**: Consider data locality
3. **Lock-Free**: Where possible
4. **Zero-Copy**: Minimize data movement
5. **Batch Operations**: Amortize costs

### Testing
1. **Unit Tests**: For all functions
2. **Integration Tests**: For workflows
3. **Performance Tests**: For benchmarks
4. **Stress Tests**: For limits
5. **Regression Tests**: For bugs

## Development Tools

### Required
- GCC 7+ or Clang 10+
- Make
- Git
- Valgrind (memory debugging)
- GDB (debugging)

### Recommended
- VSCode with C/C++ extension
- clang-format (code formatting)
- cppcheck (static analysis)
- perf (profiling)
- htop (monitoring)

## Project Structure

```
jdbx/
├── src/              # Source code
│   ├── components/   # Main components
│   ├── include/      # Header files
│   └── Makefile      # Build system
├── tests/            # Test suite
├── docs/             # Documentation
├── scripts/          # Helper scripts
└── examples/         # Example code
```

## Key Components

### Core Database
- `database.c` - Main database operations
- `operations.c` - CRUD operations
- `index.c` - Indexing system

### Storage
- `mmap_storage.c` - Memory-mapped storage
- `binary_format.c` - Binary serialization

### Server
- `server.c` - HTTP server
- `api.c` - REST API handlers
- `handle_client.c` - Request processing

### Utilities
- `json.c` - JSON parser/generator
- `logger.c` - Logging system
- `cache.c` - Caching layer

## Debugging Tips

### Memory Issues
```bash
# Run with valgrind
valgrind --leak-check=full ./build/jdbxd

# Use address sanitizer
make clean && make CFLAGS="-fsanitize=address"
```

### Performance
```bash
# Profile with perf
perf record ./build/jdbxd
perf report

# Generate flame graph
perf script | flamegraph.pl > flame.svg
```

### Core Dumps
```bash
# Enable core dumps
ulimit -c unlimited

# Debug core
gdb ./build/jdbxd core
```

## Common Tasks

### Adding a New API Endpoint
1. Define route in `api.c`
2. Implement handler function
3. Add tests in `tests/`
4. Update API documentation
5. Update CHANGELOG.md

### Adding a New Feature
1. Create implementation plan
2. Write tests first (TDD)
3. Implement feature
4. Update documentation
5. Submit PR with tests

### Fixing a Bug
1. Write failing test
2. Fix the bug
3. Verify test passes
4. Check for regressions
5. Update CHANGELOG.md

## Release Process

1. **Version Bump**: Update version in code
2. **Changelog**: Update CHANGELOG.md
3. **Tests**: Run full test suite
4. **Docs**: Update documentation
5. **Tag**: Create version tag
6. **Release**: Create GitHub release

## Getting Help

- **Development Chat**: [Discord](https://discord.gg/jdbx)
- **Issues**: [GitHub Issues](https://github.com/jdbx/jdbx/issues)
- **Discussions**: [GitHub Discussions](https://github.com/jdbx/jdbx/discussions)

## See Also

- [Architecture Documentation](../architecture/README.md)
- [API Reference](../api/README.md)
- [Testing Framework](testing.md)
- [Contributing Guide](contributing.md)