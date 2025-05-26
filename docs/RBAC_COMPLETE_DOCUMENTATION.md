# JSONdb RBAC Complete Documentation

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [User Management](#user-management)
4. [Role Management](#role-management)
5. [Permission System](#permission-system)
6. [Session Management](#session-management)
7. [JWT Token System](#jwt-token-system)
8. [API Token Management](#api-token-management)
9. [API Reference](#api-reference)
10. [Security Best Practices](#security-best-practices)

## Overview

JSONdb implements a comprehensive Role-Based Access Control (RBAC) system that provides fine-grained access control for database operations. The system is built on industry-standard JWT tokens and supports both session-based authentication and long-lived API tokens.

### Key Features
- User authentication with bcrypt password hashing
- Role-based permission management
- JWT-based session tokens (30-minute expiry)
- Long-lived API tokens for programmatic access
- Fine-grained permissions (CREATE, READ, UPDATE, DELETE, ADMIN)
- Collection-level access control
- Session tracking and management
- Audit logging for security events

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

## User Management

### User Schema
```json
{
  "_id": "doc-xxx",
  "username": "string",
  "password_hash": "bcrypt hash",
  "roles": ["role_id1", "role_id2"],
  "created_at": "ISO 8601 timestamp",
  "updated_at": "ISO 8601 timestamp",
  "last_login": "ISO 8601 timestamp",
  "active": true
}
```

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

### Role Schema
```json
{
  "_id": "doc-xxx",
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
    "permissions": {
      "collections": {
        "articles": ["CREATE", "READ", "UPDATE", "DELETE"]
      }
    }
  }'
```

## Permission System

### Permission Types
- **CREATE**: Create new documents
- **READ**: Read documents
- **UPDATE**: Modify existing documents
- **DELETE**: Delete documents
- **ADMIN**: Administrative operations (user/role management)

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

Response:
```json
{
  "roles": [
    {
      "_id": "doc-xxx",
      "name": "admin",
      "permissions": {
        "collections": {
          "*": ["CREATE", "READ", "UPDATE", "DELETE", "ADMIN"]
        }
      }
    }
  ]
}
```

## Session Management

### Session Schema
```json
{
  "_id": "doc-xxx",
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

#### Revoke Session
```bash
curl -X DELETE http://localhost:5000/api/rbac/sessions/doc-xxx \
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
- Expiry: 30 minutes (configurable)
- Secret: Loaded from environment or config
- Refresh: Not implemented (use new login)

### Token Validation
1. Verify signature
2. Check expiration
3. Validate issuer
4. Lookup session
5. Check session active status

## API Token Management

### Long-Lived Tokens
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

Response:
```json
{
  "token": "jdb_pat_xxxxxxxxxxxxx",
  "expires_at": "2026-05-26T10:00:00Z",
  "scopes": ["read", "write"]
}
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

### API Token Schema
```json
{
  "_id": "doc-xxx",
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
| DELETE | `/api/rbac/sessions/:id` | Revoke session | Yes (Admin) |
| DELETE | `/api/rbac/sessions/all` | Revoke all sessions | Yes (Admin) |

### API Token Endpoints

| Method | Endpoint | Description | Auth Required |
|--------|----------|-------------|---------------|
| GET | `/api/rbac/tokens` | List API tokens | Yes |
| POST | `/api/rbac/tokens` | Generate new token | Yes |
| DELETE | `/api/rbac/tokens/:token` | Revoke token | Yes |
| GET | `/api/rbac/tokens/:token/usage` | Get token usage stats | Yes |

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

### Session Security
- Limit concurrent sessions
- Implement idle timeout
- Track session locations
- Alert on suspicious logins

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

## Migration Guide

### From Basic Auth to RBAC
1. Create admin user
2. Define roles based on access patterns
3. Assign roles to users
4. Update application to use JWT tokens
5. Test permission boundaries
6. Enable audit logging
7. Monitor and adjust

### Upgrading Permissions
1. Review current access patterns
2. Define new permission requirements
3. Create/update roles
4. Test with limited users
5. Roll out to all users
6. Monitor for issues

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

## Conclusion

The JSONdb RBAC system provides comprehensive security features for controlling access to your database. By properly configuring users, roles, and permissions, you can ensure that your data is protected while maintaining flexibility for different access patterns.

For additional support or feature requests, please refer to the JSONdb documentation or submit an issue on the project repository.