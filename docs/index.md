# JSONdb Documentation

**Version**: 2.0.7  
**Last Updated**: January 2025

Welcome to the JSONdb documentation. This index provides a comprehensive overview of all available documentation.

## Quick Start

- [README](../README.md) - Project overview and quick start guide
- [Installation Guide](guides/installation.md) - Detailed installation instructions
- [Getting Started](guides/getting-started.md) - Your first steps with JSONdb

## API Documentation

### REST API
- [API Reference](api/api-rest.md) - Complete REST API documentation
- [Authentication](api/api-rest.md#authentication) - JWT authentication guide
- [Collections](api/api-rest.md#collections) - Collection management
- [Documents](api/api-rest.md#documents) - Document CRUD operations
- [Queries](api/api-rest.md#queries) - Query language reference

### Specialized APIs
- [RBAC API](api/RBAC_API.md) - Role-based access control endpoints
- [Metrics API](reference/reference-metrics.md#api-endpoints) - Metrics collection and retrieval
- [Transaction API](reference/reference-transactions.md#api-reference) - Transaction management
- [JavaScript API](reference/reference-javascript.md#http-endpoints) - JavaScript execution endpoints

## Reference Documentation

### Core Systems
- [Configuration Reference](reference/reference-configuration.md) - All configuration options
- [RBAC Reference](reference/reference-rbac.md) - Complete RBAC system documentation
- [Binary Format](architecture/architecture-binary-format.md) - Binary persistence format
- [Socket Binding](reference/reference-socket-binding.md) - Network socket implementation

### Advanced Features
- [Transactions](reference/reference-transactions.md) - ACID transaction system
- [Metrics System](reference/reference-metrics.md) - Performance monitoring
- [JavaScript Integration](reference/reference-javascript.md) - QuickJS integration
- [Query Language](reference/QUERY_LANGUAGE.md) - Advanced query syntax

### Database Features
- [Indexing](reference/reference-indexing.md) - Database indexing system
- [Caching](reference/reference-caching.md) - Query and document caching
- [Persistence](reference/reference-persistence.md) - Data persistence mechanisms

## Architecture Documentation

- [System Architecture](architecture/architecture-overview.md) - High-level system design
- [Component Architecture](architecture/component_organization.md) - Internal component structure
- [Binary Format Design](architecture/architecture-binary-format.md) - TLV encoding details
- [Code Organization](architecture/code_organization.md) - Source code structure

## Development Guides

### Getting Started
- [Development Setup](guides/development-setup.md) - Setting up development environment
- [Building from Source](guides/building.md) - Compilation instructions
- [Testing Guide](guides/testing.md) - Running tests

### Best Practices
- [Frontend Best Practices](guidelines/FRONTEND_BEST_PRACTICES.md) - UI development guidelines
- [Git Guidelines](guidelines/GIT_GUIDELINES.md) - Version control practices
- [Logging Standards](guidelines/LOGGING_STANDARDS.md) - Logging conventions
- [Security Guidelines](security/SECURITY_GUIDELINES.md) - Security best practices

### Advanced Topics
- [JavaScript Functions](guides/javascript-development.md) - Writing JavaScript extensions
- [Custom Validators](guides/validators.md) - Creating document validators
- [Performance Tuning](guides/performance.md) - Optimization techniques

## Project Information

### Status and Planning
- [CHANGELOG](../CHANGELOG.md) - Version history and release notes
- [CLAUDE.md](../CLAUDE.md) - Development guidelines and principles
- [Project Status](status/project-status.json) - Current implementation status

### Implementation Details
- [Implementation Status](reference/IMPLEMENTATION_STATUS.md) - Feature completion status
- [TODO](src/TODO.md) - Pending tasks and future plans

## Tools and Utilities

- [JSONdb Tools](reference/TOOLS.md) - Command-line utilities
- [Metrics Exporter](reference/reference-metrics.md#monitoring-integration) - Prometheus integration
- [Migration Scripts](helpers/migrate_to_new_structure.sh) - Database migration tools

## Troubleshooting

- [Socket Binding Issues](reference/reference-socket-binding.md#troubleshooting)
- [Transaction Problems](reference/reference-transactions.md#troubleshooting)
- [JavaScript Errors](reference/reference-javascript.md#troubleshooting)
- [Performance Issues](reference/reference-metrics.md#troubleshooting)

## Examples

- [Basic Examples](../share/examples/basic/) - Simple usage examples
- [Advanced Examples](../share/examples/advanced/) - Complex scenarios
- [JavaScript Examples](../share/examples/js-examples/) - JavaScript integration examples
- [Configuration Examples](../share/examples/config/) - Configuration templates

## Contributing

- [Contributing Guide](project/CONTRIBUTING.md) - How to contribute
- [Code of Conduct](project/CODE_OF_CONDUCT.md) - Community guidelines
- [Development Guidelines](guidelines/REPOSITORY_ORGANIZATION.md) - Repository structure

## Quick Reference

### Common Tasks

1. **Start the Server**
   ```bash
   build/jsondb_runtime.sh start
   ```

2. **Check Status**
   ```bash
   build/jsondb_runtime.sh status
   ```

3. **View Logs**
   ```bash
   tail -f var/log/jsondb_server.log
   ```

4. **Test Connection**
   ```bash
   curl http://localhost:5000/api/health
   ```

### Default Credentials

- **Username**: admin
- **Password**: admin (change immediately!)
- **Port**: 5000
- **API Base**: http://localhost:5000/api

### Important Paths

- **Configuration**: `/opt/jsondb/build/config.json`
- **Database**: `/opt/jsondb/build/var/database.jdb`
- **Logs**: `/opt/jsondb/var/log/jsondb_server.log`
- **Web UI**: http://localhost:5000/

## Documentation Standards

All documentation follows the standards defined in [DOCUMENTATION_STANDARDS.md](DOCUMENTATION_STANDARDS.md).

## Search

To search the documentation:

```bash
# Search for a term
grep -r "search term" docs/

# Find files by name
find docs/ -name "*pattern*"
```

## Feedback

For documentation issues or improvements:
- File an issue on GitHub
- Submit a pull request
- Contact the maintainers

---

*This documentation is continuously updated. For the latest version, check the repository.*
