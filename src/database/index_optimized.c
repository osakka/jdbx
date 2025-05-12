#include "database/database.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <math.h>

/* Initial and maximum number of hash buckets for indexes */
#define DEFAULT_INDEX_BUCKETS 128
#define MAX_INDEX_BUCKETS (1024 * 1024) /* 1M buckets max */

/* Load factors for resizing */
#define MAX_LOAD_FACTOR 0.75
#define MIN_LOAD_FACTOR 0.25
#define RESIZE_FACTOR 2

/* Improved hash function for index keys (FNV-1a algorithm) */
static uint32_t hash_string(const char* str) {
    uint32_t hash = 2166136261u; /* FNV offset basis */
    
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619; /* FNV prime */
    }
    
    return hash;
}

/* Check if a number is prime */
static int is_prime(size_t n) {
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;
    
    size_t i = 5;
    while (i * i <= n) {
        if (n % i == 0 || n % (i + 2) == 0) return 0;
        i += 6;
    }
    
    return 1;
}

/* Find the next prime number >= n */
static size_t next_prime(size_t n) {
    if (n <= 2) return 2;
    
    /* Make sure n is odd */
    if (n % 2 == 0) n++;
    
    /* Find the next prime */
    while (!is_prime(n)) {
        n += 2;
    }
    
    return n;
}

/* Get a JSON value at a specific path (dot notation) */
static json_value_t* get_json_value_at_path(json_value_t* root, const char* path) {
    if (!root || !path) {
        return NULL;
    }
    
    /* Create a copy of the path for tokenization */
    char* path_copy = strdup(path);
    if (!path_copy) {
        return NULL;
    }
    
    /* Tokenize the path */
    char* token = strtok(path_copy, ".");
    json_value_t* current = root;
    
    while (token && current) {
        if (current->type == JSON_OBJECT) {
            current = json_object_get(current, token);
        } else if (current->type == JSON_ARRAY) {
            /* Check if token is a numeric index */
            char* endptr;
            long index = strtol(token, &endptr, 10);
            if (*endptr == '\0' && index >= 0) {
                current = json_array_get(current, index);
            } else {
                current = NULL;
            }
        } else {
            current = NULL;
        }
        
        token = strtok(NULL, ".");
    }
    
    free(path_copy);
    return current;
}

/* Convert a JSON value to a string for indexing */
static char* json_value_to_string(json_value_t* value) {
    if (!value) {
        return NULL;
    }
    
    char* result = NULL;
    
    switch (value->type) {
        case JSON_STRING:
            return strdup(value->value.string);
            
        case JSON_NUMBER:
            result = (char*)malloc(32);
            if (result) {
                snprintf(result, 32, "%.16g", value->value.number);
            }
            return result;
            
        case JSON_INTEGER:
            result = (char*)malloc(32);
            if (result) {
                snprintf(result, 32, "%lld", (long long)value->value.integer);
            }
            return result;
            
        case JSON_BOOLEAN:
            return strdup(value->value.boolean ? "true" : "false");
            
        case JSON_NULL:
            return strdup("null");
            
        case JSON_OBJECT:
        case JSON_ARRAY:
            /* For complex types, use the JSON representation */
            return json_stringify(value);
            
        default:
            return NULL;
    }
}

/* Resize the index hash table */
static int resize_index(index_t* index, size_t new_size) {
    if (!index || new_size == 0) {
        return 0;
    }
    
    /* Ensure new size is prime for better distribution */
    new_size = next_prime(new_size);
    
    /* Allocate new buckets */
    index_entry_t** new_buckets = (index_entry_t**)calloc(new_size, sizeof(index_entry_t*));
    if (!new_buckets) {
        return 0;
    }
    
    /* Rehash all entries */
    for (size_t i = 0; i < index->num_buckets; i++) {
        index_entry_t* entry = index->buckets[i];
        
        while (entry) {
            /* Save next entry */
            index_entry_t* next = entry->next;
            
            /* Rehash entry */
            uint32_t hash = hash_string(entry->key_value) % new_size;
            
            /* Insert at the beginning of the new bucket */
            entry->next = new_buckets[hash];
            new_buckets[hash] = entry;
            
            /* Move to next entry */
            entry = next;
        }
    }
    
    /* Free old buckets array (not the entries) */
    free(index->buckets);
    
    /* Update index with new buckets */
    index->buckets = new_buckets;
    index->num_buckets = new_size;
    
    return 1;
}

