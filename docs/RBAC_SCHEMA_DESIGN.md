# RBAC Database Schema Design

## Overview
The RBAC system uses special collections in the main database to store users, roles, permissions, and sessions.

## Collections

### 1. _users Collection
Stores user accounts and authentication information.

```json
{
  "_id": "user-uuid",
  "username": "john_doe",
  "email": "john@example.com",
  "password_hash": "$pbkdf2$...",
  "roles": ["role-uuid-1", "role-uuid-2"],
  "created_at": "2025-01-23T12:00:00Z",
  "updated_at": "2025-01-23T12:00:00Z",
  "last_login": "2025-01-23T12:00:00Z",
  "active": true,
  "metadata": {
    "full_name": "John Doe",
    "department": "Engineering"
  }
}
```

### 2. _roles Collection
Defines roles and their base permissions.

```json
{
  "_id": "role-uuid",
  "name": "editor",
  "description": "Can edit content",
  "permissions": {
    "collections": {
      "articles": ["CREATE", "READ", "UPDATE"],
      "comments": ["READ"]
    },
    "system": ["READ_OWN_PROFILE", "UPDATE_OWN_PROFILE"]
  },
  "created_at": "2025-01-23T12:00:00Z",
  "updated_at": "2025-01-23T12:00:00Z"
}
```

### 3. _permissions Collection
Stores specific permission overrides for users on collections/documents.

```json
{
  "_id": "perm-uuid",
  "user_id": "user-uuid",
  "resource_type": "collection|document",
  "resource_id": "collection-name|document-id",
  "collection": "collection-name",
  "permissions": ["READ", "UPDATE"],
  "granted_by": "admin-user-uuid",
  "granted_at": "2025-01-23T12:00:00Z",
  "expires_at": null
}
```

### 4. _collections Collection
Metadata about collections including ownership.

```json
{
  "_id": "articles",
  "name": "articles",
  "owner_id": "user-uuid",
  "created_at": "2025-01-23T12:00:00Z",
  "updated_at": "2025-01-23T12:00:00Z",
  "schema": {},
  "indexes": [],
  "settings": {
    "public_read": false,
    "require_auth": true
  }
}
```

### 5. _sessions Collection
Active user sessions for tracking and revocation.

```json
{
  "_id": "session-uuid",
  "user_id": "user-uuid",
  "token_hash": "sha256-hash-of-token",
  "created_at": "2025-01-23T12:00:00Z",
  "expires_at": "2025-01-23T14:00:00Z",
  "last_activity": "2025-01-23T12:30:00Z",
  "ip_address": "192.168.1.1",
  "user_agent": "Mozilla/5.0...",
  "revoked": false
}
```

### 6. _permission_cache Collection
Cache for computed permissions with TTL.

```json
{
  "_id": "cache-key",
  "user_id": "user-uuid",
  "resource": "collection:articles",
  "permissions": ["CREATE", "READ", "UPDATE"],
  "computed_at": "2025-01-23T12:00:00Z",
  "expires_at": "2025-01-23T12:05:00Z"
}
```

## Permission Model

### Permission Types
- **CREATE**: Can create new documents/resources
- **READ**: Can read/list documents/resources
- **UPDATE**: Can modify existing documents/resources
- **DELETE**: Can delete documents/resources
- **ADMIN**: Can manage settings, permissions, schema

### Permission Inheritance
1. User has roles
2. Roles grant permissions on collections
3. Collection permissions cascade to documents
4. Document-specific permissions can override (add or restrict)
5. User-specific permissions can override role permissions

### Permission Resolution Algorithm
```
1. Get all roles for user
2. Combine all role permissions (additive)
3. Apply user-specific permission overrides
4. Apply document-specific permissions (if applicable)
5. Check collection permissions (if no document override)
6. Return final permission set
```

## Indexes

### Required Indexes for Performance
1. _users: username (unique), email (unique)
2. _roles: name (unique)
3. _permissions: user_id, resource_type, resource_id (compound)
4. _collections: name (unique), owner_id
5. _sessions: token_hash (unique), user_id, expires_at
6. _permission_cache: user_id, resource, expires_at

## Default Data

### System Roles
1. **admin**: Full system access
2. **user**: Basic authenticated user (no permissions by default)
3. **guest**: Unauthenticated access (read-only public data)

### System User
- Username: admin
- Password: (set on first run)
- Roles: [admin]