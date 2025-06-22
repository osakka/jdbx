# RBAC Setup Guide

A comprehensive guide to setting up and configuring Role-Based Access Control in JDBX.

## Table of Contents
1. [Quick Start](#quick-start)
2. [User Management](#user-management)
3. [Role Configuration](#role-configuration)
4. [Permission Assignment](#permission-assignment)
5. [API Token Setup](#api-token-setup)
6. [Testing Your Configuration](#testing-your-configuration)
7. [Common Scenarios](#common-scenarios)
8. [Troubleshooting](#troubleshooting)

## Quick Start

### Initial Admin Setup

1. **Start JDBX Server**
   ```bash
   cd /opt/jdbx && build/jdbx_runtime.sh start
   ```

2. **Access Admin Interface**
   Open your browser to `http://localhost:5000` and log in with the default admin credentials:
   - Username: `admin`
   - Password: `admin` (change immediately after first login)

3. **Navigate to RBAC**
   Click on the "RBAC" tab in the admin interface to access user and role management.

### First Steps Checklist
- [ ] Change default admin password
- [ ] Create your first custom role
- [ ] Add a regular user account
- [ ] Test permissions with the new user
- [ ] Set up API tokens if needed

## User Management

### Creating Users

**Via Admin Interface:**
1. Go to RBAC → Users
2. Click "Create New User"
3. Fill in the required fields:
   - Username (unique identifier)
   - Common Name (display name)
   - Email address
   - Password (will be hashed with bcrypt)
4. Assign roles
5. Click "Create User"

**Via API:**
```bash
curl -X POST http://localhost:5000/api/rbac/users \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "username": "john_doe",
    "cn": "John Doe",
    "email": "john@company.com",
    "password": "secure_password_123",
    "roles": ["user"]
  }'
```

### User Password Management

**Changing Passwords:**
```bash
curl -X PUT http://localhost:5000/api/rbac/users/USER_ID/password \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "current_password": "old_password",
    "new_password": "new_secure_password"
  }'
```

**Password Requirements:**
- Minimum 8 characters
- Stored using bcrypt with salt
- No password history enforcement (implement if needed)

## Role Configuration

### Understanding Roles

JDBX uses a hierarchical role system where each role defines a set of permissions. Roles can be assigned to users to grant specific access levels.

**Built-in Roles:**
- `admin` - Full system access
- `user` - Basic read/write access

### Creating Custom Roles

**Via Admin Interface:**
1. Go to RBAC → Roles
2. Click "Create New Role"
3. Define role properties:
   - Name (unique identifier)
   - Common Name (display name)
   - Description
4. Configure permissions (see Permission Assignment section)

**Via API:**
```bash
curl -X POST http://localhost:5000/api/rbac/roles \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "editor",
    "cn": "Content Editor",
    "description": "Can create and edit content but not manage users",
    "permissions": {
      "collections": {
        "posts": ["CREATE", "READ", "UPDATE"],
        "pages": ["CREATE", "READ", "UPDATE"]
      }
    }
  }'
```

### Role Hierarchy Example

```
admin (Full Access)
├── manager (Department Management)
│   ├── editor (Content Management)
│   └── reviewer (Content Review)
└── api_user (Programmatic Access)
    └── readonly (Read-only API Access)
```

## Permission Assignment

### Permission Types

JDBX supports five permission levels:
- `CREATE` - Can create new documents
- `READ` - Can read existing documents  
- `UPDATE` - Can modify existing documents
- `DELETE` - Can remove documents
- `ADMIN` - Full administrative access (includes all above)

### Collection-Level Permissions

**Granting Specific Collection Access:**
```json
{
  "permissions": {
    "collections": {
      "users": ["READ"],
      "posts": ["CREATE", "READ", "UPDATE"],
      "admin_logs": ["READ"]
    }
  }
}
```

**Wildcard Permissions:**
```json
{
  "permissions": {
    "collections": {
      "*": ["READ"],
      "public_*": ["CREATE", "READ", "UPDATE"]
    }
  }
}
```

### System-Level Permissions

**Full System Access:**
```json
{
  "permissions": {
    "system": ["*"],
    "collections": {
      "*": ["CREATE", "READ", "UPDATE", "DELETE", "ADMIN"]
    }
  }
}
```

## API Token Setup

### Creating API Tokens

API tokens provide long-lived access for programmatic use.

**Via Admin Interface:**
1. Go to RBAC → API Tokens
2. Click "Generate New Token"
3. Configure token properties:
   - Name/Description
   - Expiration date (optional)
   - Associated user account
4. Copy the generated token (shown only once)

**Via API:**
```bash
curl -X POST http://localhost:5000/api/rbac/tokens \
  -H "Authorization: Bearer YOUR_SESSION_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Production API Access",
    "expires_at": "2025-12-31T23:59:59Z",
    "user_id": "api_user_id"
  }'
```

### Using API Tokens

**In API Requests:**
```bash
curl -X GET http://localhost:5000/api/collections/posts \
  -H "Authorization: Bearer YOUR_API_TOKEN"
```

**In Applications:**
```javascript
const response = await fetch('http://localhost:5000/api/collections/posts', {
  headers: {
    'Authorization': 'Bearer YOUR_API_TOKEN',
    'Content-Type': 'application/json'
  }
});
```

## Testing Your Configuration

### Permission Testing Checklist

1. **Login Testing**
   - [ ] Admin user can log in
   - [ ] Regular users can log in
   - [ ] Invalid credentials are rejected

2. **Collection Access Testing**
   - [ ] Users can access permitted collections
   - [ ] Users are denied access to restricted collections
   - [ ] Permission levels work correctly (READ vs WRITE)

3. **API Token Testing**
   - [ ] API tokens authenticate successfully
   - [ ] Expired tokens are rejected
   - [ ] Revoked tokens are rejected

### Test Script Example

```bash
#!/bin/bash
# RBAC Configuration Test Script

BASE_URL="http://localhost:5000"
ADMIN_TOKEN="your_admin_token"

echo "Testing user creation..."
curl -X POST $BASE_URL/api/rbac/users \
  -H "Authorization: Bearer $ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "username": "test_user",
    "password": "test_password",
    "roles": ["user"]
  }'

echo "Testing user authentication..."
curl -X POST $BASE_URL/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{
    "username": "test_user",
    "password": "test_password"
  }'

echo "Testing permissions..."
# Add more test cases as needed
```

## Common Scenarios

### Scenario 1: Multi-Tenant Application

**Setup:**
- Create roles per tenant: `tenant_a_admin`, `tenant_a_user`
- Use collection naming: `tenant_a_posts`, `tenant_a_users`
- Configure permissions by tenant prefix

**Role Configuration:**
```json
{
  "name": "tenant_a_admin",
  "permissions": {
    "collections": {
      "tenant_a_*": ["CREATE", "READ", "UPDATE", "DELETE", "ADMIN"]
    }
  }
}
```

### Scenario 2: API-Only Application

**Setup:**
- Create `api_readonly` and `api_readwrite` roles
- Generate long-lived API tokens
- No session-based authentication needed

**Role Configuration:**
```json
{
  "name": "api_readwrite",
  "permissions": {
    "collections": {
      "api_data": ["CREATE", "READ", "UPDATE"],
      "api_logs": ["CREATE", "READ"]
    }
  }
}
```

### Scenario 3: Content Management System

**Setup:**
- Roles: `admin`, `editor`, `author`, `viewer`
- Collections: `posts`, `pages`, `media`, `comments`
- Hierarchical permissions

**Role Hierarchy:**
```
admin: Full system access
editor: Manage all content
author: Create/edit own content
viewer: Read-only access
```

## Troubleshooting

### Common Issues

**"Unauthorized" Errors**
1. Check token validity and expiration
2. Verify user has required permissions
3. Ensure collection names match exactly
4. Check for typos in permission configuration

**Users Can't Access Collections**
1. Verify role assignment to user
2. Check role permissions configuration
3. Ensure collection exists
4. Review permission inheritance

**API Tokens Not Working**
1. Check token expiration date
2. Verify token hasn't been revoked
3. Ensure associated user account is active
4. Check permission inheritance from user roles

### Debug Commands

**Check User Permissions:**
```bash
curl -X GET http://localhost:5000/api/rbac/users/USER_ID \
  -H "Authorization: Bearer YOUR_TOKEN"
```

**List Active Sessions:**
```bash
curl -X GET http://localhost:5000/api/rbac/sessions \
  -H "Authorization: Bearer YOUR_TOKEN"
```

**Validate Token:**
```bash
curl -X GET http://localhost:5000/api/auth/validate \
  -H "Authorization: Bearer YOUR_TOKEN"
```

### Log Analysis

**Check RBAC logs:**
```bash
grep "RBAC\|Auth" /opt/jdbx/build/var/jdbx.log | tail -50
```

**Permission denied logs:**
```bash
grep "Permission denied\|Unauthorized" /opt/jdbx/build/var/jdbx.log
```

## Best Practices

1. **Security**
   - Change default passwords immediately
   - Use strong passwords (consider password policies)
   - Regularly rotate API tokens
   - Monitor for suspicious activity

2. **Role Design**
   - Follow principle of least privilege
   - Create roles based on job functions
   - Use descriptive role names
   - Document role purposes

3. **Permission Management**
   - Be specific with collection permissions
   - Avoid overly broad wildcard permissions
   - Regularly audit user permissions
   - Remove unused roles and users

4. **Monitoring**
   - Enable audit logging
   - Monitor failed authentication attempts
   - Track permission changes
   - Set up alerts for security events

## Related Documentation

- [RBAC API Reference](../api/rbac-api.md)
- [RBAC Technical Reference](../reference/rbac.md)
- [Authentication Guide](authentication-guide.md)
- [Admin Interface Guide](admin-interface.md)