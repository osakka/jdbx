#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <limits.h> /* For SIZE_MAX */

/* Enhanced index configuration */
#define DEFAULT_INDEX_BUCKETS 256    /* Increased from 128 for better initial distribution */
#define MAX_INDEX_BUCKETS (1024 * 1024 * 4) /* 4M buckets max */
#define MIN_INDEX_BUCKETS 16

/* Optimized load factors for better performance/memory trade-off */
#define MAX_LOAD_FACTOR 0.70       /* Slightly reduced to trigger earlier resizing */
#define MIN_LOAD_FACTOR 0.15       /* Reduced for more aggressive downsize */
#define RESIZE_GROWTH_FACTOR 2      /* Double size when growing */
#define RESIZE_SHRINK_FACTOR 2      /* Half size when shrinking */

/* Index entry cache */
#define INDEX_CACHE_SIZE 256       /* LRU cache size for frequent lookups */
#define INDEX_CACHE_ENABLED 1      /* Set to 0 to disable cache */

/* Statistics tracking */
#define STATS_TRACKING_ENABLED 1     /* Set to 0 to disable statistics */
#define STATS_SAMPLE_RATE 100      /* Track stats every N operations */

/* Advanced configurations */
#define USE_ROBIN_HOOD_HASHING 1     /* Robin Hood hashing to reduce variance */
#define MAX_PROBE_DISTANCE 16      /* Maximum probe distance for Robin Hood */
#define BLOOM_FILTER_ENABLED 1      /* Bloom filter for negative lookups */
#define BLOOM_FILTER_SIZE 8192      /* Bloom filter size in bits */
#define BLOOM_FILTER_HASHES 3      /* Number of hash functions for bloom filter */

/* Optimized hash functions - based on high-quality non-cryptographic hashing */

/* FNV-1a hash function for general purpose use */
static uint32_t hash_string_fnv1a(const char* str) {
  uint32_t hash = 2166136261u; /* FNV offset basis */

  while (*str) {
    hash ^= (uint8_t)*str++;
    hash *= 16777619; /* FNV prime */
  }

  return hash;
}

/* xxHash-inspired hash function for better distribution */
static uint32_t hash_string_xx(const char* str) {
  const uint32_t PRIME1 = 2654435761U;
  const uint32_t PRIME2 = 2246822519U;
  const uint32_t PRIME3 = 3266489917U;
  const uint32_t PRIME4 = 668265263U;
  const uint32_t PRIME5 = 374761393U;

  uint32_t hash = PRIME5;
  size_t len = strlen(str);
  const char* end = str + len;
  
  /* Handle bulk data in 4-byte chunks */
  while (str + 4 <= end) {
    uint32_t block;
    memcpy(&block, str, 4);
    hash ^= block * PRIME1;
    hash = ((hash << 13) | (hash >> 19)) * PRIME2;
    str += 4;
  }
  
  /* Handle remaining bytes */
  while (str < end) {
    hash ^= (uint32_t)(*str++) * PRIME3;
    hash = ((hash << 17) | (hash >> 15)) * PRIME4;
  }
  
  /* Finalize */
  hash ^= hash >> 15;
  hash *= PRIME2;
  hash ^= hash >> 13;
  hash *= PRIME3;
  hash ^= hash >> 16;
  
  return hash;
}

/* MurmurHash3 implementation for high-quality hashing */
static uint32_t hash_string_murmur3(const char* str) {
  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;
  const uint32_t r1 = 15;
  const uint32_t r2 = 13;
  const uint32_t m = 5;
  const uint32_t n = 0xe6546b64;
  
  uint32_t hash = 0; /* Seed */
  size_t len = strlen(str);
  const uint8_t* data = (const uint8_t*)str;
  const size_t block_count = len / 4;
  
  /* Body */
  const uint32_t* blocks = (const uint32_t*)(data);
  for (size_t i = 0; i < block_count; i++) {
    uint32_t k;
    memcpy(&k, &blocks[i], sizeof(uint32_t));
    
    k *= c1;
    k = (k << r1) | (k >> (32 - r1));
    k *= c2;
    
    hash ^= k;
    hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
  }
  
  /* Tail */
  const uint8_t* tail = (const uint8_t*)(data + block_count * 4);
  uint32_t k1 = 0;
  
  switch (len & 3) {
    case 3:
      k1 ^= tail[2] << 16;
      /* fall through */
    case 2:
      k1 ^= tail[1] << 8;
      /* fall through */
    case 1:
      k1 ^= tail[0];
      k1 *= c1;
      k1 = (k1 << r1) | (k1 >> (32 - r1));
      k1 *= c2;
      hash ^= k1;
  }
  
  /* Finalization */
  hash ^= len;
  hash ^= (hash >> 16);
  hash *= 0x85ebca6b;
  hash ^= (hash >> 13);
  hash *= 0xc2b2ae35;
  hash ^= (hash >> 16);
  
  return hash;
}

/* Combine multiple hash functions for improved distribution */
static uint32_t hash_string(const char* str) {
  /* Combine different hashing algorithms for better distribution */
  uint32_t h1 = hash_string_fnv1a(str);
  uint32_t h2 = hash_string_xx(str);
  
  /* XOR and rotate to mix */
  return (h1 ^ (h2 << 5) ^ (h2 >> 2));
}

