# Storage vs Virtual Layer Architecture Guide

## ⚠️ CRITICAL: Understanding JDBX's Two-Layer Architecture

JDBX implements a sophisticated architecture that separates physical storage from logical concepts. **Misunderstanding this distinction is the #1 cause of bugs.**

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────────┐
│           APPLICATION / API LAYER               │
│  (Works with logical concepts: users, roles)   │
└─────────────────────┬───────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────┐
│            VIRTUAL LAYER                        │
│  • Logical collections (users, roles, etc)     │
│  • Business logic & validation                 │
│  • RBAC enforcement                           │
│  • Uses: virtual_*() functions                │
└─────────────────────┬───────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────┐
│            STORAGE LAYER                        │
│  • Single physical collection                  │
│  • "default/documents" contains EVERYTHING     │
│  • Type discrimination via fields             │
│  • Uses: storage_*() functions               │
└─────────────────────────────────────────────────┘
```

## 🎯 Key Concepts

### Physical Storage (What Actually Exists)
- **ONE collection**: `default/documents`
- **ALL data** stored here: users, roles, sessions, configs, metrics, EVERYTHING
- Documents distinguished by fields: `type`, `library`, `collection`

### Virtual Collections (Logical Organization)
- **Conceptual groupings**: "users", "roles", "sessions"
- **NOT physical paths**: These don't exist as separate collections
- **Type-based queries**: Filter by `type` field to get specific document types

## ❌ Common Mistakes (DON'T DO THIS)

### Mistake 1: Using Physical Collection Paths
```c
// ❌ WRONG - Assumes physical "system/users" collection exists
json_value_t* users = db_query_documents(db, "system", "users", query);

// ✅ CORRECT - Query unified storage with type filter
json_value_t* query = json_create_object();
json_object_set(query, "type", json_create_string("user"));
json_object_set(query, "library", json_create_string("system"));
json_value_t* users = db_query_documents(db, "default", "documents", query);
```

### Mistake 2: Mixing Storage and Virtual Functions
```c
// ❌ WRONG - Mixing layers
storage_insert_document(db, virtual_create_user(...));

// ✅ CORRECT - Use consistent layer
json_value_t* user_doc = virtual_create_user(db, username, password, library);
// virtual_create_user internally handles storage
```

### Mistake 3: Assuming Hierarchical Paths
```c
// ❌ WRONG - No hierarchical collections exist
const char* path = "system/rbac/users/john";

// ✅ CORRECT - Everything is a document with fields
json_value_t* query = json_create_object();
json_object_set(query, "type", json_create_string("user"));
json_object_set(query, "library", json_create_string("system"));
json_object_set(query, "username", json_create_string("john"));
```

## ✅ Best Practices

### 1. Use the Right Layer Functions

**Storage Layer** (Direct physical access):
```c
storage_insert_document()    // Insert into unified storage
storage_query_documents()    // Query with field filters
storage_update_document()    // Update by UUID
storage_delete_document()    // Delete by UUID
```

**Virtual Layer** (Business logic):
```c
virtual_create_user()        // Creates user with proper fields
virtual_query_users()        // Queries with type="user" automatically
virtual_create_role()        // Creates role with permissions
virtual_query_sessions()     // Gets active sessions
```

### 2. Always Include Type Discrimination
```c
// When querying storage directly, ALWAYS include type
json_value_t* query = json_create_object();
json_object_set(query, "type", json_create_string("session"));  // CRITICAL!
json_object_set(query, "library", json_create_string("system"));
```

### 3. Use Constants for Clarity
```c
// Good practice - use defined constants
#define DOC_TYPE_USER "user"
#define DOC_TYPE_ROLE "role"
#define DOC_TYPE_SESSION "session"

json_object_set(query, "type", json_create_string(DOC_TYPE_USER));
```

## 🔍 How to Identify Which Layer You're In

### You're in the STORAGE layer if:
- Working directly with skiplists or B-trees
- Implementing database internals
- Building indexing or persistence
- Function names start with `storage_` or `db_`

### You're in the VIRTUAL layer if:
- Implementing business logic
- Working with RBAC or authentication
- Building API endpoints
- Function names start with `virtual_` or contain entity names (user, role, etc.)

## 📝 Code Review Checklist

When reviewing code, check for:

- [ ] No hardcoded collection paths like "system/users"
- [ ] All queries include proper type discrimination
- [ ] Consistent use of either storage_*() or virtual_*() functions
- [ ] Comments explaining which layer the code operates in
- [ ] No assumptions about physical collection structure

## 🚨 Red Flags in Code

If you see any of these, the code is probably wrong:

```c
// 🚨 Collection paths that aren't "default/documents"
db_query_documents(db, "system", "users", ...)

// 🚨 Missing type field in queries
json_value_t* query = json_create_object();
// No type field set!

// 🚨 Hardcoded hierarchical paths
sprintf(path, "%s/%s/%s", library, collection, doc_id);

// 🚨 Direct skiplist access for business logic
skiplist_find(db->collections["users"], ...)
```

## 💡 Remember

**The sophistication of JDBX comes from presenting a traditional database interface (libraries, collections, documents) on top of a unified storage engine. This gives us:**

- Performance of a single data structure
- Flexibility of a document database
- Simplicity of unified storage
- Power of virtual organization

**But it requires discipline:** Always be conscious of which layer you're working in!