/* Check if index needs resizing and resize if necessary */
static void check_and_resize_index(index_t* index) {
    if (!index || !index->buckets) {
        return;
    }
    
    double load_factor = (double)index->entries / index->num_buckets;
    
    /* Check if we need to grow */
    if (load_factor > MAX_LOAD_FACTOR && index->num_buckets < MAX_INDEX_BUCKETS) {
        size_t new_size = index->num_buckets * RESIZE_FACTOR;
        if (new_size > MAX_INDEX_BUCKETS) {
            new_size = MAX_INDEX_BUCKETS;
        }
        resize_index(index, new_size);
    }
    /* Check if we need to shrink */
    else if (load_factor < MIN_LOAD_FACTOR && index->num_buckets > DEFAULT_INDEX_BUCKETS) {
        size_t new_size = index->num_buckets / RESIZE_FACTOR;
        if (new_size < DEFAULT_INDEX_BUCKETS) {
            new_size = DEFAULT_INDEX_BUCKETS;
        }
        resize_index(index, new_size);
    }
}

/* Create a new index */
index_t* db_create_index(database_t* db, const char* collection, const char* name, 
                       const char* field_path, index_type_t type) {
    if (!db || !collection || !name || !field_path) {
        return NULL;
    }
    
    db_collection_t* coll = db_get_collection(db, collection);
    if (!coll) {
        return NULL;
    }
    
    /* Lock collection */
    pthread_mutex_lock(&coll->lock);
    
    /* Check if index already exists */
    index_t* existing = coll->indexes;
    while (existing) {
        if (strcmp(existing->name, name) == 0) {
            pthread_mutex_unlock(&coll->lock);
            return existing;
        }
        existing = existing->next;
    }
    
    /* Create new index */
    index_t* index = (index_t*)malloc(sizeof(index_t));
    if (!index) {
        pthread_mutex_unlock(&coll->lock);
        return NULL;
    }
    
    /* Initialize index fields */
    index->name = strdup(name);
    index->field_path = strdup(field_path);
    index->type = type;
    index->entries = 0;
    index->next = NULL;
    
    /* Initialize hash buckets with a prime number for better distribution */
    index->num_buckets = next_prime(DEFAULT_INDEX_BUCKETS);
    index->buckets = (index_entry_t**)calloc(index->num_buckets, sizeof(index_entry_t*));
    
    if (!index->buckets) {
        free(index->name);
        free(index->field_path);
        free(index);
        pthread_mutex_unlock(&coll->lock);
        return NULL;
    }
    
    /* Initialize lock */
    pthread_rwlock_init(&index->lock, NULL);
    
    /* Add index to collection */
    if (!coll->indexes) {
        coll->indexes = index;
    } else {
        index_t* last = coll->indexes;
        while (last->next) {
            last = last->next;
        }
        last->next = index;
    }
    
    /* Build index */
    if (coll->documents && coll->documents->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(coll->documents); i++) {
            json_value_t* doc = json_array_get(coll->documents, i);
            if (doc && doc->type == JSON_OBJECT) {
                /* Get document ID */
                json_value_t* id_val = json_object_get(doc, "id");
                if (id_val && id_val->type == JSON_STRING) {
                    const char* doc_id = id_val->value.string;
                    
                    /* Get indexed field value */
                    json_value_t* field_val = get_json_value_at_path(doc, field_path);
                    
                    if (field_val) {
                        /* Convert value to string for indexing */
                        char* value_str = json_value_to_string(field_val);
                        
                        if (value_str) {
                            /* Lock index for writing */
                            pthread_rwlock_wrlock(&index->lock);
                            
                            /* Check for uniqueness if required */
                            if (type == INDEX_TYPE_UNIQUE) {
                                /* Check if value already exists */
                                uint32_t hash = hash_string(value_str) % index->num_buckets;
                                index_entry_t* entry = index->buckets[hash];
                                
                                while (entry) {
                                    if (strcmp(entry->key_value, value_str) == 0) {
                                        /* Value exists, which violates uniqueness */
                                        free(value_str);
                                        pthread_rwlock_unlock(&index->lock);
                                        
                                        /* Clean up and return error */
                                        db_drop_index(db, collection, name);
                                        pthread_mutex_unlock(&coll->lock);
                                        return NULL;
                                    }
                                    
                                    entry = entry->next;
                                }
                            }
                            
                            /* Add to index */
                            /* Calculate hash */
                            uint32_t hash = hash_string(value_str) % index->num_buckets;
                            
                            /* Create new entry */
                            index_entry_t* new_entry = (index_entry_t*)malloc(sizeof(index_entry_t));
                            if (new_entry) {
                                new_entry->document_id = strdup(doc_id);
                                new_entry->key_value = value_str;
                                new_entry->next = index->buckets[hash];
                                index->buckets[hash] = new_entry;
                                index->entries++;
                                
                                /* Check if index needs resizing */
                                check_and_resize_index(index);
                            } else {
                                free(value_str);
                            }
                            
                            /* Unlock index */
                            pthread_rwlock_unlock(&index->lock);
                        }
                    }
                }
            }
        }
    }
    
    pthread_mutex_unlock(&coll->lock);
    return index;
}

