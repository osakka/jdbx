#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include "utils/memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>

/* Default number of hash buckets for indexes */
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

/* Resize the index hash table */
static int resize_index(index_t* index, size_t new_size) {
  if (!index || new_size == 0) {
    return 0;
  }

  /* Ensure new size is prime for better distribution */
  new_size = next_prime(new_size);

  /* Allocate new buckets */
  index_entry_t** new_buckets = (index_entry_t**)BUFFER_ALLOC(new_size * sizeof(index_entry_t*));
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
  BUFFER_FREE(index->buckets);

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

/* Get a JSON value at a specific path (dot notation) */
static json_value_t* get_json_value_at_path(json_value_t* root, const char* path) {
  if (!root || !path) {
    return NULL;
  }
  
  /* Create a copy of the path for tokenization */
  char* path_copy = BUFFER_STRDUP(path);
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
  
  BUFFER_FREE(path_copy);
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
      return BUFFER_STRDUP(value->value.string);

    case JSON_NUMBER:
      result = (char*)BUFFER_ALLOC(32);
      if (result) {
        snprintf(result, 32, "%.16g", value->value.number);
      }
      return result;

    case JSON_INTEGER:
      result = (char*)BUFFER_ALLOC(32);
      if (result) {
        snprintf(result, 32, "%lld", (long long)value->value.integer);
      }
      return result;

    case JSON_BOOLEAN:
      return BUFFER_STRDUP(value->value.boolean ? "true" : "false");

    case JSON_NULL:
      return BUFFER_STRDUP("null");

    case JSON_OBJECT:
    case JSON_ARRAY:
      /* For complex types, use the JSON representation */
      return json_stringify(value);

    default:
      return BUFFER_STRDUP("");
  }
}

