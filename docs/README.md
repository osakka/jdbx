# JSONdb Documentation

Welcome to the JSONdb documentation. This guide will help you navigate the comprehensive documentation for the JSON Database Server.

## Quick Links

- [API Reference](api/API_CORRECTED.md) - Complete API documentation (corrected version)
- [Getting Started](../README.md#getting-started) - Installation and quick start
- [Configuration Guide](reference/CONFIG.md) - Server configuration options
- [CHANGELOG](../CHANGELOG.md) - Version history and updates

## Documentation Structure

### 🚀 Getting Started
- [Installation](../README.md#installation) - How to build and install JSONdb
- [Quick Start](../README.md#getting-started) - Basic usage examples
- [Configuration](reference/CONFIGURATION.md) - Configuration options

### 📚 API Documentation
- [API Reference](api/API_CORRECTED.md) - **USE THIS** - Corrected and verified API endpoints
- [RBAC API](api/RBAC_API.md) - Role-based access control endpoints
- [JavaScript API](api/JAVASCRIPT_API.md) - JavaScript extension endpoints
- [OpenAPI Spec](../share/htdocs/openapi.json) - OpenAPI 3.0 specification

### 📖 User Guides
- [Query Language](reference/QUERY_LANGUAGE.md) - Comprehensive query syntax
- [JavaScript Functions](guides/javascript_functions.md) - Creating JS extensions
- [Authentication](AUTHENTICATION_FIX_PLAN.md) - Authentication system guide
- [Transactions](reference/TRANSACTIONS.md) - Transaction support

### 🔧 Operations
- [Server Usage](reference/SERVER_USAGE.md) - Running the server
- [Metrics](reference/METRICS.md) - Monitoring and metrics
- [Performance](reference/PERFORMANCE_BOTTLENECKS.md) - Performance tuning
- [Production Readiness](reference/PRODUCTION_READINESS.md) - Production deployment

### 🏗️ Architecture
- [Project Structure](architecture/project_structure.md) - Codebase organization
- [Component Organization](architecture/component_organization.md) - System components
- [Binary Format](BINARY_FORMAT.md) - Binary persistence format
- [Database Locking](reference/DATABASE_LOCKING.md) - Concurrency control

### 🛠️ Development
- [Build System](reference/BUILD_FIX_PLAN.md) - Building from source
- [Testing Framework](testing/TESTING_FRAMEWORK.md) - Testing guidelines
- [Git Guidelines](guidelines/GIT_GUIDELINES.md) - Version control practices
- [Frontend Best Practices](guidelines/FRONTEND_BEST_PRACTICES.md) - UI development

### 📋 Reference
- [Configuration Reference](reference/CONFIG.md) - All configuration options
- [Environment Variables](reference/ENVIRONMENT_CONFIGURATION.md) - Environment setup
- [File Descriptions](reference/FILE_DESCRIPTIONS.md) - Source file guide
- [Tools](reference/TOOLS.md) - Available tools and utilities

## Important Notes

### ⚠️ Documentation Status

We are currently undergoing a major documentation reorganization. Please note:

1. **Use API_CORRECTED.md** for accurate API endpoint information
2. The original API.md contains several inaccuracies (wrong port, incorrect endpoints)
3. Backup/Restore endpoints shown in some docs are NOT implemented
4. Authentication is currently disabled for some collection endpoints

### 🔄 Recent Changes

- **v2.0.6** (2025-01-28): Fixed binary serialization crash, improved logging
- **v2.0.5**: Added session management with IP/UA tracking
- **v2.0.4**: Implemented time-series metrics with 10x performance improvement
- **v2.0.3**: Added binary persistence with automatic saves

### 📊 Documentation Audit Results

A comprehensive audit revealed:
- 154 total documentation files
- ~40% duplicate content
- Significant API documentation inaccuracies
- Missing documentation for new features

See [DOCUMENTATION_ACCURACY_AUDIT.md](DOCUMENTATION_ACCURACY_AUDIT.md) for details.

## Finding Information

### By Feature

**Authentication & Security**
- [Authentication Guide](AUTHENTICATION_FIX_PLAN.md)
- [RBAC Implementation](RBAC_IMPLEMENTATION_PLAN.md)
- [JWT Implementation](jwt/JWT_VERIFICATION_FIX.md)
- [Security Guidelines](security/SECURITY_GUIDELINES.md)

**Data Management**
- [Collections & Documents](api/API_CORRECTED.md#collections)
- [Query Language](reference/QUERY_LANGUAGE.md)
- [Indexing](api/API_CORRECTED.md#indexes)
- [Transactions](reference/TRANSACTIONS.md)

**Extensions**
- [JavaScript Integration](reference/JAVASCRIPT.md)
- [JavaScript Functions](guides/javascript_functions.md)
- [Schema Validation](api/API_CORRECTED.md#schemas)

**Operations**
- [Metrics Collection](METRICS_COLLECTION_IMPLEMENTATION.md)
- [Session Management](SESSION_MANAGEMENT_IMPLEMENTATION.md)
- [Binary Persistence](BINARY_PERSISTENCE.md)
- [Performance Tuning](reference/PERFORMANCE_BOTTLENECKS.md)

### By User Role

**Application Developers**
1. Start with [Getting Started](../README.md#getting-started)
2. Review [API Reference](api/API_CORRECTED.md)
3. Learn [Query Language](reference/QUERY_LANGUAGE.md)
4. Explore [JavaScript Extensions](guides/javascript_functions.md)

**System Administrators**
1. Read [Server Usage](reference/SERVER_USAGE.md)
2. Configure using [Configuration Guide](reference/CONFIG.md)
3. Monitor with [Metrics Guide](reference/METRICS.md)
4. Deploy with [Production Readiness](reference/PRODUCTION_READINESS.md)

**Contributors**
1. Understand [Architecture](architecture/project_structure.md)
2. Follow [Git Guidelines](guidelines/GIT_GUIDELINES.md)
3. Use [Testing Framework](testing/TESTING_FRAMEWORK.md)
4. Read [Contributing Guide](project/CONTRIBUTING.md)

## Directory Structure

The documentation is organized into these directories:

- `/api/` - API documentation and usage guides
- `/architecture/` - Design and architecture documentation
- `/build/` - Build-related documentation
- `/compiler/` - Compiler warning fixes and related information
- `/development/` - Developer documentation and build guides
- `/guidelines/` - Project guidelines and contribution rules
- `/guides/` - User guides and tutorials
- `/implementation/` - Implementation status and details
- `/integration/` - Integration documentation and status
- `/jwt/` - JWT verification and authentication details
- `/port/` - Port and network binding information
- `/rbac/` - Role-based access control documentation
- `/reference/` - Reference materials and detailed information
- `/socket-binding/` - Socket binding and network connection documentation
- `/src/` - Source code documentation
- `/status/` - Project status and progress tracking
- `/testing/` - Testing framework and approach
- `/threads/` - Thread management and synchronization
- `/tty/` - TTY and terminal handling documentation

## Known Issues

1. **Port Documentation**: Many docs show port 8080, but default is 5000
2. **RBAC Endpoints**: Some docs show `/api/rbac/*` but actual endpoints are `/api/*`
3. **Backup/Restore**: Documentation exists but feature is not implemented
4. **Duplicate Files**: Multiple versions of RBAC, metrics, and socket documentation

## Getting Help

- Check the [Troubleshooting Guide](../README.md#troubleshooting)
- Review [Implementation Status](IMPLEMENTATION_STATUS.md)
- See [Latest Changes](reference/LATEST_CHANGES.md)

## Contributing to Documentation

Please help improve our documentation:

1. Report inaccuracies via GitHub issues
2. Submit corrections via pull requests
3. Follow [Documentation Standards](guidelines/DOCUMENTATION_STANDARDS.md)
4. Test all code examples before submitting

## Documentation Standards

- All documentation is written in Markdown format
- Documentation follows the "One source of truth" principle from `guidelines/CLAUDE.md`
- README.md files may exist in code directories for context-specific guidance
- All substantial documentation belongs in the `/docs` directory

## Cross-References

When referring to other documents, use relative paths from the document location:

```markdown
Please see the [API documentation](../api/API.md) for details.
```

## Maintaining Documentation

- Keep documentation updated as code changes
- Follow the principles in `guidelines/CLAUDE.md`
- Document thoroughly with clear, concise language
- Update index files when adding new documentation

## Version

This documentation is for JSONdb v2.0.6. Last updated: January 28, 2025.

For version-specific documentation, see the [CHANGELOG](../CHANGELOG.md).