/* Add a document to an index */
int db_index_add_document(index_t* index, const char* doc_id, json_value_t* document) {
    if (!index || !doc_id || !document) {
        return 0;
    }
    
    /* Get indexed field value */
    json_value_t* field_val = get_json_value_at_path(document, index->field_path);
    if (!field_val) {
        return 0; /* Field not found */
    }
    
    /* Convert value to string for indexing */
    char* value_str = json_value_to_string(field_val);
    if (!value_str) {
        return 0; /* Could not convert to string */
    }
    
    /* Lock index for writing */
    pthread_rwlock_wrlock(&index->lock);
    
    /* Check for uniqueness if required */
    if (index->type == INDEX_TYPE_UNIQUE) {
        /* Calculate hash */
        uint32_t hash = hash_string(value_str) % index->num_buckets;
        
        /* Check if value already exists */
        index_entry_t* entry = index->buckets[hash];
        while (entry) {
            if (strcmp(entry->key_value, value_str) == 0) {
                /* Value exists, which violates uniqueness */
                free(value_str);
                pthread_rwlock_unlock(&index->lock);
                return 0;
            }
            entry = entry->next;
        }
    }
    
    /* Add to index */
    /* Calculate hash */
    uint32_t hash = hash_string(value_str) % index->num_buckets;
    
    /* Create new entry */
    index_entry_t* new_entry = (index_entry_t*)malloc(sizeof(index_entry_t));
    if (!new_entry) {
        free(value_str);
        pthread_rwlock_unlock(&index->lock);
        return 0;
    }
    
    /* Initialize entry */
    new_entry->document_id = strdup(doc_id);
    new_entry->key_value = value_str;
    new_entry->next = index->buckets[hash];
    index->buckets[hash] = new_entry;
    index->entries++;
    
    /* Check if index needs resizing */
    check_and_resize_index(index);
    
    /* Unlock index */
    pthread_rwlock_unlock(&index->lock);
    
    return 1;
}

