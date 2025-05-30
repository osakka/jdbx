# JSONdb RBAC Reference Documentation

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Database Schema](#database-schema)
4. [User Management](#user-management)
5. [Role Management](#role-management)
6. [Permission System](#permission-system)
7. [Session Management](#session-management)
8. [JWT Token System](#jwt-token-system)
9. [API Token Management](#api-token-management)
10. [API Reference](#api-reference)
11. [Implementation Details](#implementation-details)
12. [Security Best Practices](#security-best-practices)
13. [Troubleshooting](#troubleshooting)

## Overview

JSONdb implements a comprehensive Role-Based Access Control (RBAC) system that provides fine-grained access control for database operations. The system is built on industry-standard JWT tokens and supports both session-based authentication and long-lived API tokens.

### Key Features
- User authentication with bcrypt password hashing
- Role-based permission management
- JWT-based session tokens (30-minute expiry)
- Long-lived API tokens for programmatic access
- Fine-grained permissions (CREATE, READ, UPDATE, DELETE, ADMIN)
- Collection-level access control
- Session tracking and management with sliding timeouts
- Audit logging for security events
- Database-backed storage (no file-based configuration)

### Current Implementation Status
The RBAC system has been fully implemented as of v2.0.6 with the following capabilities:
- ✅ Database-backed user storage in `_users` collection
- ✅ User registration and authentication with JWT tokens
- ✅ Password hashing and verification
- ✅ All RBAC collections created
- ✅ Binary persistence for all RBAC data
- ✅ Complete role management API (CRUD operations)
- ✅ UUID support for consistent API behavior
- ✅ Parameterized route matching
- ✅ Secure JWT signature verification
- ✅ Permission system with numeric keys

## Architecture

### Component Structure
```
RBAC System
├── Authentication Layer
│   ├── Password validation (bcrypt)
│   ├── JWT token generation
│   └── Token verification
├── Authorization Layer
│   ├── Role assignment
│   ├── Permission checking
│   └── Collection access control
├── Session Management
│   ├── Active session tracking
│   ├── Session expiration
│   ├── Sliding session timeouts
│   └── Concurrent session limits
└── API Token Management
    ├── Long-lived token generation
    ├── Token revocation
    └── Usage tracking
```

### Database Collections
- `_users`: User accounts and authentication
- `_roles`: Role definitions and permissions
- `_sessions`: Active user sessions
- `_api_tokens`: Long-lived API tokens
- `_permission_cache`: Cached permission evaluations

### Implementation Files
- `/src/components/rbac/rbac.c` - Core RBAC functionality
- `/src/components/rbac/rbac_database.c` - Database operations
- `/src/components/rbac/rbac_db.c` - Database-specific RBAC functions
- `/src/components/rbac/rbac_enhanced.c` - Enhanced RBAC features
- `/src/components/rbac/jwt.c` - JWT token handling
- `/src/components/api/rbac_api.c` - RBAC API endpoints
- `/src/include/rbac/*.h` - Header files

## Database Schema

### User Schema
```json
{
  "_id": "doc-<timestamp>-<random>",
  "username": "string",
  "password_hash": "bcrypt hash",
  "roles": ["role_id1", "role_id2"],
  "created_at": "ISO 8601 timestamp",
  "updated_at": "ISO 8601 timestamp",
  "last_login": "ISO 8601 timestamp",
  "active": true
}
```

### Role Schema
```json
{
  "_id": "doc-<timestamp>-<random>",
  "uuid": "uuid-v4-string",
  "name": "string",
  "description": "string",
  "permissions": {
    "collections": {
      "*": ["CREATE", "READ", "UPDATE", "DELETE"],
      "specific_collection": ["READ"]
    },
    "system": ["*"]
  },
  "created_at": "ISO 8601 timestamp",
  "updated_at": "ISO 8601 timestamp"
}
```

### Session Schema
```json
{
  "_id": "doc-<timestamp>-<random>",
  "user_id": "doc-xxx",
  "username": "string",
  "token": "JWT token",
  "created_at": "ISO 8601 timestamp",
  "last_seen": "ISO 8601 timestamp",
  "expires_at": "ISO 8601 timestamp",
  "active": true,
  "ip_address": "string",
  "user_agent": "string"
}
```

### API Token Schema (Planned)
```json
{
  "_id": "doc-<timestamp>-<random>",
  "token_hash": "bcrypt hash",
  "token_prefix": "jdb_pat_xxx",
  "user_id": "doc-xxx",
  "name": "string",
  "created_at": "ISO 8601 timestamp",
  "expires_at": "ISO 8601 timestamp",
  "last_used": "ISO 8601 timestamp",
  "scopes": ["read", "write"],
  "active": true
}
```

## User Management

### User Operations

#### Create User
```bash
curl -X POST http://localhost:5000/api/rbac/users \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "username": "newuser",
    "password": "SecurePass123!",
    "roles": ["doc-xxx-role"]
  }'
```

#### Update User
```bash
curl -X PUT http://localhost:5000/api/rbac/users/doc-xxx \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "roles": ["doc-xxx-role1", "doc-xxx-role2"]
  }'
```

#### Delete User
```bash
curl -X DELETE http://localhost:5000/api/rbac/users/doc-xxx \
  -H "Authorization: Bearer $TOKEN"
```

#### List Users
```bash
curl http://localhost:5000/api/rbac/users \
  -H "Authorization: Bearer $TOKEN"
```

## Role Management

### Built-in Roles

#### Admin Role
- Full system access
- All permissions on all collections
- User and role management
- System configuration

#### Read-Only Role
- READ permission on all collections
- No write permissions
- Cannot modify system settings

### Role Operations

#### Create Role
```bash
curl -X POST http://localhost:5000/api/rbac/roles \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "editor",
    "description": "Content editor role",
    "permissions": {
      "collections": {
        "articles": ["CREATE", "READ", "UPDATE"],
        "comments": ["READ", "UPDATE"]
      }
    }
  }'
```

#### Update Role
```bash
curl -X PUT http://localhost:5000/api/rbac/roles/doc-xxx \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "super_editor",
    "permissions": {
      "collections": {
        "articles": ["CREATE", "READ", "UPDATE", "DELETE"]
      }
    }
  }'
```

#### Delete Role
```bash
curl -X DELETE http://localhost:5000/api/rbac/roles/doc-xxx \
  -H "Authorization: Bearer $TOKEN"
```

## Permission System

### Permission Types
- **CREATE**: Create new documents
- **READ**: Read documents
- **UPDATE**: Modify existing documents
- **DELETE**: Delete documents
- **ADMIN**: Administrative operations (user/role management)

### Numeric Permission System
Permissions use a numeric key format with bitwise operations:

```json
{
    "permissions": {
        "3:*": 15,     // All permissions on all users
        "1:*": 7       // Read, write, delete on all documents
    }
}
```

Resource Types:
- 0 = Collection
- 1 = Document
- 2 = Index
- 3 = User
- 4 = Role
- 5 = System

Permission Values (bitwise):
- 1 = Read
- 2 = Write
- 4 = Delete
- 8 = Admin
- 15 = All (1+2+4+8)

### Permission Evaluation
1. Check user's roles
2. Aggregate permissions from all roles
3. Check collection-specific permissions
4. Apply wildcard permissions
5. Cache result for performance

### Permission Matrix API
```bash
curl http://localhost:5000/api/rbac/permissions \
  -H "Authorization: Bearer $TOKEN"
```

## Session Management

Sessions now support sliding timeouts - each request extends the session expiration time by 30 minutes.

### Session Operations

#### Login (Create Session)
```bash
curl -X POST http://localhost:5000/api/login \
  -H "Content-Type: application/json" \
  -d '{
    "username": "admin",
    "password": "password"
  }'
```

Response:
```json
{
  "token": "eyJhbGciOiJIUzI1NiIs...",
  "user": {
    "_id": "doc-xxx",
    "username": "admin",
    "roles": ["doc-xxx"]
  },
  "expires_at": "2025-05-26T10:22:14Z"
}
```

#### Logout (End Session)
```bash
curl -X POST http://localhost:5000/api/logout \
  -H "Authorization: Bearer $TOKEN"
```

#### List Active Sessions
```bash
curl http://localhost:5000/api/rbac/sessions \
  -H "Authorization: Bearer $TOKEN"
```

#### Terminate Specific Session
```bash
curl -X DELETE http://localhost:5000/api/sessions/doc-xxx \
  -H "Authorization: Bearer $TOKEN"
```

## JWT Token System

### Token Structure
```json
{
  "iss": "jsondb",
  "sub": "user_id",
  "exp": 1748254934,
  "iat": 1748253134,
  "username": "admin",
  "type": "access"
}
```

### Token Configuration
- Algorithm: HS256
- Expiry: 30 minutes (sliding timeout)
- Secret: Loaded from environment or config
- Signature: HMAC-SHA256 with timing-safe comparison

### Token Validation
1. Verify signature with timing-safe comparison
2. Check expiration
3. Validate issuer
4. Lookup session
5. Check session active status
6. Update session expiration (sliding timeout)

## API Token Management

### Long-Lived Tokens (Planned Feature)
For applications that need programmatic access without frequent re-authentication:

#### Generate API Token
```bash
curl -X POST http://localhost:5000/api/rbac/tokens \
  -H "Authorization: Bearer $SESSION_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "CI/CD Pipeline",
    "expires_in_days": 365,
    "scopes": ["read", "write"]
  }'
```

#### List API Tokens
```bash
curl http://localhost:5000/api/rbac/tokens \
  -H "Authorization: Bearer $TOKEN"
```

#### Revoke API Token
```bash
curl -X DELETE http://localhost:5000/api/rbac/tokens/jdb_pat_xxxxxxxxxxxxx \
  -H "Authorization: Bearer $TOKEN"
```

## API Reference

### Authentication Endpoints

| Method | Endpoint | Description | Auth Required |
|--------|----------|-------------|---------------|
| POST | `/api/login` | User login | No |
| POST | `/api/logout` | User logout | Yes |
| POST | `/api/refresh` | Refresh token | Yes |
| GET | `/api/verify` | Verify token | Yes |

### User Management Endpoints

| Method | Endpoint | Description | Auth Required |
|--------|----------|-------------|---------------|
| GET | `/api/rbac/users` | List all users | Yes (Admin) |
| GET | `/api/rbac/users/:id` | Get user details | Yes (Admin) |
| POST | `/api/rbac/users` | Create new user | Yes (Admin) |
| PUT | `/api/rbac/users/:id` | Update user | Yes (Admin) |
| DELETE | `/api/rbac/users/:id` | Delete user | Yes (Admin) |
| POST | `/api/rbac/users/:id/password` | Change password | Yes (Self/Admin) |

### Role Management Endpoints

| Method | Endpoint | Description | Auth Required |
|--------|----------|-------------|---------------|
| GET | `/api/rbac/roles` | List all roles | Yes |
| GET | `/api/rbac/roles/:id` | Get role details | Yes |
| POST | `/api/rbac/roles` | Create new role | Yes (Admin) |
| PUT | `/api/rbac/roles/:id` | Update role | Yes (Admin) |
| DELETE | `/api/rbac/roles/:id` | Delete role | Yes (Admin) |

### Permission Endpoints

| Method | Endpoint | Description | Auth Required |
|--------|----------|-------------|---------------|
| GET | `/api/rbac/permissions` | Get permission matrix | Yes |
| GET | `/api/rbac/permissions/check` | Check specific permission | Yes |
| POST | `/api/rbac/permissions/cache/clear` | Clear permission cache | Yes (Admin) |

### Session Management Endpoints

| Method | Endpoint | Description | Auth Required |
|--------|----------|-------------|---------------|
| GET | `/api/rbac/sessions` | List active sessions | Yes (Admin) |
| GET | `/api/rbac/sessions/mine` | List my sessions | Yes |
| GET | `/api/sessions` | List all sessions with details | Yes (Admin) |
| DELETE | `/api/sessions/:id` | Terminate specific session | Yes (Admin) |
| DELETE | `/api/rbac/sessions/:id` | Revoke session | Yes (Admin) |
| DELETE | `/api/rbac/sessions/all` | Revoke all sessions | Yes (Admin) |

### API Token Endpoints (Planned)

| Method | Endpoint | Description | Auth Required |
|--------|----------|-------------|---------------|
| GET | `/api/rbac/tokens` | List API tokens | Yes |
| POST | `/api/rbac/tokens` | Generate new token | Yes |
| DELETE | `/api/rbac/tokens/:token` | Revoke token | Yes |
| GET | `/api/rbac/tokens/:token/usage` | Get token usage stats | Yes |

## Implementation Details

### Database Integration
- All RBAC data stored in database collections (no file-based storage)
- Full CRUD operations with proper error handling
- UUID lookup layer for ID compatibility
- Thread-safe operations with database locking
- Binary persistence for all RBAC data

### Route Matching
API routes support parameterized paths:
```c
/* Enhanced route matching in api.c */
if (*route_ptr == ':') {
    /* Skip parameter name and actual value */
    while (*route_ptr && *route_ptr != '/') route_ptr++;
    while (*path_ptr && *path_ptr != '/') path_ptr++;
}
```

### Security Features
- Passwords stored as bcrypt hashes
- JWT tokens with HMAC-SHA256 signatures
- Timing-safe signature comparison
- Session tracking with IP and user agent
- Automatic session expiration
- Permission caching for performance

## Security Best Practices

### Password Requirements
- Minimum 8 characters
- Mix of uppercase, lowercase, numbers, special characters
- No common passwords
- Regular rotation policy

### Token Security
- Store tokens securely (never in logs)
- Use HTTPS in production
- Implement token rotation
- Monitor for suspicious activity
- Use timing-safe comparison for signatures

### Session Security
- Limit concurrent sessions
- Implement idle timeout
- Track session locations
- Alert on suspicious logins
- Use sliding session timeouts

### API Token Best Practices
- Use descriptive names
- Set appropriate expiration
- Rotate regularly
- Monitor usage patterns
- Revoke unused tokens

### Audit Logging
- Log all authentication attempts
- Track permission changes
- Monitor role assignments
- Record session activities

### Implementation Checklist
- [ ] Enable HTTPS in production
- [ ] Configure strong JWT secret
- [ ] Set appropriate token expiry
- [ ] Implement rate limiting
- [ ] Enable audit logging
- [ ] Regular security reviews
- [ ] Monitor failed login attempts
- [ ] Implement account lockout
- [ ] Regular permission audits
- [ ] Token usage monitoring

## Troubleshooting

### Common Issues

#### "Unauthorized" Errors
- Check token expiration
- Verify token in request header
- Confirm session is active
- Check user permissions

#### Session Not Created
- Verify user credentials
- Check password hash
- Confirm user is active
- Review session limits

#### Permission Denied
- Check user roles
- Verify role permissions
- Clear permission cache
- Review collection access

### Debug Commands
```bash
# Check current user
curl http://localhost:5000/api/verify \
  -H "Authorization: Bearer $TOKEN"

# Check permissions
curl http://localhost:5000/api/rbac/permissions/check?collection=users&action=READ \
  -H "Authorization: Bearer $TOKEN"

# View active sessions
curl http://localhost:5000/api/rbac/sessions \
  -H "Authorization: Bearer $TOKEN"
```

## Migration Notes

The RBAC system has fully transitioned to database-based storage with no file-based fallback:

1. All RBAC data is stored in database collections
2. File-based RBAC configuration is completely ignored
3. No automatic migration path from file-based to database-based RBAC
4. Manual setup required when upgrading from file-based systems

For existing installations:
- Create users with the same usernames and new passwords
- Create roles with appropriate permissions
- Assign users to roles through the API

## Future Enhancements

While the core RBAC system is complete, potential future work includes:
- API token management implementation
- Enhanced audit logging
- Bulk user/role import endpoints
- Permission inheritance mechanisms
- Group-based permissions
- Time-based access control
- Geographic access restrictions

## Conclusion

The JSONdb RBAC system provides comprehensive security features for controlling access to your database. By properly configuring users, roles, and permissions, you can ensure that your data is protected while maintaining flexibility for different access patterns. The system follows security best practices with bcrypt password hashing, JWT tokens with secure signatures, and fine-grained permission control.

For additional support or feature requests, please refer to the JSONdb documentation or submit an issue on the project repository.