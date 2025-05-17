# Database-Based RBAC System

This document describes the new database-based RBAC (Role-Based Access Control) system in JSONdb.

## Overview

The RBAC system has been migrated from a file-based storage approach to a fully database-based implementation. This means that all user accounts, roles, and permissions are now stored directly in the database as documents in dedicated collections.

## Benefits

- **Self-contained configuration**: All configuration is now stored within the database itself
- **Real-time updates**: Changes to RBAC take effect immediately without file I/O
- **API-driven management**: RBAC can be managed via REST API endpoints
- **Improved consistency**: Single source of truth for all RBAC data
- **Simplified operations**: No need to manage separate configuration files

## Implementation

### Collections

The RBAC system uses the following collections in the database:

- `_system`: System-wide configuration
- `_users`: User accounts with authentication information
- `_roles`: Role definitions with permissions

### Collection Structure

#### _users Collection

```json
{
  "id": "user123",
  "username": "admin",
  "password_hash": "hashed_password_string",
  "roles": ["role1", "role2"]
}
```

#### _roles Collection

```json
{
  "id": "role123",
  "name": "Administrator",
  "permissions": {
    "DATABASE:*": ["ADMIN"],
    "COLLECTION:users": ["READ", "WRITE"],
    "USER:*": ["READ"]
  }
}
```

### Indexes

For performance, the following indexes are created:

- `idx_user_id` on `_users.id` (unique)
- `idx_username` on `_users.username` (unique)
- `idx_role_id` on `_roles.id` (unique)
- `idx_role_name` on `_roles.name` (unique)

### API Endpoints

The RBAC system can be managed through the following REST API endpoints:

#### User Management

- `GET /api/rbac/users` - List all users
- `GET /api/rbac/users/:id` - Get user details
- `POST /api/rbac/users` - Create new user
- `PUT /api/rbac/users/:id` - Update user
- `DELETE /api/rbac/users/:id` - Delete user

#### Role Management

- `GET /api/rbac/roles` - List all roles
- `GET /api/rbac/roles/:id` - Get role details
- `POST /api/rbac/roles` - Create new role
- `DELETE /api/rbac/roles/:id` - Delete role

#### Role-User Management

- `POST /api/rbac/roles/:id/users/:user_id` - Add user to role
- `DELETE /api/rbac/roles/:id/users/:user_id` - Remove user from role

#### Permission Management

- `POST /api/rbac/roles/:id/permissions` - Grant permission to role
- `DELETE /api/rbac/roles/:id/permissions` - Revoke permission from role

For detailed API documentation, see [RBAC API Reference](/docs/api/RBAC_API.md).

## Resource Types and Permissions

### Resource Types

The RBAC system supports the following resource types:

- `DATABASE` - Database-level permissions
- `COLLECTION` - Collection-level permissions
- `DOCUMENT` - Document-level permissions
- `USER` - User account permissions
- `ROLE` - Role definition permissions
- `PERMISSION` - Permission management permissions

### Permission Types

The following permission types are supported:

- `READ` - Permission to read/view resources
- `WRITE` - Permission to modify resources
- `DELETE` - Permission to delete resources
- `ADMIN` - Full administrative access to resources

## Migration

As of the latest update, file-based RBAC has been fully deprecated with no fallback mechanism. The system now exclusively uses database-based RBAC with no file-based alternatives.

The startup process includes:

1. Check if the `_users` and `_roles` collections exist in the database
2. If they do not exist, create them and build necessary indices
3. Initialize the RBAC system from the database
4. If no RBAC data exists in the database, create a new default RBAC system

> **Important**: Any existing file-based RBAC configuration will be ignored. If you're upgrading from a previous version that used file-based RBAC, you'll need to manually set up your users and roles through the RBAC API.

## Security Features

The database-based RBAC system includes several security features:

1. Passwords are never stored in plain text, only as hashes
2. Permission checks are enforced consistently across all API endpoints
3. Only administrators can create or delete user accounts
4. Users can only manage their own passwords
5. JWT tokens are used for authentication

## Source Code

- `/src/components/rbac/rbac_db.c`: Implementation of database RBAC functions
- `/src/include/rbac/rbac_db.h`: Header for database RBAC API
- `/src/components/rbac/rbac_enhanced.c`: Entry points for database-only RBAC
- `/src/components/api/rbac_api.c`: API endpoints for RBAC management

## Design Principles

1. **Database as single source of truth**: All RBAC configuration is stored in the database.
2. **No fallbacks**: The system does not fall back to file-based storage.
3. **Clean API**: Clear separation between database operations and RBAC logic.
4. **Complete cutover**: Full removal of file-based RBAC with no migration path.
5. **Permission granularity**: Fine-grained control over resources.
6. **Role-based approach**: Permissions are granted to roles, not directly to users.

## Best Practices

When using the database-based RBAC system:

1. Create a dedicated administrator role with full permissions
2. Assign users to roles rather than granting permissions directly
3. Follow the principle of least privilege
4. Regularly audit user access and permissions
5. Use wildcard resource IDs (`*`) sparingly

## API Implementation Details

The RBAC API handlers:
- Validate user authentication using JWT tokens
- Check user permissions for each operation
- Extract parameters from request URLs and bodies
- Perform database operations
- Format and return appropriate responses
- Handle error conditions consistently

All API endpoints return standardized responses with appropriate HTTP status codes and JSON-formatted bodies.

## Conclusion

The database-based RBAC system provides a robust, secure, and flexible way to manage access control in JSONdb. By storing all RBAC data in the database, it ensures that permissions are always up-to-date and can be managed through a consistent API interface.