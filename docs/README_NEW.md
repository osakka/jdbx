# JSONdb Documentation

Welcome to the JSONdb documentation. This guide will help you navigate through all available documentation.

## 📚 Documentation Structure

### Getting Started
- **[Installation Guide](getting-started/installation.md)** - System requirements and installation steps
- **[Quick Start](getting-started/quick-start.md)** - Get up and running in 5 minutes
- **[Configuration](getting-started/configuration.md)** - Configuration options and environment variables

### API Reference
- **[REST API](api/API.md)** - Complete REST API reference
- **[JavaScript API](api/JAVASCRIPT_API.md)** - JavaScript integration and functions
- **[RBAC API](api/RBAC_API.md)** - Role-based access control endpoints

### Core Features
- **[Binary Persistence](../BINARY_FORMAT.md)** - High-performance binary storage format
- **[RBAC System](RBAC_COMPLETE_DOCUMENTATION.md)** - Role-based access control
- **[Metrics System](reference/METRICS.md)** - Performance monitoring and metrics
- **[Transactions](reference/TRANSACTIONS.md)** - ACID transaction support
- **[JavaScript Engine](reference/JAVASCRIPT.md)** - QuickJS integration

### Architecture
- **[System Architecture](architecture/project_structure.md)** - Overall system design
- **[Component Organization](architecture/component_organization.md)** - Code structure
- **[Component Interactions](architecture/component_interactions.md)** - How components work together

### Operations
- **[Deployment Guide](reference/PRODUCTION_READINESS.md)** - Production deployment
- **[Performance Tuning](reference/PERFORMANCE_BOTTLENECKS.md)** - Optimization guide
- **[Troubleshooting](reference/INVESTIGATION_SUMMARY.md)** - Common issues and solutions

### Development
- **[Building from Source](build/README.md)** - Build instructions
- **[Testing](testing/TESTING_FRAMEWORK.md)** - Testing approach and tools
- **[Contributing](project/CONTRIBUTING.md)** - Contribution guidelines

### Reference
- **[Configuration Reference](reference/CONFIGURATION.md)** - All configuration options
- **[Environment Variables](reference/ENVIRONMENT_CONFIGURATION.md)** - Environment setup
- **[Database Operations](reference/DATABASE_LOCKING.md)** - Database internals
- **[Security Guidelines](security/SECURITY_GUIDELINES.md)** - Security best practices

## 🔍 Quick Links

### Most Common Tasks
1. [Install JSONdb](getting-started/installation.md)
2. [Create a Collection](api/API.md#create-collection)
3. [Insert Documents](api/API.md#insert-document)
4. [Query Documents](api/API.md#query-documents)
5. [Set Up Authentication](api/RBAC_API.md#authentication)

### Key Concepts
- [Document Structure](api/API.md#document-structure)
- [Query Language](reference/QUERY_LANGUAGE.md)
- [Permissions Model](RBAC_SCHEMA_DESIGN.md)
- [Binary Format](../BINARY_FORMAT.md#format-specification)

### Troubleshooting
- [Server Won't Start](reference/INVESTIGATION_SUMMARY.md#startup-issues)
- [Authentication Issues](AUTHENTICATION_FIX_PLAN.md)
- [Performance Problems](reference/PERFORMANCE_BOTTLENECKS.md)
- [Binary Format Issues](BINARY_PERSISTENCE.md#troubleshooting)

## 📖 Documentation Standards

All documentation follows these principles:
1. **Accuracy** - Documentation reflects the current codebase
2. **Clarity** - Clear, concise language with examples
3. **Organization** - Logical structure with clear navigation
4. **Maintenance** - Regular updates with version changes

## 🔄 Recent Updates

- **v2.0.6** - Fixed server startup issues and binary serialization crash
- **v2.0.5** - Metrics system overhaul with time-series support
- **v2.0.4** - RBAC display fixes and session management
- **v2.0.0** - Binary persistence system with 4-6x performance improvement

## 📝 Contributing to Documentation

See [Documentation Guidelines](guidelines/GIT_GUIDELINES.md) for:
- Documentation standards
- Markdown formatting
- Cross-reference conventions
- Update procedures

## 🗂️ Full Documentation Index

### API Documentation (`/api`)
- [API.md](api/API.md) - REST API reference
- [JAVASCRIPT_API.md](api/JAVASCRIPT_API.md) - JavaScript integration
- [RBAC_API.md](api/RBAC_API.md) - RBAC endpoints

### Architecture Documentation (`/architecture`)
- [project_structure.md](architecture/project_structure.md)
- [component_organization.md](architecture/component_organization.md)
- [component_interactions.md](architecture/component_interactions.md)
- [code_organization.md](architecture/code_organization.md)

### Reference Documentation (`/reference`)
- [CONFIGURATION.md](reference/CONFIGURATION.md) - Configuration guide
- [METRICS.md](reference/METRICS.md) - Metrics system
- [TRANSACTIONS.md](reference/TRANSACTIONS.md) - Transaction support
- [QUERY_LANGUAGE.md](reference/QUERY_LANGUAGE.md) - Query syntax
- [JAVASCRIPT.md](reference/JAVASCRIPT.md) - JavaScript engine

### Guidelines (`/guidelines`)
- [LOGGING_STANDARDS.md](guidelines/LOGGING_STANDARDS.md) - Logging conventions
- [SECURITY_GUIDELINES.md](security/SECURITY_GUIDELINES.md) - Security practices
- [GIT_GUIDELINES.md](guidelines/GIT_GUIDELINES.md) - Git workflow

## 🔗 External Resources

- [GitHub Repository](https://github.com/yourusername/jsondb)
- [Issue Tracker](https://github.com/yourusername/jsondb/issues)
- [Release Notes](../CHANGELOG.md)

---

*Last updated: May 28, 2025 - v2.0.6*