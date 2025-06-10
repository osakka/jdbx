/**
 * Cascading Versioning Policy System
 * 
 * Implements a hierarchical versioning policy that cascades from:
 * Library → Collection → Document
 * 
 * Each level can override settings from the parent level.
 */

#define _GNU_SOURCE /* for strptime */
#include "database/versioning_policy.h"
#include "database/database.h"
#include "database/unified_documents.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <string.h>
#include <time.h>
#include <stdbool.h>

/* Default versioning policy */
static const versioning_policy_t DEFAULT_POLICY = {
    .enabled = false,
    .max_versions = 10,
    .retention_days = 30,
    .version_on_update = true,
    .version_on_delete = true,
    .compression_enabled = false,
    .archive_old_versions = false,
    .archive_after_days = 90
};

/**
 * Load versioning policy from a JSON object
 */
static void load_policy_from_json(versioning_policy_t* policy, json_value_t* json) {
    if (!policy || !json || json->type != JSON_OBJECT) {
        return;
    }
    
    /* Load enabled flag */
    json_value_t* enabled = json_object_get(json, "enabled");
    if (enabled && enabled->type == JSON_BOOLEAN) {
        policy->enabled = enabled->value.boolean;
    }
    
    /* Load max versions */
    json_value_t* max_vers = json_object_get(json, "maxVersions");
    if (max_vers && max_vers->type == JSON_NUMBER) {
        policy->max_versions = (int)max_vers->value.number;
    } else if (max_vers && max_vers->type == JSON_INTEGER) {
        policy->max_versions = (int)max_vers->value.integer;
    }
    
    /* Load retention days */
    json_value_t* retention = json_object_get(json, "retentionDays");
    if (retention && retention->type == JSON_NUMBER) {
        policy->retention_days = (int)retention->value.number;
    } else if (retention && retention->type == JSON_INTEGER) {
        policy->retention_days = (int)retention->value.integer;
    }
    
    /* Load version triggers */
    json_value_t* on_update = json_object_get(json, "versionOnUpdate");
    if (on_update && on_update->type == JSON_BOOLEAN) {
        policy->version_on_update = on_update->value.boolean;
    }
    
    json_value_t* on_delete = json_object_get(json, "versionOnDelete");
    if (on_delete && on_delete->type == JSON_BOOLEAN) {
        policy->version_on_delete = on_delete->value.boolean;
    }
    
    /* Load compression settings */
    json_value_t* compression = json_object_get(json, "compressionEnabled");
    if (compression && compression->type == JSON_BOOLEAN) {
        policy->compression_enabled = compression->value.boolean;
    }
    
    /* Load archive settings */
    json_value_t* archive = json_object_get(json, "archiveOldVersions");
    if (archive && archive->type == JSON_BOOLEAN) {
        policy->archive_old_versions = archive->value.boolean;
    }
    
    json_value_t* archive_days = json_object_get(json, "archiveAfterDays");
    if (archive_days && archive_days->type == JSON_NUMBER) {
        policy->archive_after_days = (int)archive_days->value.number;
    } else if (archive_days && archive_days->type == JSON_INTEGER) {
        policy->archive_after_days = (int)archive_days->value.integer;
    }
}

/**
 * Convert versioning policy to JSON
 */
json_value_t* versioning_policy_to_json(const versioning_policy_t* policy) {
    if (!policy) {
        return NULL;
    }
    
    json_value_t* json = json_create_object();
    if (!json) {
        return NULL;
    }
    
    json_object_set(json, "enabled", json_create_boolean(policy->enabled));
    json_object_set(json, "maxVersions", json_create_number(policy->max_versions));
    json_object_set(json, "retentionDays", json_create_number(policy->retention_days));
    json_object_set(json, "versionOnUpdate", json_create_boolean(policy->version_on_update));
    json_object_set(json, "versionOnDelete", json_create_boolean(policy->version_on_delete));
    json_object_set(json, "compressionEnabled", json_create_boolean(policy->compression_enabled));
    json_object_set(json, "archiveOldVersions", json_create_boolean(policy->archive_old_versions));
    json_object_set(json, "archiveAfterDays", json_create_number(policy->archive_after_days));
    
    return json;
}

/**
 * Get library versioning policy
 */
