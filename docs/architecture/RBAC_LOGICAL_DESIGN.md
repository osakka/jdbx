# JSONdb RBAC Logical Design

## Core Principles

1. **Single Source of Truth**: Each relationship should be stored in one place only
2. **Efficient Queries**: Design should optimize for common access patterns
3. **Scalability**: Support millions of users and complex permission hierarchies
4. **Flexibility**: Support both simple and complex permission models

## Design Decision: Where to Store Relationships

### Option A: Roles in User Documents (Current Approach)
```json
// User document
{
  "uuid": "user-123",
  "username": "alice",
  "roles": ["role-admin", "role-editor"]
}
```

**Pros:**
- Fast permission checks (single user lookup)
- User-centric queries are efficient
- Simple to understand

**Cons:**
- Finding all users with a role requires full scan
- Role deletion requires updating all users
- Potential inconsistency if role doesn't exist

### Option B: Users in Role Documents
```json
// Role document
{
  "uuid": "role-admin",
  "name": "admin",
  "users": ["user-123", "user-456"]
}
```

**Pros:**
- Easy to find all users with a role
- Role-centric management

**Cons:**
- Permission checks require multiple lookups
- Large roles could have huge user arrays
- User deletion requires updating all roles

### Option C: Separate Membership Collection (Recommended)
```json
// Membership document
{
  "uuid": "membership-789",
  "user_id": "user-123",
  "role_id": "role-admin",
  "granted_by": "user-000",
  "granted_at": "2025-06-09T10:00:00Z",
  "expires_at": null,
  "conditions": {
    "ip_whitelist": ["192.168.1.0/24"],
    "time_restrictions": "business_hours"
  }
}
```

**Pros:**
- Single source of truth
- Supports metadata (who granted, when, conditions)
- Efficient indexes on both user_id and role_id
- Supports temporary permissions
- Audit trail built-in
- No document size limits

**Cons:**
- Requires join-like operations
- Slightly more complex

## Recommended Architecture

### 1. Collections in System Library

```
system/
├── users           # User accounts
├── roles           # Role definitions  
├── memberships     # User-role associations
├── permissions     # Permission definitions
├── libraries       # Library metadata
└── collections     # Collection metadata
```

### 2. User Document Structure
```json
{
  "uuid": "550e8400-e29b-41d4-a716-446655440000",
  "username": "alice",
  "email": "alice@example.com",
  "password_hash": "...",
  "profile": {
    "full_name": "Alice Smith",
    "avatar_url": "..."
  },
  "status": "active",
  "created_at": "2025-06-09T10:00:00Z",
  "updated_at": "2025-06-09T10:00:00Z",
  "last_login": "2025-06-09T15:00:00Z"
}
```

### 3. Role Document Structure
```json
{
  "uuid": "660e8400-e29b-41d4-a716-446655440001",
  "name": "editor",
  "display_name": "Content Editor",
  "description": "Can create and edit content",
  "permissions": {
    "content/*": ["read", "write", "execute"],
    "media/*": ["read", "write"],
    "users/profile": ["read"]
  },
  "parent_roles": ["viewer"],  // Role inheritance
  "constraints": {
    "max_documents": 1000,
    "allowed_collections": ["posts", "pages"]
  },
  "created_at": "2025-06-09T10:00:00Z"
}
```

### 4. Membership Document Structure
```json
{
  "uuid": "770e8400-e29b-41d4-a716-446655440002",
  "user_id": "550e8400-e29b-41d4-a716-446655440000",
  "role_id": "660e8400-e29b-41d4-a716-446655440001",
  "granted_by": "000e8400-e29b-41d4-a716-446655440000",
  "granted_at": "2025-06-09T10:00:00Z",
  "expires_at": "2025-12-31T23:59:59Z",
  "conditions": {
    "ip_whitelist": ["10.0.0.0/8"],
    "require_2fa": true
  },
  "active": true
}
```

### 5. Permission Checking Flow

```
1. User attempts action on resource
2. System queries memberships: WHERE user_id = ? AND active = true
3. For each membership:
   - Load role document
   - Check role permissions
   - Apply role inheritance
   - Check conditions
4. Aggregate permissions (OR logic)
5. Check document-level permissions (AND logic)
6. Allow or deny action
```

### 6. Index Strategy

```sql
// Memberships collection indexes
- user_id (hash index) - for permission checks
- role_id (hash index) - for role member listing
- (user_id, role_id) unique - prevent duplicates
- expires_at (btree) - for cleanup
- active (bitmap) - for filtering

// Users collection indexes  
- username (unique hash)
- email (unique hash, case-insensitive)

// Roles collection indexes
- name (unique hash)
```

## Common Operations

### 1. Check User Permissions
```javascript
// Efficient: Single query to memberships, then parallel role lookups
function getUserPermissions(userId) {
  const memberships = db.query('memberships', { user_id: userId, active: true });
  const roles = db.batchGet('roles', memberships.map(m => m.role_id));
  return aggregatePermissions(roles);
}
```

### 2. Grant Role to User
```javascript
function grantRole(userId, roleId, grantedBy, conditions = {}) {
  // Check if membership already exists
  const existing = db.query('memberships', { 
    user_id: userId, 
    role_id: roleId 
  });
  
  if (existing.length > 0) {
    throw new Error('User already has this role');
  }
  
  // Create membership
  return db.insert('memberships', {
    user_id: userId,
    role_id: roleId,
    granted_by: grantedBy,
    granted_at: new Date().toISOString(),
    conditions: conditions,
    active: true
  });
}
```

### 3. List Users with Role
```javascript
function getUsersWithRole(roleId) {
  const memberships = db.query('memberships', { 
    role_id: roleId, 
    active: true 
  });
  return db.batchGet('users', memberships.map(m => m.user_id));
}
```

### 4. Remove User from Role
```javascript
function removeUserFromRole(userId, roleId) {
  // Soft delete for audit trail
  return db.update('memberships', 
    { user_id: userId, role_id: roleId },
    { active: false, deactivated_at: new Date().toISOString() }
  );
}
```

## Migration Plan

1. Create new collections in system library
2. Migrate existing RBAC data:
   - _users → system/users
   - _roles → system/roles  
   - Create memberships from user.roles arrays
3. Update RBAC code to use new structure
4. Remove old collections

## Benefits of This Design

1. **Scalability**: No large arrays in documents
2. **Flexibility**: Supports complex permissions with conditions
3. **Auditability**: Full history of who granted what and when
4. **Performance**: Optimized indexes for common queries
5. **Consistency**: Single source of truth for relationships
6. **Extensibility**: Easy to add new features (delegated permissions, approval workflows)

## Document-Level Permissions

In addition to role-based permissions, documents can have their own permission metadata:

```json
{
  "uuid": "doc-123",
  "title": "Private Document",
  "_permissions": {
    "owners": ["user-123", "user-456"],
    "readers": ["role-viewer"],
    "writers": ["user-789"],
    "inheritance": "none"  // or "parent", "library"
  }
}
```

This allows fine-grained control at the document level while maintaining the role-based system for broader permissions.