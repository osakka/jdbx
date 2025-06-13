# Multi-Role Permission System

## Overview

JDBX implements a flexible multi-role permission system where:
- Users can have multiple roles
- Roles can be nested (inherit from parent roles)
- Documents can grant permissions to multiple roles
- Field-level permissions use regex patterns

## Collection Metadata Example

```json
{
  "_id": "_meta",
  "_collection": "user_profiles",
  "permissions": {
    "owner_id": "system",
    "owner_perms": "rwxda",
    "world_perms": "r",
    "roles": [
      {
        "role_id": "admins",
        "permissions": "rwxda"
      },
      {
        "role_id": "user_managers",
        "permissions": "rw"
      },
      {
        "role_id": "auditors",
        "permissions": "r"
      }
    ],
    "field_rules": [
      {
        "pattern": "^_id$",
        "owner_perms": "r",
        "world_perms": "r",
        "roles": []
      },
      {
        "pattern": "^password(_hash)?$",
        "owner_perms": "w",
        "world_perms": "",
        "roles": [
          {
            "role_id": "security_team",
            "permissions": "rw"
          }
        ]
      },
      {
        "pattern": "^(ssn|tax_id)$",
        "owner_perms": "r",
        "world_perms": "",
        "roles": [
          {
            "role_id": "hr_managers",
            "permissions": "rw"
          },
          {
            "role_id": "auditors",
            "permissions": "r"
          }
        ]
      }
    ]
  }
}
```

## Nested Roles

```json
{
  "_id": "engineering",
  "_type": "role",
  "name": "Engineering Department",
  "parent_roles": ["all_employees"],
  "child_roles": ["frontend_team", "backend_team", "devops_team"]
}

{
  "_id": "frontend_team", 
  "_type": "role",
  "name": "Frontend Team",
  "parent_roles": ["engineering"],
  "child_roles": ["senior_frontend", "junior_frontend"]
}
```

## User with Multiple Roles

```json
{
  "_id": "user_alice",
  "username": "alice",
  "roles": ["senior_frontend", "code_reviewers", "project_x_team"]
}
```

Alice inherits permissions from:
1. Direct roles: senior_frontend, code_reviewers, project_x_team
2. Parent roles: frontend_team, engineering, all_employees
3. World permissions

## Permission Resolution

```javascript
function resolvePermissions(user, document, field) {
  // Expand all roles including inherited
  const allRoles = expandRoles(user.roles);
  
  let perms = 0;
  
  // Owner permissions
  if (document.owner_id === user.id) {
    perms |= document.permissions.owner_perms;
  }
  
  // World permissions
  perms |= document.permissions.world_perms;
  
  // Role permissions (additive)
  for (const role of allRoles) {
    const rolePerms = findRolePermissions(document.permissions.roles, role);
    perms |= rolePerms;
  }
  
  // Apply field-level rules
  if (field) {
    perms = applyFieldRules(perms, field, user, document);
  }
  
  return perms;
}
```

## Benefits

1. **Flexible**: Users can have multiple roles
2. **Hierarchical**: Role inheritance reduces duplication
3. **Granular**: Field-level control with regex patterns
4. **Performant**: Permissions cached per request
5. **Natural**: Fits JDBX's document model perfectly