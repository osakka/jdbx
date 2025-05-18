# Changelog

All notable changes to JSONdb will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Improved database locking system using read-write locks for higher concurrency
- Improved server startup sequence with proper dependency ordering
- Enhanced socket binding thread synchronization
- Better error handling for socket binding failures

### Changed
- Database operations now use read-write locks to reduce contention and improve performance
- Socket initialization and binding now occurs after database initialization but before RBAC and API initialization
- Improved command-line verbose mode handling
- Removed deprecated foreground mode in favor of verbose mode

### Fixed
- Race conditions in database operations with improved locking strategy
- Race condition in server socket binding
- Socket binding thread synchronization issues
- Command-line parsing for verbose mode
- All compiler warnings throughout the codebase to allow -Wall -Wextra -Werror compilation
- Unused function warnings in RBAC implementation
- Unused parameter warnings in JavaScript engine callbacks
- Format-truncation warnings in string handling functions
- Sign comparison warnings in JavaScript file operations
- External library warnings through selective suppression with wrapper headers

## [1.0.6-database-rbac]

### Added
- Database-based RBAC system with database collections
- RBAC API for user, role, and permission management
- Automatic migration from file-based to database-based RBAC
- Token refresh mechanism for improved authentication
- Comprehensive JavaScript functions support
- Document validators and transformers
- JavaScript query capabilities
- Transaction visualization and monitoring
- Enhanced transaction logging and metrics

### Fixed
- CORS implementation for cross-origin requests
- Web interface collection creation and document management
- Unused function warnings and integration of previously uncalled functions
- Repository structure cleanup and organization
- SameSite cookie attributes for improved security

## [1.0.5]

### Added
- Transaction support for atomic operations with isolation levels
- Transaction visualization for monitoring
- Transaction logs and metrics
- JavaScript integration via QuickJS

### Changed
- Enhanced document caching system with invalidation

## [1.0.0]

### Added
- Core database functionality (collections, documents, CRUD operations)
- RESTful API with proper error handling
- Web-based admin interface
- CORS support with proper handling of preflight requests
- Authentication via JWT tokens
- Role-Based Access Control (RBAC)
- Document caching system