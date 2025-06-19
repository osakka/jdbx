/*
 * JDBX Integrated Indexes
 * All indexes stored inside the JDBX file - no separate .idx files!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "storage/jdbx.h"
#include "utils/buffer_pool.h"
#include "database/database.h"
#include "utils/logger.h"
#include "utils/json_helpers.h"

/*==============================================================================
 * Hash Index Implementation (stored as B-tree in JDBX)
 *============================================================================*/

/* Hash function for document IDs */
static uint64_t hash_string(const char* str) {
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/* Create hash index key from document ID */
static void create_hash_key(const char* doc_id, char* key_buf, size_t buf_size) {
    uint64_t hash = hash_string(doc_id);
    snprintf(key_buf, buf_size, "hash:%016lx:%s", hash, doc_id);
}

/* Hash index operations using JDBX B-tree */
int jdbx_hash_index_insert(jdbx_btree_t* hash_idx, const char* doc_id, uint64_t doc_offset) {
    char hash_key[512];
    create_hash_key(doc_id, hash_key, sizeof(hash_key));
    
    return jdbx_btree_insert(hash_idx, hash_key, strlen(hash_key), 
                            &doc_offset, sizeof(doc_offset));
}

int jdbx_hash_index_find(jdbx_btree_t* hash_idx, const char* doc_id, uint64_t* doc_offset) {
    char hash_key[512];
    create_hash_key(doc_id, hash_key, sizeof(hash_key));
    
    void* value = NULL;
    size_t value_len = 0;
    
    if (jdbx_btree_get(hash_idx, hash_key, strlen(hash_key), &value, &value_len) == 0) {
        if (value_len == sizeof(uint64_t)) {
            *doc_offset = *(uint64_t*)value;
            BUFFER_FREE(value);
            return 0;
        }
        BUFFER_FREE(value);
    }
    
    return -1;
}

int jdbx_hash_index_delete(jdbx_btree_t* hash_idx, const char* doc_id) {
    char hash_key[512];
    create_hash_key(doc_id, hash_key, sizeof(hash_key));
    
    return jdbx_btree_delete(hash_idx, hash_key, strlen(hash_key));
}

/*==============================================================================
 * Secondary B-tree Index Implementation (field-based indexes)
 *============================================================================*/

/* Create index key from field value and document ID */
static void create_index_key(const char* field_value, const char* doc_id, 
                            char* key_buf, size_t buf_size) {
    /* Format: field_value\0doc_id to ensure proper ordering */
    size_t value_len = strlen(field_value);
    size_t id_len = strlen(doc_id);
    
    if (value_len + id_len + 2 > buf_size) {
        /* Truncate if too long */
        value_len = buf_size - id_len - 2;
    }
    
    memcpy(key_buf, field_value, value_len);
    key_buf[value_len] = '\0';
    memcpy(key_buf + value_len + 1, doc_id, id_len + 1);
}

/* Secondary index operations */
int jdbx_secondary_index_insert(jdbx_btree_t* idx, const char* field_value, 
                               const char* doc_id) {
    char idx_key[1024];
    create_index_key(field_value, doc_id, idx_key, sizeof(idx_key));
    
    /* Store doc_id as value for easy retrieval */
    return jdbx_btree_insert(idx, idx_key, strlen(field_value) + strlen(doc_id) + 2,
                            doc_id, strlen(doc_id) + 1);
}

/* Find all documents with a specific field value */
json_value_t* jdbx_secondary_index_find(jdbx_btree_t* idx, const char* field_value) {
    json_value_t* results = json_create_array();
    
    /* Create iterator starting at field_value */
    jdbx_btree_iterator_t* iter = jdbx_btree_iterator_create(idx);
    if (!iter) return results;
    
    size_t field_len = strlen(field_value);
    
    void* key = NULL;
    size_t key_len = 0;
    void* value = NULL;
    size_t value_len = 0;
    
    while (jdbx_btree_iterator_next(iter, &key, &key_len, &value, &value_len) == 0) {
        /* Check if key starts with field_value */
        if (key_len > field_len && memcmp(key, field_value, field_len) == 0 &&
            ((char*)key)[field_len] == '\0') {
            /* Found matching document */
            json_value_t* doc_id = json_create_string((char*)value);
            if (doc_id) {
                json_array_append(results, doc_id);
            }
        } else if (memcmp(key, field_value, field_len) > 0) {
            /* Passed our range */
            BUFFER_FREE(key);
            BUFFER_FREE(value);
            break;
        }
        
        BUFFER_FREE(key);
        BUFFER_FREE(value);
    }
    
    jdbx_btree_iterator_destroy(iter);
    return results;
}

int jdbx_secondary_index_delete(jdbx_btree_t* idx, const char* field_value, 
                               const char* doc_id) {
    char idx_key[1024];
    create_index_key(field_value, doc_id, idx_key, sizeof(idx_key));
    
    return jdbx_btree_delete(idx, idx_key, strlen(field_value) + strlen(doc_id) + 2);
}

/*==============================================================================
 * Index Maintenance Functions
 *============================================================================*/

/* Update all indexes when a document is inserted/updated */
int jdbx_update_document_indexes(jdbx_btree_t* hash_idx, 
                                jdbx_btree_t** secondary_indexes,
                                const char** index_fields,
                                int num_indexes,
                                const char* doc_id,
                                json_value_t* document,
                                uint64_t doc_offset) {
    /* Update hash index */
    if (jdbx_hash_index_insert(hash_idx, doc_id, doc_offset) != 0) {
        LOG_ERROR("Failed to update hash index for document %s", doc_id);
        return -1;
    }
    
    /* Update secondary indexes */
    for (int i = 0; i < num_indexes; i++) {
        if (!secondary_indexes[i] || !index_fields[i]) continue;
        
        /* Extract field value from document */
        json_value_t* field_val = json_object_get(document, index_fields[i]);
        if (field_val) {
            char* field_str = NULL;
            
            /* Convert field value to string */
            switch (field_val->type) {
                case JSON_STRING:
                    field_str = BUFFER_STRDUP(json_get_string(field_val));
                    break;
                case JSON_NUMBER:
                    field_str = BUFFER_ALLOC(32);
                    snprintf(field_str, 32, "%.6f", json_get_number(field_val));
                    break;
                case JSON_BOOLEAN:
                    field_str = BUFFER_STRDUP(json_get_boolean(field_val) ? "true" : "false");
                    break;
                default:
                    /* Skip non-indexable types */
                    continue;
            }
            
            if (field_str) {
                if (jdbx_secondary_index_insert(secondary_indexes[i], field_str, doc_id) != 0) {
                    LOG_WARNING("Failed to update secondary index for field %s", index_fields[i]);
                }
                BUFFER_FREE(field_str);
            }
        }
    }
    
    return 0;
}

/* Remove document from all indexes */
int jdbx_remove_document_indexes(jdbx_btree_t* hash_idx,
                                jdbx_btree_t** secondary_indexes,
                                const char** index_fields,
                                int num_indexes,
                                const char* doc_id,
                                json_value_t* document) {
    /* Remove from hash index */
    if (jdbx_hash_index_delete(hash_idx, doc_id) != 0) {
        LOG_WARNING("Failed to remove document %s from hash index", doc_id);
    }
    
    /* Remove from secondary indexes */
    for (int i = 0; i < num_indexes; i++) {
        if (!secondary_indexes[i] || !index_fields[i]) continue;
        
        /* Extract field value from document */
        json_value_t* field_val = json_object_get(document, index_fields[i]);
        if (field_val) {
            char* field_str = NULL;
            
            /* Convert field value to string */
            switch (field_val->type) {
                case JSON_STRING:
                    field_str = BUFFER_STRDUP(json_get_string(field_val));
                    break;
                case JSON_NUMBER:
                    field_str = BUFFER_ALLOC(32);
                    snprintf(field_str, 32, "%.6f", json_get_number(field_val));
                    break;
                case JSON_BOOLEAN:
                    field_str = BUFFER_STRDUP(json_get_boolean(field_val) ? "true" : "false");
                    break;
                default:
                    continue;
            }
            
            if (field_str) {
                if (jdbx_secondary_index_delete(secondary_indexes[i], field_str, doc_id) != 0) {
                    LOG_WARNING("Failed to remove from secondary index for field %s", index_fields[i]);
                }
                BUFFER_FREE(field_str);
            }
        }
    }
    
    return 0;
}

/* Rebuild index from scratch by scanning all documents */
int jdbx_rebuild_secondary_index(jdbx_btree_t* doc_tree, jdbx_btree_t* idx,
                                const char* field_name) {
    LOG_INFO("Rebuilding secondary index for field '%s'", field_name);
    
    int count = 0;
    jdbx_btree_iterator_t* iter = jdbx_btree_iterator_create(doc_tree);
    if (!iter) return -1;
    
    void* key = NULL;
    size_t key_len = 0;
    void* value = NULL;
    size_t value_len = 0;
    
    while (jdbx_btree_iterator_next(iter, &key, &key_len, &value, &value_len) == 0) {
        /* Parse document */
        json_value_t* doc = json_parse((char*)value);
        if (doc) {
            /* Extract field value */
            json_value_t* field_val = json_object_get(doc, field_name);
            if (field_val) {
                char* field_str = NULL;
                
                /* Convert to string */
                switch (field_val->type) {
                    case JSON_STRING:
                        field_str = BUFFER_STRDUP(json_get_string(field_val));
                        break;
                    case JSON_NUMBER:
                        field_str = BUFFER_ALLOC(32);
                        snprintf(field_str, 32, "%.6f", json_get_number(field_val));
                        break;
                    case JSON_BOOLEAN:
                        field_str = BUFFER_STRDUP(json_get_boolean(field_val) ? "true" : "false");
                        break;
                    default:
                        break;
                }
                
                if (field_str) {
                    /* Copy key as doc_id */
                    char doc_id[256];
                    if (key_len < sizeof(doc_id)) {
                        memcpy(doc_id, key, key_len);
                        doc_id[key_len] = '\0';
                        
                        if (jdbx_secondary_index_insert(idx, field_str, doc_id) == 0) {
                            count++;
                        }
                    }
                    BUFFER_FREE(field_str);
                }
            }
            /* CHECKPOINT: json_free(doc); */
        }
        
        BUFFER_FREE(key);
        BUFFER_FREE(value);
    }
    
    jdbx_btree_iterator_destroy(iter);
    
    LOG_INFO("Rebuilt secondary index for field '%s': %d entries", field_name, count);
    return count;
}