/* Create a new index */
index_t* db_create_index_old(database_t* db, const char* collection, const char* name,
             const char* field_path, index_type_t type) {
  if (!db || !collection || !name || !field_path) {
    LOG_ERROR("create index: invalid params",
         db, collection ? collection : "NULL", name ? name : "NULL", field_path ? field_path : "NULL");
    return NULL;
  }

  LOG_INFO("Creating index '%s' on collection '%s', field '%s', type %d",
      name, collection, field_path, type);

  /* Get collection */
  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    LOG_ERROR("create index: Collection '%s' not found", collection);
    return NULL;
  }

  /* Lock collection */
  pthread_mutex_lock(&coll->lock);

  /* Check if index already exists */
  index_t* existing = coll->indexes;
  while (existing) {
    if (strcmp(existing->name, name) == 0) {
      LOG_INFO("Index '%s' already exists on collection '%s', returning existing index",
          name, collection);
      pthread_mutex_unlock(&coll->lock);
      return existing;
    }
    existing = existing->next;
  }
  
  /* Create new index */
  LOG_DEBUG("Allocating memory for new index.");
  index_t* index = (index_t*)BUFFER_ALLOC(sizeof(index_t));
  if (!index) {
    LOG_ERROR("Out of memory.");
    pthread_mutex_unlock(&coll->lock);
    return NULL;
  }
  memory_promote(index);  /* Index survives checkpoints */

  /* Initialize index fields */
  LOG_DEBUG("Initializing index fields.");
  index->name = BUFFER_STRDUP(name);
  index->field_path = BUFFER_STRDUP(field_path);
  if (index->name) memory_promote(index->name);  /* Part of index */
  if (index->field_path) memory_promote(index->field_path);  /* Part of index */
  index->type = type;
  index->entries = 0;
  index->next = NULL;

  /* Initialize hash buckets with a prime number for better distribution */
  index->num_buckets = next_prime(DEFAULT_INDEX_BUCKETS);
  LOG_DEBUG("Initializing hash buckets with prime number %zu", index->num_buckets);

  index->buckets = (index_entry_t**)BUFFER_ALLOC(index->num_buckets * sizeof(index_entry_t*));
  if (index->buckets) memory_promote(index->buckets);  /* Part of index */

  if (!index->buckets) {
    LOG_ERROR("Out of memory", index->num_buckets);
    BUFFER_FREE(index->name);
    BUFFER_FREE(index->field_path);
    BUFFER_FREE(index);
    pthread_mutex_unlock(&coll->lock);
    return NULL;
  }

  /* Initialize lock */
  LOG_DEBUG("Initializing index read-write lock.");
  pthread_rwlock_init(&index->lock, NULL);
  
  /* Add index to collection */
  LOG_DEBUG("Adding index to collection.");
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
  LOG_INFO("Building index from existing documents.");
  size_t doc_count = 0;
  size_t indexed_count = 0;

  if (coll->documents && coll->documents->type == JSON_ARRAY) {
    doc_count = json_array_size(coll->documents);
    LOG_DEBUG("Collection has %zu documents to index", doc_count);

    for (size_t i = 0; i < doc_count; i++) {
      json_value_t* doc = json_array_get(coll->documents, i);
      if (doc && doc->type == JSON_OBJECT) {
        /* Get document ID */
        json_value_t* id_val = json_object_get(doc, "id");
        if (id_val && id_val->type == JSON_STRING) {
          const char* doc_id = id_val->value.string;

          /* Get indexed field value */
          json_value_t* field_val = get_json_value_at_path(doc, field_path);
          if (field_val) {
            /* Convert field value to string */
            char* value_str = json_value_to_string(field_val);
            if (value_str) {
              /* Write lock index */
              pthread_rwlock_wrlock(&index->lock);

              /* Check for uniqueness if needed */
              if (type == INDEX_TYPE_UNIQUE) {
                /* Calculate hash */
                unsigned long hash = hash_string(value_str) % index->num_buckets;

                /* Check if value already exists */
                index_entry_t* entry = index->buckets[hash];
                while (entry) {
                  if (strcmp(entry->key_value, value_str) == 0) {
                    /* Duplicate value */
                    LOG_ERROR("Unique index violation: duplicate value '%s' for document ID '%s'",
                        value_str, doc_id);
                    BUFFER_FREE(value_str);
                    pthread_rwlock_unlock(&index->lock);

                    /* Cleanup and return error */
                    pthread_mutex_unlock(&coll->lock);
                    LOG_INFO("Dropping partially built index due to uniqueness violation.");
                    db_drop_index(db, collection, name);
                    return NULL;
                  }
                  entry = entry->next;
                }
              }
              
              /* Add to index */
              /* Calculate hash */
              unsigned long hash = hash_string(value_str) % index->num_buckets;
              
              /* Create new entry */
              TRACE_DB("Adding document %s with value '%s' to index", doc_id, value_str);
              index_entry_t* new_entry = (index_entry_t*)BUFFER_ALLOC(sizeof(index_entry_t));
              if (new_entry) {
                memory_promote(new_entry);  /* Index entry survives checkpoints */
                new_entry->document_id = BUFFER_STRDUP(doc_id);
                if (new_entry->document_id) memory_promote(new_entry->document_id);  /* Part of entry */
                new_entry->key_value = value_str;
                if (value_str) memory_promote(value_str);  /* Part of entry */
                new_entry->next = index->buckets[hash];
                index->buckets[hash] = new_entry;
                index->entries++;
                indexed_count++;

                /* Check if index needs resizing */
                check_and_resize_index(index);
              } else {
                LOG_ERROR("Out of memory.");
                BUFFER_FREE(value_str);
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

  /* Mark database as modified */
  pthread_rwlock_wrlock(&db->rwlock);
  db->is_modified = 1;
  pthread_rwlock_unlock(&db->rwlock);

  LOG_INFO("Index '%s' created with %zu/%zu documents indexed",
      name, indexed_count, doc_count);

  return index;
}

/* Free an index entry */
static void free_index_entry(index_entry_t* entry) {
  if (entry) {
    if (entry->document_id) {
      BUFFER_FREE(entry->document_id);
    }
    if (entry->key_value) {
      BUFFER_FREE(entry->key_value);
    }
    BUFFER_FREE(entry);
  }
}

/* Drop an index */
int db_drop_index_old(database_t* db, const char* collection, const char* name) {
  if (!db || !collection || !name) {
    LOG_ERROR("drop index: Invalid parameters (db: %p, collection: %s, name: %s)",
         db, collection ? collection : "NULL", name ? name : "NULL");
    return 0;
  }

  LOG_INFO("Dropping index '%s' from collection '%s'", name, collection);

  /* Get collection */
  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    LOG_ERROR("drop index: Collection '%s' not found", collection);
    return 0;
  }

  /* Lock collection */
  pthread_mutex_lock(&coll->lock);

  /* Find index */
  index_t* prev = NULL;
  index_t* current = coll->indexes;

  while (current) {
    if (strcmp(current->name, name) == 0) {
      LOG_DEBUG("Found index '%s', removing from collection", name);

      /* Found index, remove from list */
      if (prev) {
        prev->next = current->next;
      } else {
        coll->indexes = current->next;
      }

      /* Write lock index */
      pthread_rwlock_wrlock(&current->lock);

      /* Free all entries */
      LOG_DEBUG("Freeing index entries (total: %zu across %zu buckets)",
           current->entries, current->num_buckets);

      size_t freed_entries = 0;
      for (size_t i = 0; i < current->num_buckets; i++) {
        index_entry_t* entry = current->buckets[i];
        while (entry) {
          index_entry_t* next = entry->next;
          free_index_entry(entry);
          entry = next;
          freed_entries++;
        }
      }

      /* Free buckets */
      LOG_DEBUG("Freed %zu index entries, now freeing buckets", freed_entries);
      BUFFER_FREE(current->buckets);
      
      /* Unlock index */
      pthread_rwlock_unlock(&current->lock);

      /* Destroy lock */
      LOG_DEBUG("Destroying index read-write lock.");
      pthread_rwlock_destroy(&current->lock);

      /* Free index */
      LOG_DEBUG("Freeing index name and field path memory.");
      BUFFER_FREE(current->name);
      BUFFER_FREE(current->field_path);
      BUFFER_FREE(current);

      pthread_mutex_unlock(&coll->lock);

      /* Mark database as modified */
      pthread_rwlock_wrlock(&db->rwlock);
      db->is_modified = 1;
      pthread_rwlock_unlock(&db->rwlock);

      LOG_INFO("Index '%s' dropped", name);
      return 1;
    }

    prev = current;
    current = current->next;
  }

  pthread_mutex_unlock(&coll->lock);

  LOG_WARNING("Index '%s' not found in collection '%s'", name, collection);
  return 0;
}

/* Get an index by name */
index_t* db_get_index(database_t* db, const char* collection, const char* name) {
  if (!db || !collection || !name) {
    return NULL;
  }
  
  /* Get collection */
  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    return NULL;
  }
  
  /* Lock collection */
  pthread_mutex_lock(&coll->lock);
  
  /* Find index */
  index_t* index = coll->indexes;
  while (index) {
    if (strcmp(index->name, name) == 0) {
      pthread_mutex_unlock(&coll->lock);
      return index;
    }
    index = index->next;
  }
  
  pthread_mutex_unlock(&coll->lock);
  
  return NULL;
}

/* List all indexes for a collection */
json_value_t* db_list_indexes_old(database_t* db, const char* collection) {
  if (!db || !collection) {
    return NULL;
  }
  
  /* Get collection */
  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    return NULL;
  }
  
  /* Lock collection */
  pthread_mutex_lock(&coll->lock);
  
  /* Create indexes array */
  json_value_t* indexes = json_create_array();
  if (!indexes) {
    pthread_mutex_unlock(&coll->lock);
    return NULL;
  }
  
  /* Add each index */
  index_t* index = coll->indexes;
  while (index) {
    /* Create index object */
    json_value_t* index_obj = json_create_object();
    if (index_obj) {
      json_object_set(index_obj, "name", json_create_string(index->name));
      json_object_set(index_obj, "field", json_create_string(index->field_path));
      
      /* Set type */
      const char* type_str = "unknown";
      switch (index->type) {
        case INDEX_TYPE_UNIQUE:
          type_str = "unique";
          break;
        case INDEX_TYPE_NON_UNIQUE:
          type_str = "non_unique";
          break;
        case INDEX_TYPE_TEXT:
          type_str = "text";
          break;
        case INDEX_TYPE_GEO:
          type_str = "geo";
          break;
      }
      
      json_object_set(index_obj, "type", json_create_string(type_str));
      
      /* Read lock index to get entry count */
      pthread_rwlock_rdlock(&index->lock);
      json_object_set(index_obj, "entries", json_create_integer(index->entries));
      pthread_rwlock_unlock(&index->lock);
      
      /* Add to array */
      json_array_append(indexes, index_obj);
    }
    
    index = index->next;
  }
  
  pthread_mutex_unlock(&coll->lock);
  
  return indexes;
}

/* Rebuild an index */
int db_rebuild_index(database_t* db, const char* collection, const char* name) {
  if (!db || !collection || !name) {
    return 0;
  }
  
  /* Get index */
  index_t* index = db_get_index(db, collection, name);
  if (!index) {
    return 0;
  }
  
  /* Get collection */
  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    return 0;
  }
  
  /* Write lock the index */
  pthread_rwlock_wrlock(&index->lock);
  
  /* Clear all entries */
  for (size_t i = 0; i < index->num_buckets; i++) {
    index_entry_t* entry = index->buckets[i];
    while (entry) {
      index_entry_t* next = entry->next;
      free_index_entry(entry);
      entry = next;
    }
    index->buckets[i] = NULL;
  }
  
  index->entries = 0;
  
  /* Lock collection */
  pthread_mutex_lock(&coll->lock);
  
  /* Rebuild index */
  if (coll->documents && coll->documents->type == JSON_ARRAY) {
    for (size_t i = 0; i < json_array_size(coll->documents); i++) {
      json_value_t* doc = json_array_get(coll->documents, i);
      if (doc && doc->type == JSON_OBJECT) {
        /* Get document ID */
        json_value_t* id_val = json_object_get(doc, "id");
        if (id_val && id_val->type == JSON_STRING) {
          const char* doc_id = id_val->value.string;
          
          /* Get indexed field value */
          json_value_t* field_val = get_json_value_at_path(doc, index->field_path);
          if (field_val) {
            /* Convert field value to string */
            char* value_str = json_value_to_string(field_val);
            if (value_str) {
              /* Check for uniqueness if needed */
              if (index->type == INDEX_TYPE_UNIQUE) {
                /* Calculate hash */
                unsigned long hash = hash_string(value_str) % index->num_buckets;
                
                /* Check if value already exists */
                index_entry_t* entry = index->buckets[hash];
                while (entry) {
                  if (strcmp(entry->key_value, value_str) == 0) {
                    /* Duplicate value */
                    BUFFER_FREE(value_str);
                    pthread_rwlock_unlock(&index->lock);
                    pthread_mutex_unlock(&coll->lock);
                    return 0;
                  }
                  entry = entry->next;
                }
              }
              
              /* Add to index */
              /* Calculate hash */
              unsigned long hash = hash_string(value_str) % index->num_buckets;
              
              /* Create new entry */
              index_entry_t* new_entry = (index_entry_t*)BUFFER_ALLOC(sizeof(index_entry_t));
              if (new_entry) {
                memory_promote(new_entry);  /* Index entry survives checkpoints */
                new_entry->document_id = BUFFER_STRDUP(doc_id);
                if (new_entry->document_id) memory_promote(new_entry->document_id);  /* Part of entry */
                new_entry->key_value = value_str;
                if (value_str) memory_promote(value_str);  /* Part of entry */
                new_entry->next = index->buckets[hash];
                index->buckets[hash] = new_entry;
                index->entries++;

                /* Check if index needs resizing */
                check_and_resize_index(index);
              } else {
                BUFFER_FREE(value_str);
              }
            }
          }
        }
      }
    }
  }
  
  pthread_mutex_unlock(&coll->lock);
  pthread_rwlock_unlock(&index->lock);
  
  return 1;
}

/* Rebuild all indexes for a collection */
int db_rebuild_all_indexes(database_t* db, const char* collection) {
  if (!db || !collection) {
    return 0;
  }
  
  /* Get collection */
  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    return 0;
  }
  
  /* Lock collection */
  pthread_mutex_lock(&coll->lock);
  
  /* Rebuild each index */
  index_t* index = coll->indexes;
  while (index) {
    /* Unlock collection before rebuilding each index */
    pthread_mutex_unlock(&coll->lock);
    
    /* Rebuild index */
    db_rebuild_index(db, collection, index->name);
    
    /* Lock collection again to continue iteration */
    pthread_mutex_lock(&coll->lock);
    
    index = index->next;
  }
  
  pthread_mutex_unlock(&coll->lock);
  
  return 1;
}

/* Query documents using an index */
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
  
  /* Lock collection */
  pthread_mutex_lock(&coll->lock);
  
  /* Find index for the field */
  index_t* index = coll->indexes;
  while (index) {
    if (strcmp(index->field_path, field_path) == 0) {
      break;
    }
    index = index->next;
  }
  
  /* Return empty result if no index found */
  if (!index) {
    /* Fall back to full collection scan */
    json_value_t* results = json_create_array();
    
    if (!results) {
      pthread_mutex_unlock(&coll->lock);
      return NULL;
    }
    
    /* Linear scan of all documents */
    if (coll->documents && coll->documents->type == JSON_ARRAY) {
      /* Apply skip and limit */
      int matched = 0;
      int added = 0;
      
      for (size_t i = 0; i < json_array_size(coll->documents) && (limit <= 0 || added < limit); i++) {
        json_value_t* doc = json_array_get(coll->documents, i);
        if (doc && doc->type == JSON_OBJECT) {
          /* Get indexed field value */
          json_value_t* field_val = get_json_value_at_path(doc, field_path);
          if (field_val) {
            /* Convert field value to string */
            char* value_str = json_value_to_string(field_val);
            if (value_str) {
              /* Compare values */
              if (strcmp(value_str, value) == 0) {
                /* Match found */
                matched++;
                
                /* Apply skip */
                if (matched > skip) {
                  /* Add document to results */
                  json_array_append(results, json_clone(doc));
                  added++;
                }
              }
              
              BUFFER_FREE(value_str);
            }
          }
        }
      }
    }
    
    pthread_mutex_unlock(&coll->lock);
    return results;
  }
  
  /* Read lock the index */
  pthread_rwlock_rdlock(&index->lock);
  
  /* Create results array */
  json_value_t* results = json_create_array();
  if (!results) {
    pthread_rwlock_unlock(&index->lock);
    pthread_mutex_unlock(&coll->lock);
    return NULL;
  }
  
  /* Calculate hash */
  unsigned long hash = hash_string(value) % index->num_buckets;
  
  /* Search the bucket */
  index_entry_t* entry = index->buckets[hash];
  
  /* Apply skip and limit */
  int matched = 0;
  int added = 0;
  
  while (entry && (limit <= 0 || added < limit)) {
    /* Value matches */
    if (strcmp(entry->key_value, value) == 0) {
      matched++;
      
      /* Apply skip */
      if (matched > skip) {
        /* Find document */
        if (coll->documents && coll->documents->type == JSON_ARRAY) {
          for (size_t i = 0; i < json_array_size(coll->documents); i++) {
            json_value_t* doc = json_array_get(coll->documents, i);
            if (doc && doc->type == JSON_OBJECT) {
              /* Check document ID */
              json_value_t* id_val = json_object_get(doc, "id");
              if (id_val && id_val->type == JSON_STRING && 
                strcmp(id_val->value.string, entry->document_id) == 0) {
                /* Add document to results */
                json_array_append(results, json_clone(doc));
                added++;
                break;
              }
            }
          }
        }
      }
    }
    
    entry = entry->next;
  }
  
  pthread_rwlock_unlock(&index->lock);
  pthread_mutex_unlock(&coll->lock);
  
  return results;
}