versioning_policy_t* versioning_policy_get_library(database_t* db, const char* library_name) {
    if (!db || !library_name) {
        return NULL;
    }
    
    versioning_policy_t* policy = calloc(1, sizeof(versioning_policy_t));
    if (!policy) {
        return NULL;
    }
    
    /* Start with defaults */
    memcpy(policy, &DEFAULT_POLICY, sizeof(versioning_policy_t));
    
    /* Query library document from documents collection */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("library"));
    json_object_set(query, "name", json_create_string(library_name));
    
    json_value_t* results = db_query_documents(db, DOCUMENTS_COLLECTION, query);
    json_free(query);
    
    if (!results) {
        return policy;
    }
    
    json_value_t* documents = json_object_get(results, "documents");
    if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
        json_value_t* lib_doc = json_array_get(documents, 0);
        json_value_t* settings = json_object_get(lib_doc, "settings");
        if (settings) {
            json_value_t* versioning = json_object_get(settings, "versioning");
            if (versioning) {
                load_policy_from_json(policy, versioning);
                LOG_DEBUG("Loaded versioning policy for library %s", library_name);
            }
        }
    }
    
    json_free(results);
    return policy;
}

/**
 * Get collection versioning policy (with library cascade)
 */
versioning_policy_t* versioning_policy_get_collection(database_t* db, const char* library_name,
                                                     const char* collection_name) {
    if (!db || !collection_name) {
        return NULL;
    }
    
    /* Start with library policy if available */
    versioning_policy_t* policy;
    if (library_name) {
        policy = versioning_policy_get_library(db, library_name);
    } else {
        policy = calloc(1, sizeof(versioning_policy_t));
        if (!policy) return NULL;
        memcpy(policy, &DEFAULT_POLICY, sizeof(versioning_policy_t));
    }
    
    /* Query collection document */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("collection"));
    json_object_set(query, "library", json_create_string(library_name ? library_name : "default"));
    json_object_set(query, "name", json_create_string(collection_name));
    
    json_value_t* results = db_query_documents(db, DOCUMENTS_COLLECTION, query);
    json_free(query);
    
    if (!results) {
        return policy;
    }
    
    json_value_t* documents = json_object_get(results, "documents");
    if (documents && documents->type == JSON_ARRAY && json_array_size(documents) > 0) {
        json_value_t* coll_doc = json_array_get(documents, 0);
        json_value_t* settings = json_object_get(coll_doc, "settings");
        if (settings) {
            json_value_t* versioning = json_object_get(settings, "versioning");
            if (versioning) {
                /* Override library settings with collection settings */
                load_policy_from_json(policy, versioning);
                LOG_DEBUG("Loaded versioning policy for collection %s/%s", 
                         library_name ? library_name : "default", collection_name);
            }
        }
    }
    
    json_free(results);
    return policy;
}

/**
 * Get document versioning policy (with full cascade)
 */
versioning_policy_t* versioning_policy_get_document(database_t* db, const char* library_name,
                                                   const char* collection_name, json_value_t* document) {
    if (!db || !collection_name) {
        return NULL;
    }
    
    /* Start with collection policy (which includes library cascade) */
    versioning_policy_t* policy = versioning_policy_get_collection(db, library_name, collection_name);
    if (!policy) {
        return NULL;
    }
    
    /* Check if document has versioning overrides */
    if (document && document->type == JSON_OBJECT) {
        json_value_t* doc_versioning = json_object_get(document, "_versioning");
        if (doc_versioning) {
            load_policy_from_json(policy, doc_versioning);
            LOG_DEBUG("Applied document-level versioning overrides");
        }
    }
    
    return policy;
}

/**
 * Create a version of a document
 */
