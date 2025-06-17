/**
 * CRITICAL ARCHITECTURAL DISTINCTION - READ THIS FIRST!
 * 
 * JDBX implements a sophisticated two-layer architecture:
 * 
 * 1. STORAGE LAYER (Physical)
 *    - Single unified collection: "default/documents" 
 *    - ALL data stored here with discriminator fields
 *    - Direct skiplist/B-tree operations
 *    - Use storage_*() functions ONLY
 * 
 * 2. VIRTUAL LAYER (Logical)
 *    - Conceptual collections: "users", "roles", "sessions"
 *    - Document types distinguished by "type" field
 *    - Business logic and RBAC enforcement
 *    - Use virtual_*() functions ONLY
 * 
 * NEVER MIX THESE LAYERS!
 * 
 * ❌ WRONG - Physical collection path:
 *    db_query_documents(db, "system", "users", query)
 * 
 * ✅ CORRECT - Virtual through unified storage:
 *    query = {"type": "user", "library": "system"}
 *    storage_query_documents(db, query)
 * 
 * ✅ CORRECT - Virtual with business logic:
 *    virtual_query_users(db, "system", filters)
 */

#ifndef VIRTUAL_VS_STORAGE_CRITICAL_H
#define VIRTUAL_VS_STORAGE_CRITICAL_H

/* Compile-time assertions to prevent misuse */
#define ASSERT_NOT_PHYSICAL_COLLECTION(collection) \
    static_assert(0, "ERROR: Using physical collection '" collection "' - use storage layer with type discrimination instead!")

#define ASSERT_STORAGE_LAYER_ONLY(func) \
    _Pragma("GCC warning \"" func " is a storage layer function - ensure you're not mixing with virtual concepts\"")

#define ASSERT_VIRTUAL_LAYER_ONLY(func) \
    _Pragma("GCC warning \"" func " is a virtual layer function - ensure you're not mixing with storage operations\"")

#endif /* VIRTUAL_VS_STORAGE_CRITICAL_H */