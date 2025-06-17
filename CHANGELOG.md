# JDBX Changelog

All notable changes to the JDBX database server project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [6.3.0] - 2025-06-17

### Added
- Revolutionary checkpoint-based memory management system
- Complete memory manager with automatic cleanup on error paths
- Thread-local checkpoint stacks for concurrent operation safety
- Memory promotion API to preserve allocations across checkpoints
- Comprehensive memory management documentation

### Changed
- All 304 allocation calls migrated to unified memory system
- BUFFER_* macros now internally use memory_manager
- Memory manager initialized FIRST in main() before any component
- Updated buffer pool architecture documentation

### Fixed
- SSL connection race conditions causing libcrypto crashes
- RBAC API queries now properly use unified documents architecture
- Critical memory corruption in CORS header handling
- HTTP response handling memory safety issues
- JWT cache corruption issues (temporarily disabled)

### Security
- Added atomic operations for thread-safe SSL cleanup
- Removed hardcoded admin password backdoor
- Enhanced defensive programming with null checks
- Fixed buffer overflow vulnerabilities

## [6.2.0] - 2025-06-16

### Added
- Enterprise configuration security infrastructure
- Cryptographic JWT secret generation (64-character)
- Three-tier configuration system (env → CLI → database)
- Bootstrap admin credentials via environment variables
- Comprehensive CLI with 33 configuration options

### Changed
- Complete elimination of ALL hardcoded security values
- Runtime scripts now validate credentials before server start
- Production-ready defaults with security warnings

### Security
- No more hardcoded admin/admin credentials
- JWT secrets generated with /dev/urandom
- Required JDBX_BOOTSTRAP_ADMIN_USER/PASS for production
- Password length validation and security warnings

## [6.1.0] - 2025-06-16

### Added
- Buffer pool managed JSON storage architecture
- Enterprise-grade memory lifecycle management
- Duplicate metrics prevention logic
- JWT cache memory safety improvements

### Fixed
- Critical segmentation faults in skiplist storage
- Metrics duplication bug (1500+ duplicate documents)
- Memory leak prevention with proper cleanup
- Thread safety with atomic operations

### Performance
- 50+ concurrent operations with 100% success rate
- Zero crashes during intensive workloads
- High-concurrency operations without corruption

## [6.0.0] - 2025-06-15

### Added
- TRUE Unified Documents Architecture
- Mixed routing support for physical and virtual collections
- Storage/Virtual function separation
- Field-based document discrimination
- Library-scoped metrics architecture
- JavaScript-enhanced analytics functions

### Changed
- ALL 25+ system components converted to unified storage
- Single physical collection for all documents
- Virtual collections based on document fields
- No hierarchical storage - everything unified

### Performance
- O(log n) operations with automatic indexing
- Simplified architecture eliminates data duplication
- Enhanced scalability with unified approach

## Earlier Versions

For changes prior to v6.0.0, please refer to git history and commit messages.