#ifndef INDEX_MAINTENANCE_H
#define INDEX_MAINTENANCE_H

#include <stdint.h>
#include "database/database.h"
#include "utils/json.h"

/**
 * @file index_maintenance.h
 * @brief Automatic index maintenance for adaptive indexes
 * 
 * This system ensures that all adaptive indexes are properly maintained
 * when documents are inserted, updated, or deleted. It provides hooks
 * that can be called from the main database operations.
 */

/* Index maintenance operation types */
typedef enum index_maintenance_operation {
    INDEX_MAINT_INSERT,
    INDEX_MAINT_UPDATE,
    INDEX_MAINT_DELETE
} index_maintenance_operation_t;

/**
 * Hook function type for index maintenance callbacks
 */
typedef int (*index_maintenance_hook_t)(const char* collection_name,
                                       json_value_t* document,
                                       const char* document_id,
                                       uint64_t offset,
                                       void* user_data);

/* Index maintenance statistics */
typedef struct index_maintenance_stats {
    uint64_t inserts_processed;
    uint64_t updates_processed;
    uint64_t deletes_processed;
    uint64_t index_updates_successful;
    uint64_t index_updates_failed;
    uint64_t total_time_ms;
    double avg_time_per_operation_ms;
} index_maintenance_stats_t;

/* === Index Maintenance Functions === */

/**
 * Initialize the index maintenance system
 */
int index_maintenance_init(void);

/**
 * Cleanup the index maintenance system
 */
void index_maintenance_cleanup(void);

/**
 * Handle document insertion - update all relevant indexes
 * @param collection_name The collection name
 * @param document The inserted document
 * @param document_offset The storage offset of the document
 * @return 0 on success, -1 on error
 */
int index_maintenance_handle_insert(const char* collection_name, 
                                   json_value_t* document,
                                   uint64_t document_offset);

/**
 * Handle document update - update all relevant indexes
 * @param collection_name The collection name
 * @param old_document The previous version of the document (optional)
 * @param new_document The new version of the document
 * @param document_id The document ID
 * @param old_offset The old storage offset
 * @param new_offset The new storage offset
 * @return 0 on success, -1 on error
 */
int index_maintenance_handle_update(const char* collection_name,
                                   json_value_t* old_document,
                                   json_value_t* new_document,
                                   const char* document_id,
                                   uint64_t old_offset,
                                   uint64_t new_offset);

/**
 * Handle document deletion - remove from all relevant indexes
 * @param collection_name The collection name
 * @param document The deleted document
 * @param document_id The document ID
 * @param document_offset The storage offset of the document
 * @return 0 on success, -1 on error
 */
int index_maintenance_handle_delete(const char* collection_name,
                                   json_value_t* document,
                                   const char* document_id,
                                   uint64_t document_offset);

/**
 * Update a specific index with field value changes
 * @param collection_name The collection name
 * @param field_path The field path being indexed
 * @param old_value The old field value (NULL for insert)
 * @param new_value The new field value (NULL for delete)
 * @param document_offset The document storage offset
 * @param operation The type of operation
 * @return 0 on success, -1 on error
 */
int index_maintenance_update_field_index(const char* collection_name,
                                        const char* field_path,
                                        json_value_t* old_value,
                                        json_value_t* new_value,
                                        uint64_t document_offset,
                                        index_maintenance_operation_t operation);

/**
 * Extract field value from document for indexing
 * @param document The document
 * @param field_path The field path (supports nested fields with dot notation)
 * @return The field value or NULL if not found
 */
json_value_t* index_maintenance_extract_field_value(json_value_t* document, 
                                                   const char* field_path);

/**
 * Convert field value to indexable string representation
 * @param field_value The field value
 * @param buffer Buffer to store the string representation
 * @param buffer_size Size of the buffer
 * @return 0 on success, -1 on error
 */
int index_maintenance_field_value_to_string(json_value_t* field_value,
                                           char* buffer,
                                           size_t buffer_size);

/**
 * Get all adaptive indexes for a collection
 * @param collection_name The collection name
 * @param index_count Output parameter for number of indexes
 * @return Array of index information structures
 */
struct adaptive_index_info** index_maintenance_get_collection_indexes(const char* collection_name,
                                                                     size_t* index_count);

/**
 * Get index maintenance statistics
 * @return JSON object with statistics
 */
json_value_t* index_maintenance_get_stats(void);

/**
 * Reset index maintenance statistics
 */
void index_maintenance_reset_stats(void);

/**
 * Check if a field path has changed between two documents
 * @param old_doc The old document
 * @param new_doc The new document  
 * @param field_path The field path to check
 * @return 1 if changed, 0 if unchanged
 */
int index_maintenance_field_changed(json_value_t* old_doc,
                                   json_value_t* new_doc,
                                   const char* field_path);

/* === Index Maintenance Hooks === */

/**
 * Register a hook for index maintenance events
 * @param operation The operation type to hook
 * @param hook The hook function
 * @param user_data User data to pass to the hook
 * @return 0 on success, -1 on error
 */
int index_maintenance_register_hook(index_maintenance_operation_t operation,
                                   index_maintenance_hook_t hook,
                                   void* user_data);

/**
 * Unregister a hook for index maintenance events
 * @param operation The operation type
 * @param hook The hook function
 * @return 0 on success, -1 on error
 */
int index_maintenance_unregister_hook(index_maintenance_operation_t operation,
                                     index_maintenance_hook_t hook);

#endif /* INDEX_MAINTENANCE_H */