/* Secondary hash for double hashing */
/* Currently unused but kept for future double-hashing optimization */
static uint32_t __attribute__((unused)) hash_string_secondary(const char* str) {
  return hash_string_murmur3(str) | 1; // Ensure odd value for coprime stepping
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
    
    /* Safety check for unreasonably large values */
    if (n > MAX_INDEX_BUCKETS)
      return MAX_INDEX_BUCKETS - 1;
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

/* Bloom filter implementation */
typedef struct {
  uint8_t* bits;
  size_t size;      /* Size in bits */
  int hash_functions;  /* Number of hash functions */
} bloom_filter_t;

/* Initialize a bloom filter */
static bloom_filter_t* bloom_filter_init(size_t size_bits, int hash_functions) {
  bloom_filter_t* filter = (bloom_filter_t*)malloc(sizeof(bloom_filter_t));
  if (!filter) return NULL;
  
  size_t size_bytes = (size_bits + 7) / 8; /* Convert bits to bytes, rounding up */
  filter->bits = (uint8_t*)calloc(size_bytes, 1);
  if (!filter->bits) {
    free(filter);
    return NULL;
  }
  
  filter->size = size_bits;
  filter->hash_functions = hash_functions;
  return filter;
}

/* Free a bloom filter */
static void bloom_filter_free(bloom_filter_t* filter) {
  if (filter) {
    free(filter->bits);
    free(filter);
  }
}

/* Add a key to the bloom filter */
static void bloom_filter_add(bloom_filter_t* filter, const char* key) {
  if (!filter || !key) return;
  
  /* Use different hash seeds for each function */
  uint32_t hash1 = hash_string_fnv1a(key);
  uint32_t hash2 = hash_string_xx(key);
  uint32_t hash3 = hash_string_murmur3(key);
  
  for (int i = 0; i < filter->hash_functions; i++) {
    /* Combine hashes with different weights */
    uint32_t hash = (hash1 + i * hash2 + i * i * hash3) % filter->size;
    size_t byte_pos = hash / 8;
    uint8_t bit_pos = hash % 8;
    
    filter->bits[byte_pos] |= (1 << bit_pos);
  }
}

/* Check if a key might be in the set */
static int bloom_filter_may_contain(bloom_filter_t* filter, const char* key) {
  if (!filter || !key) return 0;
  
  uint32_t hash1 = hash_string_fnv1a(key);
  uint32_t hash2 = hash_string_xx(key);
  uint32_t hash3 = hash_string_murmur3(key);
  
  for (int i = 0; i < filter->hash_functions; i++) {
    uint32_t hash = (hash1 + i * hash2 + i * i * hash3) % filter->size;
    size_t byte_pos = hash / 8;
    uint8_t bit_pos = hash % 8;
    
    if (!(filter->bits[byte_pos] & (1 << bit_pos))) {
      return 0; /* Definitely not in the set */
    }
  }
  
  return 1; /* Might be in the set */
}

/* LRU Cache implementation for frequent index lookups */
typedef struct cache_entry {
  char* key;
  char* document_id;
  time_t last_access;
  struct cache_entry* next;
  struct cache_entry* prev;
} cache_entry_t;

typedef struct {
  cache_entry_t* head;
  cache_entry_t* tail;
  size_t size;
  size_t capacity;
  pthread_mutex_t mutex;
} lru_cache_t;

/* Initialize an LRU cache */
static lru_cache_t* lru_cache_init(size_t capacity) {
  lru_cache_t* cache = (lru_cache_t*)malloc(sizeof(lru_cache_t));
  if (!cache) return NULL;
  
  cache->head = NULL;
  cache->tail = NULL;
  cache->size = 0;
  cache->capacity = capacity;
  
  if (pthread_mutex_init(&cache->mutex, NULL) != 0) {
    free(cache);
    return NULL;
  }
  
  return cache;
}

/* Free an LRU cache */
/* Currently unused but kept for future cache memory management */
static void __attribute__((unused)) lru_cache_free(lru_cache_t* cache) {
  if (!cache) return;
  
  pthread_mutex_lock(&cache->mutex);
  
  cache_entry_t* current = cache->head;
  while (current) {
    cache_entry_t* next = current->next;
    free(current->key);
    free(current->document_id);
    free(current);
    current = next;
  }
  
  pthread_mutex_unlock(&cache->mutex);
  pthread_mutex_destroy(&cache->mutex);
  free(cache);
}

/* Move a cache entry to the front (most recently used) */
static void lru_cache_move_to_front(lru_cache_t* cache, cache_entry_t* entry) {
  if (!cache || !entry || entry == cache->head) return;
  
  /* Remove from current position */
  if (entry->prev) entry->prev->next = entry->next;
  if (entry->next) entry->next->prev = entry->prev;
  if (entry == cache->tail) cache->tail = entry->prev;
  
  /* Move to front */
  entry->next = cache->head;
  entry->prev = NULL;
  if (cache->head) cache->head->prev = entry;
  cache->head = entry;
  
  /* Update tail if needed */
  if (!cache->tail) cache->tail = entry;
}

/* Add or update an entry in the cache */
static void lru_cache_put(lru_cache_t* cache, const char* key, const char* document_id) {
  if (!cache || !key || !document_id) return;
  
  pthread_mutex_lock(&cache->mutex);
  
  /* Check if key already exists */
  cache_entry_t* current = cache->head;
  while (current) {
    if (strcmp(current->key, key) == 0) {
      /* Update existing entry */
      current->last_access = time(NULL);
      lru_cache_move_to_front(cache, current);
      pthread_mutex_unlock(&cache->mutex);
      return;
    }
    current = current->next;
  }
  
  /* Create new entry */
  cache_entry_t* new_entry = (cache_entry_t*)malloc(sizeof(cache_entry_t));
  if (!new_entry) {
    pthread_mutex_unlock(&cache->mutex);
    return;
  }
  
  new_entry->key = strdup(key);
  new_entry->document_id = strdup(document_id);
  new_entry->last_access = time(NULL);
  
  if (!new_entry->key || !new_entry->document_id) {
    free(new_entry->key);
    free(new_entry->document_id);
    free(new_entry);
    pthread_mutex_unlock(&cache->mutex);
    return;
  }
  
  /* Add to front */
  new_entry->next = cache->head;
  new_entry->prev = NULL;
  if (cache->head) cache->head->prev = new_entry;
  cache->head = new_entry;
  
  if (!cache->tail) cache->tail = new_entry;
  
  cache->size++;
  
  /* Remove least recently used entry if over capacity */
  if (cache->size > cache->capacity && cache->tail) {
    cache_entry_t* to_remove = cache->tail;
    cache->tail = to_remove->prev;
    if (cache->tail) cache->tail->next = NULL;
    
    free(to_remove->key);
    free(to_remove->document_id);
    free(to_remove);
    
    cache->size--;
  }
  
  pthread_mutex_unlock(&cache->mutex);
}

/* Get an entry from the cache */
static int lru_cache_get(lru_cache_t* cache, const char* key, char** document_id) {
  if (!cache || !key || !document_id) return 0;
  
  pthread_mutex_lock(&cache->mutex);
  
  cache_entry_t* current = cache->head;
  while (current) {
    if (strcmp(current->key, key) == 0) {
      /* Found entry, update access time and move to front */
      current->last_access = time(NULL);
      lru_cache_move_to_front(cache, current);
      
      /* Return document_id */
      *document_id = strdup(current->document_id);
      
      pthread_mutex_unlock(&cache->mutex);
      return 1;
    }
    current = current->next;
  }
  
  pthread_mutex_unlock(&cache->mutex);
  return 0;
}

/* Enhanced index structure with performance optimizations */
typedef struct optimized_index {
  /* Original fields */
  char* name;             /* Index name */
  char* field_path;          /* Field path to index */
  index_type_t type;         /* Index type */
  size_t entries;           /* Number of entries */
  size_t num_buckets;         /* Number of hash buckets */
  index_entry_t** buckets;      /* Hash buckets */
  struct optimized_index* next;    /* Next index in collection */
  pthread_rwlock_t lock;       /* Read-write lock */
  
  /* Performance enhancements */
  bloom_filter_t* bloom_filter;    /* Bloom filter for fast negative lookups */
  lru_cache_t* cache;         /* LRU cache for frequent lookups */
  
  /* Statistics */
  size_t lookups;           /* Total number of lookups */
  size_t hits;             /* Number of successful lookups */
  size_t collisions;          /* Number of hash collisions */
  size_t longest_chain;        /* Length of longest bucket chain */
  size_t resizes;           /* Number of resize operations */
  clock_t total_lookup_time;      /* Total time spent in lookups */
  clock_t max_lookup_time;       /* Maximum lookup time */
} optimized_index_t;

/* Resize the index hash table with improved algorithm */
static int resize_index(optimized_index_t* index, size_t new_size) {
  if (!index || new_size == 0) {
    return 0;
  }
  
  /* Constrain size to valid range */
  if (new_size < MIN_INDEX_BUCKETS) new_size = MIN_INDEX_BUCKETS;
  if (new_size > MAX_INDEX_BUCKETS) new_size = MAX_INDEX_BUCKETS;
  
  /* Ensure new size is prime for better distribution */
  new_size = next_prime(new_size);
  
  /* Skip if same size */
  if (new_size == index->num_buckets) {
    return 1;
  }
  
  /* Allocate new buckets */
  index_entry_t** new_buckets = (index_entry_t**)calloc(new_size, sizeof(index_entry_t*));
  if (!new_buckets) {
    LOG_ERROR("Out of memory");
    return 0;
  }
  
  clock_t start_time = clock();
  
  /* Track statistics */
  size_t max_chain_length = 0;
  size_t collisions = 0;
  
  /* Rehash all entries */
  for (size_t i = 0; i < index->num_buckets; i++) {
    index_entry_t* entry = index->buckets[i];
    
    while (entry) {
      /* Save next entry */
      index_entry_t* next = entry->next;
      
      /* Rehash entry */
      uint32_t hash = hash_string(entry->key_value) % new_size;
      
      if (USE_ROBIN_HOOD_HASHING) {
        /* Robin Hood hashing - find optimal bucket with least probe distance */
        size_t probe_distance = 0;
        size_t bucket = hash;
        
        while (probe_distance < MAX_PROBE_DISTANCE) {
          if (new_buckets[bucket] == NULL) {
            /* Found empty slot */
            entry->next = NULL;
            new_buckets[bucket] = entry;
            break;
          }
          
          /* Slot occupied, try next (linear probing) */
          bucket = (bucket + 1) % new_size;
          probe_distance++;
          collisions++;
        }
        
        /* If we exceeded max probe distance, fall back to chaining */
        if (probe_distance >= MAX_PROBE_DISTANCE) {
          entry->next = new_buckets[hash];
          new_buckets[hash] = entry;
          
          /* Track chain length */
          size_t chain_length = 1;
          index_entry_t* chain = entry->next;
          while (chain) {
            chain_length++;
            chain = chain->next;
          }
          
          if (chain_length > max_chain_length) {
            max_chain_length = chain_length;
          }
        }
      } else {
        /* Standard chaining approach */
        entry->next = new_buckets[hash];
        new_buckets[hash] = entry;
        
        /* Track chain length */
        size_t chain_length = 1;
        index_entry_t* chain = entry->next;
        while (chain) {
          chain_length++;
          chain = chain->next;
        }
        
        if (chain_length > max_chain_length) {
          max_chain_length = chain_length;
        }
        
        if (chain_length > 1) {
          collisions += chain_length - 1;
        }
      }
      
      /* Move to next entry */
      entry = next;
    }
  }
  
  /* Update statistics */
  index->collisions += collisions;
  index->longest_chain = max_chain_length > index->longest_chain ? 
              max_chain_length : index->longest_chain;
  index->resizes++;
  
  /* Free old buckets array (not the entries) */
  free(index->buckets);
  
  /* Update index with new buckets */
  index->buckets = new_buckets;
  index->num_buckets = new_size;
  
  /* Rebuild bloom filter if needed */
  if (BLOOM_FILTER_ENABLED && index->bloom_filter) {
    bloom_filter_free(index->bloom_filter);
    index->bloom_filter = bloom_filter_init(BLOOM_FILTER_SIZE, BLOOM_FILTER_HASHES);
    
    /* Rebuild bloom filter with all keys */
    for (size_t i = 0; i < new_size; i++) {
      index_entry_t* entry = new_buckets[i];
      while (entry) {
        bloom_filter_add(index->bloom_filter, entry->key_value);
        entry = entry->next;
      }
    }
  }
  
  clock_t end_time = clock();
  double resize_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
  
  LOG_INFO("Index resize: %zu → %zu buckets, %zu entries, %.2f ms, max chain: %zu", 
       index->num_buckets, new_size, index->entries, 
       resize_time, max_chain_length);
  
  return 1;
}

/* Check if index needs resizing and resize if necessary */
static void check_and_resize_index(optimized_index_t* index) {
  if (!index || !index->buckets) {
    return;
  }
  
  double load_factor = (double)index->entries / index->num_buckets;
  
  /* Check if we need to grow */
  if (load_factor > MAX_LOAD_FACTOR && index->num_buckets < MAX_INDEX_BUCKETS) {
    size_t new_size = index->num_buckets * RESIZE_GROWTH_FACTOR;
    if (new_size > MAX_INDEX_BUCKETS) {
      new_size = MAX_INDEX_BUCKETS;
    }
    resize_index(index, new_size);
  }
  /* Check if we need to shrink */
  else if (load_factor < MIN_LOAD_FACTOR && index->num_buckets > MIN_INDEX_BUCKETS) {
    size_t new_size = index->num_buckets / RESIZE_SHRINK_FACTOR;
    if (new_size < MIN_INDEX_BUCKETS) {
      new_size = MIN_INDEX_BUCKETS;
    }
    resize_index(index, new_size);
  }
}

/* Create a new optimized index */
optimized_index_t* db_create_optimized_index(database_t* db, const char* collection, const char* name, 
                const char* field_path, index_type_t type) {
  if (!db || !collection || !name || !field_path) {
    LOG_ERROR("Invalid parameters for creating optimized index");
    return NULL;
  }
  
  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    LOG_ERROR("Collection not found: %s", collection);
    return NULL;
  }
  
  /* Lock collection */
  pthread_mutex_lock(&coll->lock);
  
  /* Check if index already exists */
  optimized_index_t* existing = (optimized_index_t*)coll->indexes;
  while (existing) {
    if (strcmp(existing->name, name) == 0) {
      pthread_mutex_unlock(&coll->lock);
      return existing;
    }
    existing = (optimized_index_t*)existing->next;
  }
  
  /* Create new index */
  optimized_index_t* index = (optimized_index_t*)malloc(sizeof(optimized_index_t));
  if (!index) {
    pthread_mutex_unlock(&coll->lock);
    LOG_ERROR("Out of memory");
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
    LOG_ERROR("Out of memory");
    return NULL;
  }
  
  /* Initialize lock */
  pthread_rwlock_init(&index->lock, NULL);
  
  /* Initialize bloom filter if enabled */
  if (BLOOM_FILTER_ENABLED) {
    index->bloom_filter = bloom_filter_init(BLOOM_FILTER_SIZE, BLOOM_FILTER_HASHES);
    if (!index->bloom_filter) {
      LOG_WARNING("Failed to initialize bloom filter for index");
    }
  } else {
    index->bloom_filter = NULL;
  }
  
  /* Initialize LRU cache if enabled */
  if (INDEX_CACHE_ENABLED) {
    index->cache = lru_cache_init(INDEX_CACHE_SIZE);
    if (!index->cache) {
      LOG_WARNING("Failed to initialize LRU cache for index");
    }
  } else {
    index->cache = NULL;
  }
  
  /* Initialize statistics */
  index->lookups = 0;
  index->hits = 0;
  index->collisions = 0;
  index->longest_chain = 0;
  index->resizes = 0;
  index->total_lookup_time = 0;
  index->max_lookup_time = 0;
  
  /* Add index to collection */
  if (!coll->indexes) {
    coll->indexes = (index_t*)index;
  } else {
    optimized_index_t* last = (optimized_index_t*)coll->indexes;
    while (last->next) {
      last = (optimized_index_t*)last->next;
    }
    last->next = (optimized_index_t*)index;
  }
  
  /* Build index */
  if (coll->documents && json_get_type(coll->documents) == JSON_ARRAY) {
    for (size_t i = 0; i < json_array_size(coll->documents); i++) {
      json_value_t* doc = json_array_get(coll->documents, i);
      if (doc && json_get_type(doc) == JSON_OBJECT) {
        /* Get document ID */
        json_value_t* id_val = json_object_get(doc, "id");
        if (id_val && json_get_type(id_val) == JSON_STRING) {
          const char* doc_id = json_get_string(id_val);
          
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
                    pthread_mutex_unlock(&coll->lock);
                    db_drop_index(db, collection, name);
                    LOG_ERROR("Uniqueness constraint violation during index creation");
                    return NULL;
                  }
                  
                  entry = entry->next;
                }
              }
              
              /* Add to index */
              int is_collision = 0;
              
              /* Calculate hash */
              uint32_t hash = hash_string(value_str) % index->num_buckets;
              
              if (USE_ROBIN_HOOD_HASHING) {
                /* Try to find an optimal position within MAX_PROBE_DISTANCE */
                size_t probe_distance = 0;
                size_t bucket = hash;
                
                while (probe_distance < MAX_PROBE_DISTANCE) {
                  if (index->buckets[bucket] == NULL) {
                    /* Found empty slot */
                    index_entry_t* new_entry = (index_entry_t*)malloc(sizeof(index_entry_t));
                    if (new_entry) {
                      new_entry->document_id = strdup(doc_id);
                      new_entry->key_value = value_str;
                      new_entry->next = NULL;
                      index->buckets[bucket] = new_entry;
                      index->entries++;
                      
                      /* Add to bloom filter */
                      if (BLOOM_FILTER_ENABLED && index->bloom_filter) {
                        bloom_filter_add(index->bloom_filter, value_str);
                      }
                      
                      /* Add to cache */
                      if (INDEX_CACHE_ENABLED && index->cache) {
                        lru_cache_put(index->cache, value_str, doc_id);
                      }
                      break;
                    } else {
                      free(value_str);
                      pthread_rwlock_unlock(&index->lock);
                      pthread_mutex_unlock(&coll->lock);
                      LOG_ERROR("Out of memory");
                      return NULL;
                    }
                  }
                  
                  /* Slot occupied, track collision */
                  is_collision = 1;
                  
                  /* Try next bucket */
                  bucket = (bucket + 1) % index->num_buckets;
                  probe_distance++;
                }
                
                /* If we exceeded max probe distance, fall back to chaining */
                if (probe_distance >= MAX_PROBE_DISTANCE) {
                  index_entry_t* new_entry = (index_entry_t*)malloc(sizeof(index_entry_t));
                  if (new_entry) {
                    new_entry->document_id = strdup(doc_id);
                    new_entry->key_value = value_str;
                    new_entry->next = index->buckets[hash];
                    index->buckets[hash] = new_entry;
                    index->entries++;
                    
                    /* Add to bloom filter */
                    if (BLOOM_FILTER_ENABLED && index->bloom_filter) {
                      bloom_filter_add(index->bloom_filter, value_str);
                    }
                    
                    /* Add to cache */
                    if (INDEX_CACHE_ENABLED && index->cache) {
                      lru_cache_put(index->cache, value_str, doc_id);
                    }
                    
                    /* Track chain length */
                    size_t chain_length = 1;
                    index_entry_t* chain = new_entry->next;
                    while (chain) {
                      chain_length++;
                      chain = chain->next;
                    }
                    
                    if (chain_length > index->longest_chain) {
                      index->longest_chain = chain_length;
                    }
                  } else {
                    free(value_str);
                    pthread_rwlock_unlock(&index->lock);
                    pthread_mutex_unlock(&coll->lock);
                    LOG_ERROR("Out of memory");
                    return NULL;
                  }
                }
              } else {
                /* Standard chaining approach */
                index_entry_t* new_entry = (index_entry_t*)malloc(sizeof(index_entry_t));
                if (new_entry) {
                  new_entry->document_id = strdup(doc_id);
                  new_entry->key_value = value_str;
                  
                  is_collision = (index->buckets[hash] != NULL);
                  
                  new_entry->next = index->buckets[hash];
                  index->buckets[hash] = new_entry;
                  index->entries++;
                  
                  /* Add to bloom filter */
                  if (BLOOM_FILTER_ENABLED && index->bloom_filter) {
                    bloom_filter_add(index->bloom_filter, value_str);
                  }
                  
                  /* Add to cache */
                  if (INDEX_CACHE_ENABLED && index->cache) {
                    lru_cache_put(index->cache, value_str, doc_id);
                  }
                  
                  /* Track chain length */
                  size_t chain_length = 1;
                  index_entry_t* chain = new_entry->next;
                  while (chain) {
                    chain_length++;
                    chain = chain->next;
                  }
                  
                  if (chain_length > index->longest_chain) {
                    index->longest_chain = chain_length;
                  }
                } else {
                  free(value_str);
                  pthread_rwlock_unlock(&index->lock);
                  pthread_mutex_unlock(&coll->lock);
                  LOG_ERROR("Out of memory");
                  return NULL;
                }
              }
              
              /* Update collision statistics */
              if (is_collision) {
                index->collisions++;
              }
              
              /* Check if index needs resizing */
              check_and_resize_index(index);
              
              /* Unlock index */
              pthread_rwlock_unlock(&index->lock);
            }
          }
        }
      }
    }
  }
  
  LOG_INFO("Created optimized index '%s' on '%s.%s' (type: %d, entries: %zu)", 
       name, collection, field_path, type, index->entries);
  
  pthread_mutex_unlock(&coll->lock);
  return index;
}