/* Update indexes for a document */
void db_update_indexes_for_document(database_t* db, const char* collection, 
                  const char* document_id, json_value_t* document) {
  if (!db || !collection || !document_id || !document) {
    return;
  }
  
  /* Get collection */
  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    return;
  }
  
  /* Lock collection */
  pthread_mutex_lock(&coll->lock);
  
  /* Remove document from all indexes first */
  db_remove_document_from_indexes(db, collection, document_id);
  
  /* Update each index */
  index_t* index = coll->indexes;
  while (index) {
    /* Get indexed field value */
    json_value_t* field_val = get_json_value_at_path(document, index->field_path);
    if (field_val) {
      /* Convert field value to string */
      char* value_str = json_value_to_string(field_val);
      if (value_str) {
        /* Write lock index */
        pthread_rwlock_wrlock(&index->lock);
        
        /* Check for uniqueness if needed */
        if (index->type == INDEX_TYPE_UNIQUE) {
          /* Calculate hash */
          unsigned long hash = hash_string(value_str) % index->num_buckets;
          
          /* Check if value already exists */
          index_entry_t* entry = index->buckets[hash];
          while (entry) {
            if (strcmp(entry->key_value, value_str) == 0) {
              /* Duplicate value */
              BUFFER_FREE(value_str);
              pthread_rwlock_unlock(&index->lock);
              pthread_mutex_unlock(&coll->lock);
              return;
            }
            entry = entry->next;
          }
        }
        
        /* Add to index */
        /* Calculate hash */
        unsigned long hash = hash_string(value_str) % index->num_buckets;
        
        /* Create new entry */
        index_entry_t* new_entry = (index_entry_t*)BUFFER_ALLOC(sizeof(index_entry_t));
        if (new_entry) {
          memory_promote(new_entry);  /* Index entry survives checkpoints */
          new_entry->document_id = BUFFER_STRDUP(document_id);
          if (new_entry->document_id) memory_promote(new_entry->document_id);  /* Part of entry */
          new_entry->key_value = value_str;
          if (value_str) memory_promote(value_str);  /* Part of entry */
          new_entry->next = index->buckets[hash];
          index->buckets[hash] = new_entry;
          index->entries++;

          /* Check if index needs resizing */
          check_and_resize_index(index);
        } else {
          BUFFER_FREE(value_str);
        }

        /* Unlock index */
        pthread_rwlock_unlock(&index->lock);
      }
    }
    
    index = index->next;
  }
  
  pthread_mutex_unlock(&coll->lock);
}

