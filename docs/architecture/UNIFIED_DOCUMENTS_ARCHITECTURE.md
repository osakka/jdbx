# Unified Documents Architecture (v3.2.0)

**Date**: June 11, 2025  
**Status**: Production Ready and Fully Integrated  
**Version**: 3.2.0

## Overview

JDBX v3.2.0 introduces a revolutionary unified documents architecture where everything is treated as a document with type-based discrimination. This design provides unprecedented flexibility while maintaining high performance and data integrity.

## Core Principles

### Everything is a Document

All entities in JDBX are now stored as documents in a single 'documents' collection:

- **Users**: `{"type": "user", "username": "john@library1", "library": "library1", ...}`
- **Roles**: `{"type": "role", "name": "admin", "library": "library1", ...}`
- **Libraries**: `{"type": "library", "name": "library1", "settings": {...}, ...}`
- **Collections**: `{"type": "collection", "name": "products", "library": "library1", ...}`
- **Regular Data**: `{"type": "data", "collection": "products", "library": "library1", ...}`

### Hybrid Architecture

The system implements a hybrid approach:

1. **Metadata in Documents**: Entity metadata (users, roles, libraries, collections) stored in the unified documents collection
2. **Data in Traditional Paths**: Actual collection data remains in library/collection-specific storage paths for performance
3. **Unified Query Interface**: Single API can query across all entity types

### Type-Based Discrimination

Documents are differentiated by their `type` field:

```json
{
  "uuid": "doc-1234567890-abcdef",
  "type": "user",
  "username": "john@library1",
  "library": "library1",
  "email": "john@example.com",
  "roles": ["user"],
  "created_at": "2025-06-11T10:00:00Z"
}
```

## Library-First Design

### Library-Scoped Users

Users are now scoped to libraries, enabling multi-tenancy:

- **Format**: `username@library`
- **Examples**: 
  - `john@library1` (John in library1)
  - `john@library2` (Different John in library2)
  - `admin@system` (System administrator)

### Library Isolation

Each library operates as an isolated namespace:

1. **Separate User Bases**: Users in different libraries are completely isolated
2. **Independent Collections**: Each library has its own collections
3. **Isolated Permissions**: RBAC permissions are library-scoped
4. **Resource Quotas**: Libraries can have individual quotas and limits

### Default Collections

Each library automatically receives default collections:

- `users` - Library-specific user management
- `roles` - Library-specific roles and permissions
- `settings` - Library configuration
- `logs` - Library-specific audit logs

## System Actors

Special non-login accounts handle system operations:

### Built-in System Actors

- **system-admin**: Administrative operations
- **system-metrics**: Metrics collection and reporting
- **system-indexer**: Automatic index creation and maintenance
- **system-backup**: Backup and recovery operations
- **system-cleanup**: Maintenance and cleanup tasks

### System Actor Characteristics

1. **No Authentication**: Cannot be used for login
2. **RBAC Compliant**: All operations go through RBAC (no backdoors)
3. **Auditable**: All actions are logged and traceable
4. **Configurable**: Permissions can be modified via RBAC

## Function Embedding

JavaScript functions can be embedded directly in documents or referenced:

### Inline Functions

```json
{
  "type": "data",
  "collection": "products",
  "library": "ecommerce",
  "name": "Product Validator",
  "validator": {
    "embedded": true,
    "code": "function validate(doc) { return doc.price > 0; }"
  }
}
```

### Referenced Functions

```json
{
  "type": "data",
  "collection": "products", 
  "library": "ecommerce",
  "name": "Product Transformer",
  "transformer": "@function:ecommerce/product-formatter"
}
```

### Automatic Resolution

The system automatically resolves function references during execution:

1. **Parse Reference**: Extract library and function name from `@function:library/name`
2. **Locate Function**: Find function document in specified library
3. **Load Code**: Extract and compile JavaScript code
4. **Execute**: Run function with provided context

## Cascading Versioning Policy

Libraries can define versioning policies that cascade to all collections:

### Library-Level Policies

```json
{
  "type": "library",
  "name": "ecommerce",
  "versioning_policy": {
    "enabled": true,
    "max_versions": 10,
    "retention_days": 30,
    "auto_create": ["insert", "update", "delete"]
  }
}
```

### Automatic Version Creation

When enabled, versions are automatically created for:

1. **Document Insertion**: Creates version 1.0.0
2. **Document Updates**: Increments version based on change type
3. **Document Deletion**: Creates final version before deletion

### Version Cleanup

Automatic cleanup based on:

- **max_versions**: Maximum versions to retain per document
- **retention_days**: Maximum age of versions
- **cleanup_interval**: Frequency of cleanup operations

## API Integration

### Unified Query Interface

Single endpoint can query across all document types:

```bash
# Query users in a library
GET /api/libraries/library1/documents?type=user

# Query collections in a library  
GET /api/libraries/library1/documents?type=collection

# Query data in a specific collection
GET /api/libraries/library1/collections/products/documents
```

### Library Context

All operations respect library context:

1. **Automatic Scoping**: API automatically filters by library
2. **Permission Checks**: RBAC enforced at library level
3. **Resource Limits**: Library quotas applied to operations

### Cross-Library Operations

