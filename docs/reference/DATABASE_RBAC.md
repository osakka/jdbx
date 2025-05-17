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

### Indexes

For performance, the following indexes are created:

- `idx_user_id` on `_users.id` (unique)
- `idx_username` on `_users.username` (unique)
- `idx_role_id` on `_roles.id` (unique)
- `idx_role_name` on `_roles.name` (unique)

### API Endpoints

The RBAC system can be managed through the following REST API endpoints:

#### Users

- `GET /api/rbac/users` - List all users
- `GET /api/rbac/users/:id` - Get user details
- `POST /api/rbac/users` - Create new user
- `PUT /api/rbac/users/:id` - Update user
- `DELETE /api/rbac/users/:id` - Delete user

#### Roles

- `GET /api/rbac/roles` - List all roles
- `GET /api/rbac/roles/:id` - Get role details
- `POST /api/rbac/roles` - Create new role
- `PUT /api/rbac/roles/:id` - Update role
- `DELETE /api/rbac/roles/:id` - Delete role

#### Permissions

- `POST /api/rbac/roles/:id/users` - Add user to role
- `DELETE /api/rbac/roles/:id/users/:user_id` - Remove user from role
- `POST /api/rbac/roles/:id/permissions` - Grant permission to role
- `DELETE /api/rbac/roles/:id/permissions` - Revoke permission from role

## Migration

During the first startup with the new system, any existing RBAC configuration from files will be automatically migrated to the database. After migration, the file-based configuration will no longer be used.

## Source Code

- `/src/components/rbac/rbac_db.c`: Implementation of database RBAC functions
- `/src/include/rbac/rbac_db.h`: Header for database RBAC API
- `/src/components/rbac/rbac_enhanced.c`: Entry points for database-only RBAC
- `/src/components/api/rbac_api.c`: API endpoints for RBAC management

## Design Principles

1. **Database as single source of truth**: All RBAC configuration is stored in the database.
2. **No fallbacks**: The system does not fall back to file-based storage.
3. **Clean API**: Clear separation between database operations and RBAC logic.
4. **Seamless transition**: Automatic migration from file-based to database-based.