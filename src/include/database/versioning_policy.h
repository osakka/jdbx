#ifndef VERSIONING_POLICY_H
#define VERSIONING_POLICY_H

#include "utils/json.h"
#include "database/database.h"

/**
 * Versioning Policy Structure
 * 
 * Defines the versioning behavior for documents in the database.
 * Policies cascade from Library → Collection → Document level.
 */
typedef struct {
    /* Basic settings */
    int enabled;                /* Whether versioning is enabled */
    int max_versions;          /* Maximum number of versions to keep (0 = unlimited) */
    int retention_days;        /* Days to retain versions (0 = forever) */
    
    /* Triggers */
    int version_on_update;     /* Create version on document update */
    int version_on_delete;     /* Create version on document delete */
    
    /* Advanced settings */
    int compression_enabled;   /* Compress version snapshots */
    int archive_old_versions;  /* Move old versions to archive */
    int archive_after_days;    /* Days before archiving */
} versioning_policy_t;

/**
 * Get versioning policy for a library
 * Returns the policy defined at the library level
 */
versioning_policy_t* versioning_policy_get_library(database_t* db, const char* library_name);

/**
 * Get versioning policy for a collection
 * Returns the cascaded policy (library → collection)
 */
versioning_policy_t* versioning_policy_get_collection(database_t* db, const char* library_name,
                                                     const char* collection_name);

/**
 * Get versioning policy for a document
 * Returns the fully cascaded policy (library → collection → document)
 */
versioning_policy_t* versioning_policy_get_document(database_t* db, const char* library_name,
                                                   const char* collection_name, json_value_t* document);

/**
 * Convert versioning policy to JSON representation
 */
json_value_t* versioning_policy_to_json(const versioning_policy_t* policy);

/**
 * Create a version of a document
 * @param operation The operation that triggered versioning ("update", "delete", "restore")
 */
int versioning_create_version(database_t* db, const char* library_name,
                             const char* collection_name, json_value_t* document,
                             const char* operation);

/**
 * Clean up old versions based on policy settings
 */
int versioning_cleanup_old_versions(database_t* db, const char* library_name,
                                   const char* collection_name, const char* document_id,
                                   versioning_policy_t* policy);

/**
 * Get version history for a document
 * Returns array of version documents
 */
json_value_t* versioning_get_history(database_t* db, const char* library_name,
                                    const char* collection_name, const char* document_id);

/**
 * Restore a specific version of a document
 * This creates a new version of the current state before restoring
 */
int versioning_restore_version(database_t* db, const char* library_name,
                              const char* collection_name, const char* version_id);

#endif /* VERSIONING_POLICY_H */