/* Remove a document from all indexes */
void db_remove_document_from_indexes(database_t* db, const char* collection, 
                  const char* document_id) {
  if (!db || !collection || !document_id) {
    return;
  }
  
  /* Get collection */
  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    return;
  }
  
  /* Lock collection */
  pthread_mutex_lock(&coll->lock);
  
  /* Remove from each index */
  index_t* index = coll->indexes;
  while (index) {
    /* Write lock index */
    pthread_rwlock_wrlock(&index->lock);
    
    /* Remove from all buckets */
    for (size_t i = 0; i < index->num_buckets; i++) {
      index_entry_t* prev = NULL;
      index_entry_t* entry = index->buckets[i];
      
      while (entry) {
        if (strcmp(entry->document_id, document_id) == 0) {
          /* Remove entry */
          if (prev) {
            prev->next = entry->next;
          } else {
            index->buckets[i] = entry->next;
          }
          
          /* Free entry */
          free_index_entry(entry);
          
          /* Decrement count */
          index->entries--;

          /* Check if index needs resizing */
          check_and_resize_index(index);

          break;
        }
        
        prev = entry;
        entry = entry->next;
      }
    }
    
    /* Unlock index */
    pthread_rwlock_unlock(&index->lock);
    
    index = index->next;
  }
  
  pthread_mutex_unlock(&coll->lock);
}

