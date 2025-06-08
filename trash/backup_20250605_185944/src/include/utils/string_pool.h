#ifndef STRING_POOL_H
#define STRING_POOL_H

#include <stddef.h>
#include <stdint.h>

/**
 * High-performance string interning pool for JSONdb
 * 
 * This provides string deduplication and interning for commonly used strings
 * such as JSON field names, document IDs, and collection names. Reduces memory
 * usage by storing only one copy of each unique string.
 * 
 * Features:
 * - Fast hash-based lookup (O(1) average case)
 * - Thread-safe operations with fine-grained locking
 * - Reference counting for automatic cleanup
 * - Size limits to prevent unbounded growth
 * - Statistics tracking for optimization
 */

/* Opaque string pool type */
typedef struct string_pool string_pool_t;

/* Interned string handle */
typedef struct {
    const char* str;    /* Pointer to interned string (read-only) */
    uint32_t ref_count; /* Reference count */
    uint32_t hash;      /* Pre-computed hash value */
} interned_string_t;

/**
 * Create a new string pool
 * 
 * @param initial_capacity Initial number of buckets (must be power of 2)
 * @param max_strings Maximum number of strings to intern (0 = unlimited)
 * @return New string pool, or NULL on failure
 */
string_pool_t* string_pool_create(size_t initial_capacity, size_t max_strings);

/**
 * Destroy a string pool and free all memory
 * 
 * @param pool String pool to destroy
 */
void string_pool_destroy(string_pool_t* pool);

/**
 * Intern a string in the pool
 * 
 * If the string already exists, returns the existing interned copy and
 * increments its reference count. Otherwise, creates a new interned copy.
 * 
 * @param pool String pool
 * @param str String to intern (will be copied)
 * @return Interned string handle, or NULL on failure
 */
interned_string_t* string_pool_intern(string_pool_t* pool, const char* str);

/**
 * Intern a string with known length (more efficient)
 * 
 * @param pool String pool
 * @param str String to intern (will be copied)
 * @param len Length of string
 * @return Interned string handle, or NULL on failure
 */
interned_string_t* string_pool_intern_len(string_pool_t* pool, const char* str, size_t len);

/**
 * Release an interned string
 * 
 * Decrements the reference count. When it reaches zero, the string
 * may be removed from the pool (depending on implementation policy).
 * 
 * @param pool String pool
 * @param istr Interned string to release
 */
void string_pool_release(string_pool_t* pool, interned_string_t* istr);

/**
 * Look up a string without interning it
 * 
 * @param pool String pool
 * @param str String to look up
 * @return Existing interned string, or NULL if not found
 */
interned_string_t* string_pool_lookup(string_pool_t* pool, const char* str);

/**
 * Get string pool statistics
 * 
 * @param pool String pool
 * @param total_strings Output: Total strings in pool
 * @param total_memory Output: Total memory used by pool
 * @param hit_count Output: Number of successful lookups
 * @param miss_count Output: Number of new allocations
 */
void string_pool_get_stats(string_pool_t* pool, size_t* total_strings, 
                          size_t* total_memory, uint64_t* hit_count, uint64_t* miss_count);

/**
 * Clear all statistics
 * 
 * @param pool String pool
 */
void string_pool_reset_stats(string_pool_t* pool);

/* Global string pools for common use cases */
extern string_pool_t* g_json_keys_pool;     /* For JSON object keys */
extern string_pool_t* g_doc_ids_pool;       /* For document IDs */
extern string_pool_t* g_collection_names_pool; /* For collection names */

/**
 * Initialize global string pools
 * 
 * @return 0 on success, -1 on failure
 */
int string_pools_init(void);

/**
 * Cleanup global string pools
 */
void string_pools_cleanup(void);

/* Convenience macros for common operations */
#define INTERN_JSON_KEY(str) string_pool_intern(g_json_keys_pool, str)
#define INTERN_DOC_ID(str) string_pool_intern(g_doc_ids_pool, str)
#define INTERN_COLLECTION_NAME(str) string_pool_intern(g_collection_names_pool, str)

#define RELEASE_JSON_KEY(istr) string_pool_release(g_json_keys_pool, istr)
#define RELEASE_DOC_ID(istr) string_pool_release(g_doc_ids_pool, istr)  
#define RELEASE_COLLECTION_NAME(istr) string_pool_release(g_collection_names_pool, istr)

#endif /* STRING_POOL_H */