/* Remove a document from an index */
int db_index_remove_document(index_t* index, const char* doc_id) {
    if (!index || !doc_id) {
        return 0;
    }
    
    /* Lock index for writing */
    pthread_rwlock_wrlock(&index->lock);
    
    int removed = 0;
    
    /* Search for document in all buckets */
    for (size_t i = 0; i < index->num_buckets; i++) {
        index_entry_t** prev = &index->buckets[i];
        index_entry_t* entry = *prev;
        
        while (entry) {
            if (strcmp(entry->document_id, doc_id) == 0) {
                /* Remove entry */
                *prev = entry->next;
                
                /* Free entry */
                free(entry->document_id);
                free(entry->key_value);
                free(entry);
                
                index->entries--;
                removed = 1;
                
                /* Continue to check for other entries with this doc_id */
                entry = *prev;
            } else {
                prev = &entry->next;
                entry = entry->next;
            }
        }
    }
    
    /* Check if index needs resizing */
    check_and_resize_index(index);
    
    /* Unlock index */
    pthread_rwlock_unlock(&index->lock);
    
    return removed;
}

/* Query documents by indexed field */
json_value_t* db_query_by_index(database_t* db, const char* collection, const char* field_path, 
                              const char* value, int limit, int skip) {
    if (!db || !collection || !field_path || !value) {
        return NULL;
    }
    
    /* Get collection */
    db_collection_t* coll = db_get_collection(db, collection);
    if (!coll) {
        return NULL;
    }
    
    /* Lock collection for reading */
    pthread_mutex_lock(&coll->lock);
    
    /* Find index for field path */
    index_t* index = NULL;
    index_t* current = coll->indexes;
    
    while (current) {
        if (strcmp(current->field_path, field_path) == 0) {
            index = current;
            break;
        }
        current = current->next;
    }
    
    /* If no index exists, fall back to full collection scan */
    if (!index) {
        /* Create result array */
        json_value_t* result = json_create_array();
        if (!result) {
            pthread_mutex_unlock(&coll->lock);
            return NULL;
        }
        
        /* Scan all documents */
        if (coll->documents && coll->documents->type == JSON_ARRAY) {
            int count = 0;
            int skipped = 0;
            
            for (size_t i = 0; i < json_array_size(coll->documents); i++) {
                json_value_t* doc = json_array_get(coll->documents, i);
                if (doc && doc->type == JSON_OBJECT) {
                    /* Get field value */
                    json_value_t* field_val = get_json_value_at_path(doc, field_path);
                    
                    if (field_val) {
                        /* Convert to string for comparison */
                        char* doc_val = json_value_to_string(field_val);
                        
                        if (doc_val) {
                            if (strcmp(doc_val, value) == 0) {
                                /* Match found */
                                if (skipped < skip) {
                                    skipped++;
                                } else {
                                    json_array_append(result, json_clone(doc));
                                    count++;
                                    
                                    if (limit > 0 && count >= limit) {
                                        free(doc_val);
                                        break;
                                    }
                                }
                            }
                            
                            free(doc_val);
                        }
                    }
                }
            }
        }
        
        pthread_mutex_unlock(&coll->lock);
        return result;
    }
    
    /* Use index for query */
    pthread_rwlock_rdlock(&index->lock);
    
    /* Create result array */
    json_value_t* result = json_create_array();
    if (!result) {
        pthread_rwlock_unlock(&index->lock);
        pthread_mutex_unlock(&coll->lock);
        return NULL;
    }
    
    /* Calculate hash for the value */
    uint32_t hash = hash_string(value) % index->num_buckets;
    
    /* Find matching entries */
    index_entry_t* entry = index->buckets[hash];
    int count = 0;
    int skipped = 0;
    
    while (entry) {
        if (strcmp(entry->key_value, value) == 0) {
            /* Match found */
            if (skipped < skip) {
                skipped++;
            } else {
                /* Get document by ID */
                if (coll->documents && coll->documents->type == JSON_ARRAY) {
                    for (size_t i = 0; i < json_array_size(coll->documents); i++) {
                        json_value_t* doc = json_array_get(coll->documents, i);
                        if (doc && doc->type == JSON_OBJECT) {
                            json_value_t* id_val = json_object_get(doc, "id");
                            if (id_val && id_val->type == JSON_STRING && 
                                strcmp(id_val->value.string, entry->document_id) == 0) {
                                json_array_append(result, json_clone(doc));
                                count++;
                                break;
                            }
                        }
                    }
                }
                
                if (limit > 0 && count >= limit) {
                    break;
                }
            }
        }
        
        entry = entry->next;
    }
    
    /* Unlock index and collection */
    pthread_rwlock_unlock(&index->lock);
    pthread_mutex_unlock(&coll->lock);
    
    return result;
}