/* Convert index to JSON */
json_value_t* db_index_to_json(index_t* index) {
  if (!index) {
    return NULL;
  }
  
  /* Create index object */
  json_value_t* index_obj = json_create_object();
  if (!index_obj) {
    return NULL;
  }
  
  /* Set basic properties */
  json_object_set(index_obj, "name", json_create_string(index->name));
  json_object_set(index_obj, "field", json_create_string(index->field_path));
  
  /* Set type */
  const char* type_str = "unknown";
  switch (index->type) {
    case INDEX_TYPE_UNIQUE:
      type_str = "unique";
      break;
    case INDEX_TYPE_NON_UNIQUE:
      type_str = "non_unique";
      break;
    case INDEX_TYPE_TEXT:
      type_str = "text";
      break;
    case INDEX_TYPE_GEO:
      type_str = "geo";
      break;
  }
  
  json_object_set(index_obj, "type", json_create_string(type_str));
  
  /* Read lock index */
  pthread_rwlock_rdlock(&index->lock);
  
  /* Add statistics */
  json_object_set(index_obj, "entries", json_create_integer(index->entries));
  json_object_set(index_obj, "buckets", json_create_integer(index->num_buckets));
  
  /* Create entries array */
  json_value_t* entries = json_create_array();
  if (entries) {
    /* Sample some entries */
    int sample_count = 0;
    int max_samples = 100; /* Limit samples for large indexes */
    
    for (size_t i = 0; i < index->num_buckets && sample_count < max_samples; i++) {
      index_entry_t* entry = index->buckets[i];
      while (entry && sample_count < max_samples) {
        /* Create entry object */
        json_value_t* entry_obj = json_create_object();
        if (entry_obj) {
          json_object_set(entry_obj, "document_id", json_create_string(entry->document_id));
          json_object_set(entry_obj, "value", json_create_string(entry->key_value));
          json_array_append(entries, entry_obj);
        }
        
        sample_count++;
        entry = entry->next;
      }
    }
    
    json_object_set(index_obj, "sample_entries", entries);
  }
  
  pthread_rwlock_unlock(&index->lock);
  
  return index_obj;
}