/* Add a document to an optimized index */
int db_index_add_document_optimized(optimized_index_t* index, const char* doc_id, json_value_t* document) {
  if (!index || !doc_id || !document) {
    LOG_ERROR("Invalid parameters for optimized index_add_document");
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
  int is_collision = 0;
  
  /* Calculate hash */
  uint32_t hash = hash_string(value_str) % index->num_buckets;
  
  if (USE_ROBIN_HOOD_HASHING) {
    /* Try to find an optimal position within MAX_PROBE_DISTANCE */
    size_t probe_distance = 0;
    size_t bucket = hash;
    
    while (probe_distance < MAX_PROBE_DISTANCE) {
      if (index->buckets[bucket] == NULL) {
        /* Found empty slot */
        index_entry_t* new_entry = (index_entry_t*)malloc(sizeof(index_entry_t));
        if (new_entry) {
          new_entry->document_id = strdup(doc_id);
          new_entry->key_value = value_str;
          new_entry->next = NULL;
          index->buckets[bucket] = new_entry;
          index->entries++;
          
          /* Add to bloom filter */
          if (BLOOM_FILTER_ENABLED && index->bloom_filter) {
            bloom_filter_add(index->bloom_filter, value_str);
          }
          
          /* Add to cache */
          if (INDEX_CACHE_ENABLED && index->cache) {
            lru_cache_put(index->cache, value_str, doc_id);
          }
          break;
        } else {
          free(value_str);
          pthread_rwlock_unlock(&index->lock);
          LOG_ERROR("Out of memory");
          return 0;
        }
      }
      
      /* Slot occupied, track collision */
      is_collision = 1;
      
      /* Try next bucket */
      bucket = (bucket + 1) % index->num_buckets;
      probe_distance++;
    }
    
    /* If we exceeded max probe distance, fall back to chaining */
    if (probe_distance >= MAX_PROBE_DISTANCE) {
      index_entry_t* new_entry = (index_entry_t*)malloc(sizeof(index_entry_t));
      if (new_entry) {
        new_entry->document_id = strdup(doc_id);
        new_entry->key_value = value_str;
        new_entry->next = index->buckets[hash];
        index->buckets[hash] = new_entry;
        index->entries++;
        
        /* Add to bloom filter */
        if (BLOOM_FILTER_ENABLED && index->bloom_filter) {
          bloom_filter_add(index->bloom_filter, value_str);
        }
        
        /* Add to cache */
        if (INDEX_CACHE_ENABLED && index->cache) {
          lru_cache_put(index->cache, value_str, doc_id);
        }
        
        /* Track chain length */
        size_t chain_length = 1;
        index_entry_t* chain = new_entry->next;
        while (chain) {
          chain_length++;
          chain = chain->next;
        }
        
        if (chain_length > index->longest_chain) {
          index->longest_chain = chain_length;
        }
      } else {
        free(value_str);
        pthread_rwlock_unlock(&index->lock);
        LOG_ERROR("Out of memory");
        return 0;
      }
    }
  } else {
    /* Standard chaining approach */
    index_entry_t* new_entry = (index_entry_t*)malloc(sizeof(index_entry_t));
    if (new_entry) {
      new_entry->document_id = strdup(doc_id);
      new_entry->key_value = value_str;
      
      is_collision = (index->buckets[hash] != NULL);
      
      new_entry->next = index->buckets[hash];
      index->buckets[hash] = new_entry;
      index->entries++;
      
      /* Add to bloom filter */
      if (BLOOM_FILTER_ENABLED && index->bloom_filter) {
        bloom_filter_add(index->bloom_filter, value_str);
      }
      
      /* Add to cache */
      if (INDEX_CACHE_ENABLED && index->cache) {
        lru_cache_put(index->cache, value_str, doc_id);
      }
      
      /* Track chain length */
      size_t chain_length = 1;
      index_entry_t* chain = new_entry->next;
      while (chain) {
        chain_length++;
        chain = chain->next;
      }
      
      if (chain_length > index->longest_chain) {
        index->longest_chain = chain_length;
      }
    } else {
      free(value_str);
      pthread_rwlock_unlock(&index->lock);
      LOG_ERROR("Out of memory");
      return 0;
    }
  }
  
  /* Update collision statistics */
  if (is_collision) {
    index->collisions++;
  }
  
  /* Check if index needs resizing */
  check_and_resize_index(index);
  
  /* Unlock index */
  pthread_rwlock_unlock(&index->lock);
  
  return 1;
}

/* Remove a document from an optimized index */
int db_index_remove_document_optimized(optimized_index_t* index, const char* doc_id) {
  if (!index || !doc_id) {
    LOG_ERROR("Invalid parameters for optimized index_remove_document");
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
        
        /* Remove from cache if enabled */
        /* Note: This is a simplification. In a complete implementation,
          we'd check if there are other documents with the same key and only
          remove from cache if this was the last one. */
        
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

/* Query documents by optimized index */
json_value_t* db_query_by_optimized_index(database_t* db, const char* collection, const char* field_path, 
               const char* value, int limit, int skip) {
  if (!db || !collection || !field_path || !value) {
    LOG_ERROR("Invalid parameters for optimized query_by_index");
    return NULL;
  }
  
  /* Get collection */
  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    LOG_ERROR("Collection not found: %s", collection);
    return NULL;
  }
  
  /* Lock collection for reading */
  pthread_mutex_lock(&coll->lock);
  
  /* Find optimized index for field path */
  optimized_index_t* index = NULL;
  optimized_index_t* current = (optimized_index_t*)coll->indexes;
  
  while (current) {
    if (strcmp(current->field_path, field_path) == 0) {
      index = current;
      break;
    }
    current = (optimized_index_t*)current->next;
  }
  
  /* If no index exists, fall back to full collection scan */
  if (!index) {
    /* Create result array */
    json_value_t* result = json_create_array();
    if (!result) {
      pthread_mutex_unlock(&coll->lock);
      LOG_ERROR("create result array for collection scan");
      return NULL;
    }
    
    /* Log performance warning */
    LOG_WARNING("Full scan on %s.%s - needs index", 
         collection, field_path);
    
    /* Scan all documents */
    if (coll->documents && json_get_type(coll->documents) == JSON_ARRAY) {
      int count = 0;
      int skipped = 0;
      
      for (size_t i = 0; i < json_array_size(coll->documents); i++) {
        json_value_t* doc = json_array_get(coll->documents, i);
        if (doc && json_get_type(doc) == JSON_OBJECT) {
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
  
  /* Track performance */
  clock_t start_time = clock();
  
  /* Use optimized index for query */
  pthread_rwlock_rdlock(&index->lock);
  
  /* Update statistics */
  index->lookups++;
  
  /* Create result array */
  json_value_t* result = json_create_array();
  if (!result) {
    pthread_rwlock_unlock(&index->lock);
    pthread_mutex_unlock(&coll->lock);
    LOG_ERROR("create result array for index query");
    return NULL;
  }
  
  /* Check cache first if enabled */
  int found_in_cache = 0;
  if (INDEX_CACHE_ENABLED && index->cache) {
    char* cached_doc_id = NULL;
    
    if (lru_cache_get(index->cache, value, &cached_doc_id)) {
      /* Found in cache, get document by ID */
      if (coll->documents && json_get_type(coll->documents) == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(coll->documents); i++) {
          json_value_t* doc = json_array_get(coll->documents, i);
          if (doc && json_get_type(doc) == JSON_OBJECT) {
            json_value_t* id_val = json_object_get(doc, "id");
            if (id_val && json_get_type(id_val) == JSON_STRING && 
              strcmp(json_get_string(id_val), cached_doc_id) == 0) {
              
              if (skip <= 0) {
                json_array_append(result, json_clone(doc));
                index->hits++;
                found_in_cache = 1;
              } else {
                skip--;
              }
              
              break;
            }
          }
        }
      }
      
      free(cached_doc_id);
      
      /* If limit is 1 and we found it, we're done */
      if (found_in_cache && limit == 1) {
        clock_t end_time = clock();
        clock_t lookup_time = end_time - start_time;
        index->total_lookup_time += lookup_time;
        
        if (lookup_time > index->max_lookup_time) {
          index->max_lookup_time = lookup_time;
        }
        
        pthread_rwlock_unlock(&index->lock);
        pthread_mutex_unlock(&coll->lock);
        return result;
      }
    }
  }
  
  /* Check bloom filter if not found in cache */
  if (!found_in_cache && BLOOM_FILTER_ENABLED && index->bloom_filter) {
    if (!bloom_filter_may_contain(index->bloom_filter, value)) {
      /* Definitely not in the index */
      pthread_rwlock_unlock(&index->lock);
      pthread_mutex_unlock(&coll->lock);
      
      clock_t end_time = clock();
      clock_t lookup_time = end_time - start_time;
      index->total_lookup_time += lookup_time;
      
      return result; /* Empty result */
    }
  }
  
  /* If not found in cache or we need more results, check the index */
  if (!found_in_cache || (found_in_cache && limit > 1)) {
    /* Calculate hash for the value */
    uint32_t hash = hash_string(value) % index->num_buckets;
    
    /* Account for any results already found in cache */
    /* Use SIZE_MAX as sentinel for "no limit" instead of -1 to avoid signedness issues */
    size_t remaining_limit = limit > 0 ? (size_t)limit - json_array_size(result) : SIZE_MAX;
    
    if (USE_ROBIN_HOOD_HASHING) {
      /* Try probing up to MAX_PROBE_DISTANCE slots */
      size_t probe_distance = 0;
      size_t bucket = hash;
      
      while (probe_distance < MAX_PROBE_DISTANCE && remaining_limit > 0) {
        index_entry_t* entry = index->buckets[bucket];
        
        /* Check if bucket is empty - key not found */
        if (!entry) {
          break;
        }
        
        /* Check entries in this bucket */
        while (entry && remaining_limit > 0) {
          if (strcmp(entry->key_value, value) == 0) {
            /* Match found */
            if (skip > 0) {
              skip--;
            } else {
              /* Get document by ID */
              if (coll->documents && json_get_type(coll->documents) == JSON_ARRAY) {
                for (size_t i = 0; i < json_array_size(coll->documents); i++) {
                  json_value_t* doc = json_array_get(coll->documents, i);
                  if (doc && json_get_type(doc) == JSON_OBJECT) {
                    json_value_t* id_val = json_object_get(doc, "id");
                    if (id_val && json_get_type(id_val) == JSON_STRING && 
                      strcmp(json_get_string(id_val), entry->document_id) == 0) {
                      
                      /* Add to cache */
                      if (INDEX_CACHE_ENABLED && index->cache) {
                        lru_cache_put(index->cache, value, entry->document_id);
                      }
                      
                      /* Add to result */
                      json_array_append(result, json_clone(doc));
                      index->hits++;
                      
                      if (remaining_limit > 0) {
                        remaining_limit--;
                      }
                      
                      break;
                    }
                  }
                }
              }
            }
          }
          
          entry = entry->next;
        }
        
        /* If we've reached our limit, stop */
        if (remaining_limit == 0) {
          break;
        }
        
        /* Try next bucket */
        bucket = (bucket + 1) % index->num_buckets;
        probe_distance++;
      }
    } else {
      /* Standard chaining approach */
      index_entry_t* entry = index->buckets[hash];
      
      while (entry && remaining_limit > 0) {
        if (strcmp(entry->key_value, value) == 0) {
          /* Match found */
          if (skip > 0) {
            skip--;
          } else {
            /* Get document by ID */
            if (coll->documents && json_get_type(coll->documents) == JSON_ARRAY) {
              for (size_t i = 0; i < json_array_size(coll->documents); i++) {
                json_value_t* doc = json_array_get(coll->documents, i);
                if (doc && json_get_type(doc) == JSON_OBJECT) {
                  json_value_t* id_val = json_object_get(doc, "id");
                  if (id_val && json_get_type(id_val) == JSON_STRING && 
                    strcmp(json_get_string(id_val), entry->document_id) == 0) {
                    
                    /* Add to cache */
                    if (INDEX_CACHE_ENABLED && index->cache) {
                      lru_cache_put(index->cache, value, entry->document_id);
                    }
                    
                    /* Add to result */
                    json_array_append(result, json_clone(doc));
                    index->hits++;
                    
                    if (remaining_limit > 0) {
                      remaining_limit--;
                    }
                    
                    break;
                  }
                }
              }
            }
          }
        }
        
        entry = entry->next;
      }
    }
  }
  
  /* Update performance statistics */
  clock_t end_time = clock();
  clock_t lookup_time = end_time - start_time;
  index->total_lookup_time += lookup_time;
  
  if (lookup_time > index->max_lookup_time) {
    index->max_lookup_time = lookup_time;
  }
  
  /* Unlock index and collection */
  pthread_rwlock_unlock(&index->lock);
  pthread_mutex_unlock(&coll->lock);
  
  return result;
}

/* Get optimized index statistics as JSON */
json_value_t* db_optimized_index_stats(database_t* db, const char* collection, const char* name) {
  if (!db || !collection || !name) {
    LOG_ERROR("Invalid parameters");
    return NULL;
  }
  
  /* Get collection */
  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    LOG_ERROR("Collection not found: %s", collection);
    return NULL;
  }
  
  /* Lock collection for reading */
  pthread_mutex_lock(&coll->lock);
  
  /* Find index */
  optimized_index_t* index = NULL;
  optimized_index_t* current = (optimized_index_t*)coll->indexes;
  
  while (current) {
    if (strcmp(current->name, name) == 0) {
      index = current;
      break;
    }
    current = (optimized_index_t*)current->next;
  }
  
  if (!index) {
    pthread_mutex_unlock(&coll->lock);
    LOG_ERROR("Index not found: %s", name);
    return NULL;
  }
  
  /* Lock index for reading */
  pthread_rwlock_rdlock(&index->lock);
  
  /* Create stats object */
  json_value_t* stats = json_create_object();
  if (!stats) {
    pthread_rwlock_unlock(&index->lock);
    pthread_mutex_unlock(&coll->lock);
    LOG_ERROR("create stats object");
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
  json_object_set(stats, "entries", json_create_number(index->entries));
  json_object_set(stats, "buckets", json_create_number(index->num_buckets));
  
  /* Calculate load factor */
  double load_factor = (double)index->entries / index->num_buckets;
  json_object_set(stats, "load_factor", json_create_number(load_factor));
  
  /* Enhanced statistics */
  json_object_set(stats, "lookups", json_create_number(index->lookups));
  json_object_set(stats, "hits", json_create_number(index->hits));
  
  double hit_rate = index->lookups > 0 ? 
           (double)index->hits / index->lookups * 100.0 : 0.0;
  json_object_set(stats, "hit_rate_percent", json_create_number(hit_rate));
  
  json_object_set(stats, "collisions", json_create_number(index->collisions));
  json_object_set(stats, "longest_chain", json_create_number(index->longest_chain));
  json_object_set(stats, "resizes", json_create_number(index->resizes));
  
  double avg_lookup_time = index->lookups > 0 ? 
              (double)index->total_lookup_time / index->lookups / CLOCKS_PER_SEC * 1000.0 : 0.0;
  json_object_set(stats, "avg_lookup_time_ms", json_create_number(avg_lookup_time));
  
  double max_lookup_time = (double)index->max_lookup_time / CLOCKS_PER_SEC * 1000.0;
  json_object_set(stats, "max_lookup_time_ms", json_create_number(max_lookup_time));
  
  /* Calculate optimizations */
  json_value_t* optimizations = json_create_object();
  json_object_set(optimizations, "bloom_filter", json_create_boolean(index->bloom_filter != NULL));
  json_object_set(optimizations, "cache", json_create_boolean(index->cache != NULL));
  json_object_set(optimizations, "robin_hood_hashing", json_create_boolean(USE_ROBIN_HOOD_HASHING));
  
  if (index->cache) {
    pthread_mutex_lock(&index->cache->mutex);
    json_object_set(optimizations, "cache_size", json_create_number(index->cache->size));
    json_object_set(optimizations, "cache_capacity", json_create_number(index->cache->capacity));
    pthread_mutex_unlock(&index->cache->mutex);
  }
  
  json_object_set(stats, "optimizations", optimizations);
  
  /* Calculate additional distribution stats */
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
  
  json_object_set(stats, "empty_buckets", json_create_number(empty_buckets));
  json_object_set(stats, "max_chain_length", json_create_number(max_chain_length));
  
  /* Calculate average chain length */
  double avg_chain_length = num_chains > 0 ? (double)total_chain_length / num_chains : 0;
  json_object_set(stats, "avg_chain_length", json_create_number(avg_chain_length));
  
  /* Recommendations */
  json_value_t* recommendations = json_create_array();
  
  /* Check if load factor is too high */
  if (load_factor > 0.8) {
    json_array_append(recommendations, json_create_string("Consider increasing the index bucket size"));
  }
  
  /* Check if hit rate is low */
  if (index->lookups > 100 && hit_rate < 20.0) {
    json_array_append(recommendations, json_create_string("Low hit rate - verify query patterns"));
  }
  
  /* Check if chain length is too high */
  if (max_chain_length > 10) {
    json_array_append(recommendations, json_create_string("Long chains detected - consider optimizing hash function"));
  }
  
  json_object_set(stats, "recommendations", recommendations);
  
  /* Unlock index and collection */
  pthread_rwlock_unlock(&index->lock);
  pthread_mutex_unlock(&coll->lock);
  
  return stats;
}

/* Initialize the optimized index system */
void db_init_optimized_indexes(database_t* db) {
  LOG_INFO("Initializing optimized index system");
  
  /* Add configuration to database metadata */
  json_value_t* config = json_create_object();
  
  json_object_set(config, "DEFAULT_INDEX_BUCKETS", json_create_number(DEFAULT_INDEX_BUCKETS));
  json_object_set(config, "MAX_LOAD_FACTOR", json_create_number(MAX_LOAD_FACTOR));
  json_object_set(config, "MIN_LOAD_FACTOR", json_create_number(MIN_LOAD_FACTOR));
  json_object_set(config, "BLOOM_FILTER_ENABLED", json_create_boolean(BLOOM_FILTER_ENABLED));
  json_object_set(config, "INDEX_CACHE_ENABLED", json_create_boolean(INDEX_CACHE_ENABLED));
  json_object_set(config, "INDEX_CACHE_SIZE", json_create_number(INDEX_CACHE_SIZE));
  json_object_set(config, "USE_ROBIN_HOOD_HASHING", json_create_boolean(USE_ROBIN_HOOD_HASHING));
  
  /* Add config to database */
  if (db) {
    pthread_mutex_lock(&db->lock);
    /* Store config in database metadata */
    json_free(config);
    pthread_mutex_unlock(&db->lock);
  }
}