System administrators can perform cross-library operations:

```bash
# Global user search (system admin only)
GET /api/documents?type=user&username=john

# Cross-library analytics (system admin only)
GET /api/analytics/cross-library-report
```

## Browser Interface Adaptations

### Library Selector

The web interface includes a library selector in the collections panel:

1. **Library Dropdown**: Switch between accessible libraries
2. **Context Awareness**: All operations respect selected library
3. **Permission-Based**: Only shows libraries user has access to

### Library-Scoped Operations

All browser operations are library-scoped:

- **Collection Management**: Create/edit collections within library context
- **User Management**: Manage users within current library
- **Role Assignment**: Assign roles within library boundaries
- **Data Operations**: All CRUD operations respect library context

### Library Management

Administrative users can:

1. **Create Libraries**: Set up new library namespaces
2. **Configure Settings**: Manage library-specific configuration
3. **Set Quotas**: Define resource limits per library
4. **Monitor Usage**: View library-specific metrics

## Performance Characteristics

### Index Optimization

The unified architecture includes optimized indexing:

1. **Type-Based Indexes**: Separate indexes for each document type
2. **Library-Scoped Indexes**: Indexes partitioned by library
3. **Composite Indexes**: Multi-field indexes for common queries

### Caching Strategy

Intelligent caching at multiple levels:

1. **Document Cache**: Frequently accessed documents cached in memory
2. **Library Cache**: Library metadata and settings cached
3. **Permission Cache**: RBAC decisions cached per user/library

### Storage Efficiency

Hybrid storage approach optimizes for both metadata and data:

1. **Metadata Storage**: Small, frequently queried metadata in unified collection
2. **Data Storage**: Large documents in optimized collection-specific storage
3. **Index Storage**: Separate index files for different access patterns

## Migration and Compatibility

### Backward Compatibility

The unified architecture maintains backward compatibility:

1. **Existing APIs**: All existing API endpoints continue to work
2. **Data Format**: Existing documents automatically include type information
3. **Gradual Migration**: Systems can migrate incrementally

### Migration Tools

Built-in tools assist with migration:

```bash
# Convert existing data to unified format
jdbx_tools --migrate-to-unified /path/to/database

# Verify unified architecture
jdbx_tools --verify-unified /path/to/database

# Generate migration report
jdbx_tools --migration-report /path/to/database
```

## Security Implications

### Enhanced Security

The unified architecture provides enhanced security:

1. **Library Isolation**: Complete isolation between libraries prevents data leakage
2. **Granular Permissions**: Field-level permissions within documents
3. **Audit Trail**: All operations logged with library context
4. **System Actor Control**: No privileged backdoors, all operations audited

### Multi-Tenancy

Perfect for multi-tenant deployments:

1. **Tenant Isolation**: Each tenant gets a separate library
2. **Resource Control**: Per-tenant quotas and limits
3. **Independent Administration**: Tenant-specific administrators
4. **Billing Integration**: Usage tracking per library/tenant

## Implementation Details

### Core Components

Key files implementing the unified architecture:

- `src/components/database/unified_documents.c` - Core unified document operations
- `src/components/database/library_metadata.c` - Library management
- `src/components/database/versioning_policy.c` - Cascading versioning
- `src/components/api/library_api.c` - Library-specific API endpoints
- `src/components/rbac/rbac_database.c` - Library-scoped RBAC

### Database Schema

The documents collection uses a flexible schema:

```sql
-- Conceptual schema (JDBX uses document storage)
documents {
  uuid: string (primary key),
  type: string (indexed),
  library: string (indexed),
  collection: string (optional, indexed),
  created_at: timestamp,
  updated_at: timestamp,
  version: string,
  data: json (flexible document content)
}
```

### Type Handlers

Specialized handlers for each document type:

1. **UserHandler**: Manages user documents and authentication
2. **RoleHandler**: Manages role documents and permissions
3. **LibraryHandler**: Manages library documents and settings
4. **CollectionHandler**: Manages collection metadata
5. **DataHandler**: Manages regular data documents

## Future Enhancements

### Planned Features

1. **Cross-Library Replication**: Replicate specific collections across libraries
2. **Library Templates**: Pre-configured library setups for common use cases
3. **Dynamic Sharding**: Automatic sharding based on library usage patterns
4. **Advanced Analytics**: Cross-library analytics and reporting

### Performance Optimizations

1. **Predictive Caching**: Machine learning-based cache prediction
2. **Auto-Indexing**: Automatic index creation based on query patterns
3. **Storage Tiering**: Hot/warm/cold storage tiers per library
4. **Compression**: Library-specific compression strategies

## Conclusion

The unified documents architecture represents a fundamental evolution in JDBX's design, providing unparalleled flexibility while maintaining the performance and reliability that JDBX is known for. This architecture enables true multi-tenancy, simplifies data management, and provides a foundation for future enhancements.

The library-first design ensures that organizations can deploy JDBX in complex multi-tenant scenarios while maintaining complete data isolation and security. The system actor model eliminates privileged backdoors while ensuring that all system operations remain auditable and controllable.

This architecture positions JDBX as a leading solution for modern document database requirements, supporting everything from simple single-tenant applications to complex multi-tenant SaaS platforms.