/* Get statistics for an index */
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

  /* For histogram of chain lengths */
  json_value_t* histogram = json_create_object();
  int chain_lengths[10] = {0}; /* Count chains of length 0-9+ */

  for (size_t i = 0; i < index->num_buckets; i++) {
    if (!index->buckets[i]) {
      empty_buckets++;
      chain_lengths[0]++;
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

      /* Update histogram */
      if (chain_length >= 9) {
        chain_lengths[9]++;
      } else {
        chain_lengths[chain_length]++;
      }
    }
  }

  json_object_set(stats, "empty_buckets", json_create_integer(empty_buckets));
  json_object_set(stats, "max_chain_length", json_create_integer(max_chain_length));

  /* Calculate average chain length */
  double avg_chain_length = num_chains > 0 ? (double)total_chain_length / num_chains : 0;
  char avg_chain_str[32];
  snprintf(avg_chain_str, sizeof(avg_chain_str), "%.2f", avg_chain_length);
  json_object_set(stats, "avg_chain_length", json_create_string(avg_chain_str));

  /* Add chain length histogram */
  for (int i = 0; i < 9; i++) {
    char key[8];
    snprintf(key, sizeof(key), "len_%d", i);
    json_object_set(histogram, key, json_create_integer(chain_lengths[i]));
  }
  json_object_set(histogram, "len_9plus", json_create_integer(chain_lengths[9]));
  json_object_set(stats, "chain_length_histogram", histogram);

  /* Calculate collision rate */
  double collision_rate = num_chains > 0 ?
    (double)(total_chain_length - num_chains) / total_chain_length : 0;
  char collision_rate_str[32];
  snprintf(collision_rate_str, sizeof(collision_rate_str), "%.2f", collision_rate);
  json_object_set(stats, "collision_rate", json_create_string(collision_rate_str));

  /* Calculate expected vs actual performance */
  double optimal_lookups = 1.0;
  double actual_lookups = num_chains > 0 ?
    (double)total_chain_length / (total_chain_length - empty_buckets) : 1.0;
  json_object_set(stats, "optimal_average_lookups", json_create_number(optimal_lookups));

  char actual_lookups_str[32];
  snprintf(actual_lookups_str, sizeof(actual_lookups_str), "%.2f", actual_lookups);
  json_object_set(stats, "actual_average_lookups", json_create_string(actual_lookups_str));

  /* Performance rating */
  const char* performance_rating = "excellent";
  if (actual_lookups > 2.0) {
    performance_rating = "poor";
  } else if (actual_lookups > 1.5) {
    performance_rating = "fair";
  } else if (actual_lookups > 1.2) {
    performance_rating = "good";
  }
  json_object_set(stats, "performance_rating", json_create_string(performance_rating));

  /* Unlock index and collection */
  pthread_rwlock_unlock(&index->lock);
  pthread_mutex_unlock(&coll->lock);

  return stats;
}