int versioning_create_version(database_t* db, const char* library_name,
                             const char* collection_name, json_value_t* document,
                             const char* operation) {
    if (!db || !collection_name || !document) {
        return -1;
    }
    
    /* Get versioning policy */
    versioning_policy_t* policy = versioning_policy_get_document(db, library_name, 
                                                                collection_name, document);
    if (!policy) {
        return -1;
    }
    
    /* Check if versioning is enabled */
    if (!policy->enabled) {
        free(policy);
        return 0; /* Success - versioning not enabled */
    }
    
    /* Don't version documents in the versions collection to avoid infinite loop */
    if (strstr(collection_name, "versions") != NULL) {
        free(policy);
        return 0; /* Success - skip versioning for version documents */
    }
    
    /* Check if we should version this operation */
    if (strcmp(operation, "update") == 0 && !policy->version_on_update) {
        free(policy);
        return 0;
    }
    if (strcmp(operation, "delete") == 0 && !policy->version_on_delete) {
        free(policy);
        return 0;
    }
    
    /* Get document ID */
    json_value_t* id_val = json_object_get(document, "uuid");
    if (!id_val || id_val->type != JSON_STRING) {
        LOG_ERROR("Document missing uuid field for versioning");
        free(policy);
        return -1;
    }
    const char* doc_id = id_val->value.string;
    
    /* Create version collection name */
    char version_collection[512];
    snprintf(version_collection, sizeof(version_collection), "%s/versions", 
             library_name ? library_name : "system");
    
    /* Create version document */
    json_value_t* version_doc = json_create_object();
    if (!version_doc) {
        free(policy);
        return -1;
    }
    
    /* Add version metadata */
    json_object_set(version_doc, "type", json_create_string("version"));
    json_object_set(version_doc, "document_id", json_create_string(doc_id));
    json_object_set(version_doc, "collection", json_create_string(collection_name));
    json_object_set(version_doc, "library", json_create_string(library_name ? library_name : "default"));
    json_object_set(version_doc, "operation", json_create_string(operation));
    
    /* Add timestamp */
    time_t now = time(NULL);
    char timestamp[64];
    struct tm* utc_tm = gmtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
    json_object_set(version_doc, "versioned_at", json_create_string(timestamp));
    
    /* Add document snapshot */
    json_value_t* snapshot = json_clone(document);
    if (policy->compression_enabled) {
        /* TODO: Implement compression */
        json_object_set(version_doc, "snapshot", snapshot);
        json_object_set(version_doc, "compressed", json_create_boolean(false));
    } else {
        json_object_set(version_doc, "snapshot", snapshot);
        json_object_set(version_doc, "compressed", json_create_boolean(false));
    }
    
    /* Store version */
    json_value_t* result = db_insert_document(db, version_collection, version_doc);
    json_free(version_doc);
    
    if (!result) {
        LOG_ERROR("Failed to create version for document %s", doc_id);
        free(policy);
        return -1;
    }
    
    json_free(result);
    
    /* Clean up old versions if needed */
    versioning_cleanup_old_versions(db, library_name, collection_name, doc_id, policy);
    
    free(policy);
    return 0;
}

/**
 * Clean up old versions based on policy
 */