/* Get index statistics */
json_value_t* db_index_stats(database_t* db, const char* collection, const char* name) {
    if (!db || !collection || !name) {
        return NULL;
    }
    
    /* Get collection */
    db_collection_t* coll = db_get_collection(db, collection);
    if (!coll) {
        return NULL;
    }
    
    /* Lock collection for reading */
    pthread_mutex_lock(&coll->lock);
    
    /* Find index */
    index_t* index = NULL;
    index_t* current = coll->indexes;
    
    while (current) {
        if (strcmp(current->name, name) == 0) {
            index = current;
            break;
        }
        current = current->next;
    }
    
    if (!index) {
        pthread_mutex_unlock(&coll->lock);
        return NULL;
    }
    
    /* Lock index for reading */
    pthread_rwlock_rdlock(&index->lock);
    
    /* Create stats object */
    json_value_t* stats = json_create_object();
    if (!stats) {
        pthread_rwlock_unlock(&index->lock);
        pthread_mutex_unlock(&coll->lock);
        return NULL;
    }
    
    /* Basic stats */
    json_object_set(stats, "name", json_create_string(index->name));
    json_object_set(stats, "field_path", json_create_string(index->field_path));
    json_object_set(stats, "type", json_create_string(
        index->type == INDEX_TYPE_UNIQUE ? "unique" :
        index->type == INDEX_TYPE_NON_UNIQUE ? "non_unique" :
        index->type == INDEX_TYPE_TEXT ? "text" :
        index->type == INDEX_TYPE_GEO ? "geo" : "unknown"));
    json_object_set(stats, "entries", json_create_integer(index->entries));
    json_object_set(stats, "buckets", json_create_integer(index->num_buckets));
    
    /* Calculate load factor */
    double load_factor = (double)index->entries / index->num_buckets;
    char load_factor_str[32];
    snprintf(load_factor_str, sizeof(load_factor_str), "%.2f", load_factor);
    json_object_set(stats, "load_factor", json_create_string(load_factor_str));
    
    /* Calculate additional stats */
    int empty_buckets = 0;
    int max_chain_length = 0;
    int total_chain_length = 0;
    int num_chains = 0;
    
    for (size_t i = 0; i < index->num_buckets; i++) {
        if (!index->buckets[i]) {
            empty_buckets++;
        } else {
            int chain_length = 0;
            index_entry_t* entry = index->buckets[i];
            
            while (entry) {
                chain_length++;
                entry = entry->next;
            }
            
            if (chain_length > max_chain_length) {
                max_chain_length = chain_length;
            }
            
            total_chain_length += chain_length;
            num_chains++;
        }
    }
    
    json_object_set(stats, "empty_buckets", json_create_integer(empty_buckets));
    json_object_set(stats, "max_chain_length", json_create_integer(max_chain_length));
    
    /* Calculate average chain length */
    double avg_chain_length = num_chains > 0 ? (double)total_chain_length / num_chains : 0;
    char avg_chain_str[32];
    snprintf(avg_chain_str, sizeof(avg_chain_str), "%.2f", avg_chain_length);
    json_object_set(stats, "avg_chain_length", json_create_string(avg_chain_str));
    
    /* Unlock index and collection */
    pthread_rwlock_unlock(&index->lock);
    pthread_mutex_unlock(&coll->lock);
    
    return stats;
}