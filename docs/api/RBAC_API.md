# RBAC API Reference

This document provides reference information for the RBAC API endpoints in JSON Database.

## Authentication

All RBAC API endpoints require authentication using a JWT token. The token should be included in the `Authorization` header of the request with the `Bearer` prefix.

```
Authorization: Bearer <token>
```

## User Management

### Get All Users

**Endpoint**: `GET /api/rbac/users`

**Permissions Required**: `ADMIN` permission on the `USER` resource type with `*` resource ID.

**Response Example**:

```json
[
  {
    "id": "user1",
    "username": "admin",
    "roles": ["role1", "role2"]
  },
  {
    "id": "user2",
    "username": "user",
    "roles": ["role3"]
  }
]
```

### Get User

**Endpoint**: `GET /api/rbac/users/:id`

**Permissions Required**: Either:
- `READ` permission on the `USER` resource type with the user's ID as the resource ID, or
- `ADMIN` permission on the `USER` resource type with `*` resource ID, or
- Be the user identified by the `:id` parameter.

**Response Example**:

```json
{
  "id": "user1",
  "username": "admin",
  "roles": ["role1", "role2"]
}
```

### Create User

**Endpoint**: `POST /api/rbac/users`

**Permissions Required**: `ADMIN` permission on the `USER` resource type with `*` resource ID.

**Request Body**:

```json
{
  "username": "newuser",
  "password": "userpassword"
}
```

**Response Example**:

```json
{
  "id": "user3",
  "username": "newuser",
  "roles": []
}
```

### Update User

**Endpoint**: `PUT /api/rbac/users/:id`

**Permissions Required**: Either:
- `ADMIN` permission on the `USER` resource type with `*` resource ID, or
- Be the user identified by the `:id` parameter.

**Request Body**:

```json
{
  "username": "updateduser",
  "password": "newpassword"
}
```

**Notes**:
- Only administrators can update the username.
- Users can update their own password.

**Response Example**:

```json
{
  "id": "user1",
  "username": "updateduser",
  "roles": ["role1", "role2"]
}
```

### Delete User

**Endpoint**: `DELETE /api/rbac/users/:id`

**Permissions Required**: `ADMIN` permission on the `USER` resource type with `*` resource ID.

**Notes**:
- Cannot delete your own account.

**Response Example**:

```json
{
  "success": true
}
```

## Role Management

### Get All Roles

**Endpoint**: `GET /api/rbac/roles`

**Permissions Required**: `ADMIN` permission on the `ROLE` resource type with `*` resource ID.

**Response Example**:

```json
[
  {
    "id": "role1",
    "name": "Administrator",
    "permissions": {
      "DATABASE:*": ["ADMIN"],
      "COLLECTION:*": ["READ", "WRITE"]
    }
  },
  {
    "id": "role2",
    "name": "User",
    "permissions": {
      "COLLECTION:public": ["READ"]
    }
  }
]
```

### Get Role

**Endpoint**: `GET /api/rbac/roles/:id`

**Permissions Required**: `READ` permission on the `ROLE` resource type with `*` resource ID.

**Response Example**:

```json
{
  "id": "role1",
  "name": "Administrator",
  "permissions": {
    "DATABASE:*": ["ADMIN"],
    "COLLECTION:*": ["READ", "WRITE"]
  }
}
```

### Create Role

**Endpoint**: `POST /api/rbac/roles`

**Permissions Required**: `ADMIN` permission on the `ROLE` resource type with `*` resource ID.

**Request Body**:

```json
{
  "name": "newrole"
}
```

**Response Example**:

```json
{
  "id": "role3",
  "name": "newrole",
  "permissions": {}
}
```

### Delete Role

**Endpoint**: `DELETE /api/rbac/roles/:id`

**Permissions Required**: `ADMIN` permission on the `ROLE` resource type with `*` resource ID.

**Response Example**:

```json
{
  "success": true
}
```

## Role-User Management

### Add User to Role

**Endpoint**: `POST /api/rbac/roles/:id/users/:user_id`

**Permissions Required**: `ADMIN` permission on the `ROLE` resource type with `*` resource ID.

**Response Example**:

```json
{
  "success": true
}
```

### Remove User from Role

**Endpoint**: `DELETE /api/rbac/roles/:id/users/:user_id`

**Permissions Required**: `ADMIN` permission on the `ROLE` resource type with `*` resource ID.

**Response Example**:

```json
{
  "success": true
}
```

## Permission Management

### Grant Permission

**Endpoint**: `POST /api/rbac/roles/:id/permissions`

**Permissions Required**: `ADMIN` permission on the `PERMISSION` resource type with `*` resource ID.

**Request Body**:

```json
{
  "resource_type": "COLLECTION",
  "resource_id": "users",
  "permission": "READ"
}
```

**Notes**:
- Resource types can be: `DATABASE`, `COLLECTION`, `DOCUMENT`, `USER`, `ROLE`, or `PERMISSION`.
- Permission types can be: `READ`, `WRITE`, `DELETE`, or `ADMIN`.

**Response Example**:

```json
{
  "success": true
}
```

### Revoke Permission

**Endpoint**: `DELETE /api/rbac/roles/:id/permissions`

**Permissions Required**: `ADMIN` permission on the `PERMISSION` resource type with `*` resource ID.

**Request Body**:

```json
{
  "resource_type": "COLLECTION",
  "resource_id": "users",
  "permission": "READ"
}
```

**Response Example**:

```json
{
  "success": true
}
```

## Error Responses

All API endpoints return standard HTTP status codes:

- `200 OK`: Request succeeded.
- `201 Created`: Resource was successfully created.
- `400 Bad Request`: Invalid request parameters or body.
- `401 Unauthorized`: Missing or invalid authentication token.
- `403 Forbidden`: Insufficient permissions.
- `404 Not Found`: Resource not found.
- `500 Internal Server Error`: Server-side error.

Error responses have the following format:

```json
{
  "error": "Error message"
}
```