int versioning_cleanup_old_versions(database_t* db, const char* library_name,
                                   const char* collection_name, const char* document_id,
                                   versioning_policy_t* policy) {
    if (!db || !document_id || !policy) {
        return -1;
    }
    
    char version_collection[512];
    snprintf(version_collection, sizeof(version_collection), "%s/versions", 
             library_name ? library_name : "system");
    
    /* Query all versions for this document */
    json_value_t* query = json_create_object();
    json_object_set(query, "document_id", json_create_string(document_id));
    json_object_set(query, "collection", json_create_string(collection_name));
    
    json_value_t* results = db_query_documents(db, version_collection, query);
    json_free(query);
    
    if (!results) {
        return -1;
    }
    
    json_value_t* documents = json_object_get(results, "documents");
    if (!documents || documents->type != JSON_ARRAY) {
        json_free(results);
        return 0;
    }
    
    size_t version_count = json_array_size(documents);
    
    /* Sort versions by timestamp (newest first) */
    /* Note: Assuming versions are already sorted by created_at desc from query */
    
    /* Apply max_versions limit */
    if (policy->max_versions > 0 && version_count > (size_t)policy->max_versions) {
        /* Delete excess versions */
        for (size_t i = policy->max_versions; i < version_count; i++) {
            json_value_t* version = json_array_get(documents, i);
            json_value_t* id_val = json_object_get(version, "uuid");
            if (id_val && id_val->type == JSON_STRING) {
                if (policy->archive_old_versions) {
                    /* TODO: Archive to cold storage */
                    LOG_DEBUG("Would archive version %s", id_val->value.string);
                } else {
                    /* Delete the version */
                    db_delete_document(db, version_collection, id_val->value.string);
                    LOG_DEBUG("Deleted excess version %s", id_val->value.string);
                }
            }
        }
    }
    
    /* Apply retention_days limit */
    if (policy->retention_days > 0) {
        time_t now = time(NULL);
        time_t retention_cutoff = now - (policy->retention_days * 86400); /* days to seconds */
        
        for (size_t i = 0; i < version_count; i++) {
            json_value_t* version = json_array_get(documents, i);
            json_value_t* timestamp_val = json_object_get(version, "versioned_at");
            if (timestamp_val && timestamp_val->type == JSON_STRING) {
                /* Parse ISO timestamp */
                struct tm tm = {0};
                if (strptime(timestamp_val->value.string, "%Y-%m-%dT%H:%M:%SZ", &tm)) {
                    time_t version_time = mktime(&tm);
                    if (version_time < retention_cutoff) {
                        json_value_t* id_val = json_object_get(version, "uuid");
                        if (id_val && id_val->type == JSON_STRING) {
                            if (policy->archive_old_versions && 
                                policy->archive_after_days > 0 &&
                                (now - version_time) > (policy->archive_after_days * 86400)) {
                                /* Archive if old enough */
                                LOG_DEBUG("Would archive old version %s", id_val->value.string);
                            } else {
                                /* Delete the version */
                                db_delete_document(db, version_collection, id_val->value.string);
                                LOG_DEBUG("Deleted old version %s (past retention)", id_val->value.string);
                            }
                        }
                    }
                }
            }
        }
    }
    
    LOG_INFO("Cleaned up versions for document %s: %zu versions reviewed", document_id, version_count);
    
    json_free(results);
    return 0;
}

/**
 * Get version history for a document
 */
json_value_t* versioning_get_history(database_t* db, const char* library_name,
                                    const char* collection_name, const char* document_id) {
    if (!db || !collection_name || !document_id) {
        return NULL;
    }
    
    char version_collection[512];
    snprintf(version_collection, sizeof(version_collection), "%s/versions", 
             library_name ? library_name : "system");
    
    /* Query versions for this document */
    json_value_t* query = json_create_object();
    json_object_set(query, "document_id", json_create_string(document_id));
    json_object_set(query, "collection", json_create_string(collection_name));
    
    json_value_t* results = db_query_documents(db, version_collection, query);
    json_free(query);
    
    return results;
}

/**
 * Restore a specific version
 */
int versioning_restore_version(database_t* db, const char* library_name,
                              const char* collection_name, const char* version_id) {
    if (!db || !collection_name || !version_id) {
        return -1;
    }
    
    char version_collection[512];
    snprintf(version_collection, sizeof(version_collection), "%s/versions", 
             library_name ? library_name : "system");
    
    /* Get the version document */
    json_value_t* version_doc = db_get_document(db, version_collection, version_id);
    if (!version_doc) {
        LOG_ERROR("Version not found: %s", version_id);
        return -1;
    }
    
    /* Extract snapshot */
    json_value_t* snapshot = json_object_get(version_doc, "snapshot");
    if (!snapshot) {
        LOG_ERROR("Version missing snapshot: %s", version_id);
        json_free(version_doc);
        return -1;
    }
    
    /* Get document ID */
    json_value_t* doc_id_val = json_object_get(version_doc, "document_id");
    if (!doc_id_val || doc_id_val->type != JSON_STRING) {
        LOG_ERROR("Version missing document_id: %s", version_id);
        json_free(version_doc);
        return -1;
    }
    const char* doc_id = doc_id_val->value.string;
    
    /* Create a new version of current state before restoring */
    json_value_t* current = db_get_document(db, collection_name, doc_id);
    if (current) {
        versioning_create_version(db, library_name, collection_name, current, "restore");
        json_free(current);
    }
    
    /* Restore the snapshot */
    json_value_t* restored = json_clone(snapshot);
    json_value_t* result = db_update_document(db, collection_name, doc_id, restored);
    
    json_free(restored);
    json_free(version_doc);
    
    if (!result) {
        LOG_ERROR("Failed to restore version %s", version_id);
        return -1;
    }
    
    json_free(result);
    LOG_INFO("Restored document %s to version %s", doc_id, version_id);
    return 0;
}