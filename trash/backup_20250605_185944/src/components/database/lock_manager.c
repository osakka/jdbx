#include "transaction/transaction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

/* Lock types */
typedef enum {
  LOCK_NONE = 0,
  LOCK_SHARED = 1,  /* Read lock (shared) */
  LOCK_EXCLUSIVE = 2 /* Write lock (exclusive) */
} lock_type_t;

/* Lock request status */
typedef enum {
  LOCK_STATUS_GRANTED, /* Lock was granted */
  LOCK_STATUS_WAITING, /* Lock request is waiting */
  LOCK_STATUS_DEADLOCK, /* Lock request would cause deadlock */
  LOCK_STATUS_ERROR   /* Error occurred during lock request */
} lock_status_t;

/* Lock key - identifies a resource to lock */
typedef struct {
  char* collection;   /* Collection name */
  char* document_id;   /* Document ID (NULL for collection locks) */
} lock_key_t;

/* Lock request */
typedef struct lock_request lock_request_t;
struct lock_request {
  transaction_t* transaction;  /* Requesting transaction */
  lock_type_t lock_type;    /* Type of lock requested */
  lock_status_t status;     /* Status of the request */
  lock_request_t* next;     /* Next request in queue */
};

/* Lock entry - represents locks on a specific resource */
typedef struct {
  lock_key_t key;         /* Resource identifier */
  lock_type_t current_lock_type;  /* Current lock type held */
  int reader_count;        /* Number of readers (for shared locks) */
  lock_request_t* waiting_queue;  /* Queue of waiting lock requests */
  lock_request_t* granted_queue;  /* Queue of granted lock requests */
  pthread_cond_t cond;       /* Condition variable for waiting */
} lock_entry_t;

/* Hash table entry for lock table */
typedef struct lock_table_entry lock_table_entry_t;
struct lock_table_entry {
  lock_entry_t* lock_entry;    /* Lock entry */
  lock_table_entry_t* next;    /* Next entry in hash chain */
};

/* Lock manager implementation fields */
typedef struct {
  lock_table_entry_t** lock_table; /* Hash table of locks */
  int table_size;          /* Size of hash table */
  int deadlock_detection;      /* Whether deadlock detection is enabled */
  int timeout_ms;          /* Lock timeout in milliseconds (0 = no timeout) */
} lock_manager_impl_t;

/* Global lock manager state added to the lock_manager struct from transaction.h */
static lock_manager_impl_t g_lock_manager_impl;

/* Additional manager fields - added outside the struct to avoid redefinition */
static int g_lock_manager_deadlock_detection = 1; /* Enable deadlock detection by default */
static int g_lock_manager_timeout_ms = 30000;   /* Default timeout: 30 seconds */

/* Hash function for lock keys */
static unsigned int hash_key(const lock_key_t* key, int table_size) {
  unsigned int hash = 0;
  const char* str = key->collection;
  
  /* Hash the collection name */
  while (*str) {
    hash = (hash * 31) + *str++;
  }
  
  /* Hash the document ID if present */
  if (key->document_id) {
    str = key->document_id;
    while (*str) {
      hash = (hash * 31) + *str++;
    }
  }
  
  return hash % table_size;
}

/* Compare two lock keys */
static int compare_keys(const lock_key_t* key1, const lock_key_t* key2) {
  /* First compare collection names */
  int coll_cmp = strcmp(key1->collection, key2->collection);
  if (coll_cmp != 0) {
    return coll_cmp;
  }
  
  /* If collection names are equal, compare document IDs */
  /* NULL document IDs are considered less than non-NULL ones */
  if (!key1->document_id && !key2->document_id) {
    return 0;
  } else if (!key1->document_id) {
    return -1;
  } else if (!key2->document_id) {
    return 1;
  } else {
    return strcmp(key1->document_id, key2->document_id);
  }
}

/* Create a new lock key */
static lock_key_t* create_lock_key(const char* collection, const char* document_id) {
  if (!collection) {
    return NULL;
  }
  
  lock_key_t* key = (lock_key_t*)malloc(sizeof(lock_key_t));
  if (!key) {
    return NULL;
  }
  
  key->collection = strdup(collection);
  key->document_id = document_id ? strdup(document_id) : NULL;
  
  if (!key->collection || (document_id && !key->document_id)) {
    if (key->collection) free(key->collection);
    if (key->document_id) free(key->document_id);
    free(key);
    return NULL;
  }
  
  return key;
}

/* Free a lock key */
static void free_lock_key(lock_key_t* key) {
  if (!key) {
    return;
  }
  
  if (key->collection) {
    free(key->collection);
  }
  
  if (key->document_id) {
    free(key->document_id);
  }
  
  free(key);
}

/* Create a new lock request */
static lock_request_t* create_lock_request(transaction_t* transaction, lock_type_t lock_type) {
  if (!transaction) {
    return NULL;
  }
  
  lock_request_t* request = (lock_request_t*)malloc(sizeof(lock_request_t));
  if (!request) {
    return NULL;
  }
  
  request->transaction = transaction;
  request->lock_type = lock_type;
  request->status = LOCK_STATUS_WAITING;
  request->next = NULL;
  
  return request;
}

/* Free a lock request */
static void free_lock_request(lock_request_t* request) {
  if (request) {
    free(request);
  }
}

/* Create a new lock entry */
static lock_entry_t* create_lock_entry(const lock_key_t* key) {
  if (!key) {
    return NULL;
  }
  
  lock_entry_t* entry = (lock_entry_t*)malloc(sizeof(lock_entry_t));
  if (!entry) {
    return NULL;
  }
  
  /* Copy the key */
  entry->key.collection = strdup(key->collection);
  entry->key.document_id = key->document_id ? strdup(key->document_id) : NULL;
  
  if (!entry->key.collection || (key->document_id && !entry->key.document_id)) {
    if (entry->key.collection) free(entry->key.collection);
    if (entry->key.document_id) free(entry->key.document_id);
    free(entry);
    return NULL;
  }
  
  /* Initialize the entry */
  entry->current_lock_type = LOCK_NONE;
  entry->reader_count = 0;
  entry->waiting_queue = NULL;
  entry->granted_queue = NULL;
  pthread_cond_init(&entry->cond, NULL);
  
  return entry;
}

/* Free a lock entry */
static void free_lock_entry(lock_entry_t* entry) {
  if (!entry) {
    return;
  }
  
  /* Free the key */
  if (entry->key.collection) {
    free(entry->key.collection);
  }
  
  if (entry->key.document_id) {
    free(entry->key.document_id);
  }
  
  /* Free the request queues */
  lock_request_t* request = entry->waiting_queue;
  while (request) {
    lock_request_t* next = request->next;
    free_lock_request(request);
    request = next;
  }
  
  request = entry->granted_queue;
  while (request) {
    lock_request_t* next = request->next;
    free_lock_request(request);
    request = next;
  }
  
  /* Destroy the condition variable */
  pthread_cond_destroy(&entry->cond);
  
  /* Free the entry */
  free(entry);
}

/* Find a lock entry in the lock table */
static lock_entry_t* find_lock_entry(lock_manager_t* manager, const lock_key_t* key) {
  if (!manager || !key) {
    return NULL;
  }
  
  /* Hash the key */
  unsigned int hash = hash_key(key, g_lock_manager_impl.table_size);
  
  /* Search the hash chain */
  lock_table_entry_t* entry = g_lock_manager_impl.lock_table[hash];
  while (entry) {
    if (compare_keys(&entry->lock_entry->key, key) == 0) {
      return entry->lock_entry;
    }
    entry = entry->next;
  }
  
  return NULL;
}

/* Add a lock entry to the lock table */
static int add_lock_entry(lock_manager_t* manager, lock_entry_t* lock_entry) {
  if (!manager || !lock_entry) {
    return 0;
  }
  
  /* Hash the key */
  unsigned int hash = hash_key(&lock_entry->key, g_lock_manager_impl.table_size);
  
  /* Create a table entry */
  lock_table_entry_t* entry = (lock_table_entry_t*)malloc(sizeof(lock_table_entry_t));
  if (!entry) {
    return 0;
  }
  
  /* Set the entry */
  entry->lock_entry = lock_entry;
  
  /* Add to the hash chain */
  entry->next = g_lock_manager_impl.lock_table[hash];
  g_lock_manager_impl.lock_table[hash] = entry;
  
  return 1;
}

/* Remove a lock entry from the lock table */
static int remove_lock_entry(lock_manager_t* manager, const lock_key_t* key) {
  if (!manager || !key) {
    return 0;
  }
  
  /* Hash the key */
  unsigned int hash = hash_key(key, g_lock_manager_impl.table_size);
  
  /* Search the hash chain */
  lock_table_entry_t** prev = &g_lock_manager_impl.lock_table[hash];
  lock_table_entry_t* entry = *prev;
  
  while (entry) {
    if (compare_keys(&entry->lock_entry->key, key) == 0) {
      /* Remove the entry from the chain */
      *prev = entry->next;
      
      /* Free the lock entry */
      free_lock_entry(entry->lock_entry);
      
      /* Free the table entry */
      free(entry);
      
      return 1;
    }
    
    prev = &entry->next;
    entry = entry->next;
  }
  
  return 0;
}

/* Check if a lock request is compatible with existing locks */
static int is_lock_compatible(lock_entry_t* entry, lock_request_t* request) {
  if (!entry || !request) {
    return 0;
  }
  
  /* If there are no existing locks, any lock is compatible */
  if (entry->current_lock_type == LOCK_NONE) {
    return 1;
  }
  
  /* If the entry has an exclusive lock, nothing else is compatible */
  if (entry->current_lock_type == LOCK_EXCLUSIVE) {
    return 0;
  }
  
  /* If the entry has shared locks, only shared locks are compatible */
  if (entry->current_lock_type == LOCK_SHARED) {
    return request->lock_type == LOCK_SHARED;
  }
  
  return 0;
}

/* Add a lock request to a lock entry's waiting queue */
static void add_waiting_request(lock_entry_t* entry, lock_request_t* request) {
  if (!entry || !request) {
    return;
  }
  
  /* Add to the end of the waiting queue */
  if (!entry->waiting_queue) {
    entry->waiting_queue = request;
  } else {
    lock_request_t* last = entry->waiting_queue;
    while (last->next) {
      last = last->next;
    }
    last->next = request;
  }
  
  request->next = NULL;
}

/* Add a lock request to a lock entry's granted queue */
static void add_granted_request(lock_entry_t* entry, lock_request_t* request) {
  if (!entry || !request) {
    return;
  }
  
  /* Update the entry's lock type */
  if (entry->current_lock_type == LOCK_NONE) {
    entry->current_lock_type = request->lock_type;
  }
  
  /* Update reader count for shared locks */
  if (request->lock_type == LOCK_SHARED) {
    entry->reader_count++;
  }
  
  /* Add to the granted queue */
  request->next = entry->granted_queue;
  entry->granted_queue = request;
  
  /* Update the request status */
  request->status = LOCK_STATUS_GRANTED;
}

/* Remove a lock request from a lock entry's granted queue */
static int remove_granted_request(lock_entry_t* entry, transaction_t* transaction) {
  if (!entry || !transaction) {
    return 0;
  }
  
  /* Search for the request */
  lock_request_t** prev = &entry->granted_queue;
  lock_request_t* request = entry->granted_queue;
  
  while (request) {
    if (request->transaction == transaction) {
      /* Remove the request from the queue */
      *prev = request->next;
      
      /* Update reader count for shared locks */
      if (request->lock_type == LOCK_SHARED) {
        entry->reader_count--;
      }
      
      /* Update the entry's lock type */
      if (!entry->granted_queue) {
        entry->current_lock_type = LOCK_NONE;
      } else if (request->lock_type == LOCK_EXCLUSIVE) {
        /* If we removed an exclusive lock, update lock type based on remaining locks */
        entry->current_lock_type = LOCK_SHARED;
      }
      
      /* Free the request */
      free_lock_request(request);
      
      return 1;
    }
    
    prev = &request->next;
    request = request->next;
  }
  
  return 0;
}

/* Process waiting lock requests for a lock entry */
static void process_waiting_requests(lock_entry_t* entry) {
  if (!entry) {
    return;
  }
  
  /* If there are no waiting requests, nothing to do */
  if (!entry->waiting_queue) {
    return;
  }
  
  /* Process the waiting queue in order (FIFO) */
  lock_request_t* request = entry->waiting_queue;
  lock_request_t* prev = NULL;
  
  while (request) {
    /* Check if the request is compatible with existing locks */
    if (is_lock_compatible(entry, request)) {
      /* Remove the request from the waiting queue */
      if (prev) {
        prev->next = request->next;
      } else {
        entry->waiting_queue = request->next;
      }
      
      /* Save the next request */
      lock_request_t* next = request->next;
      
      /* Add the request to the granted queue */
      add_granted_request(entry, request);
      
      /* Signal waiting threads */
      pthread_cond_broadcast(&entry->cond);
      
      /* Move to the next request */
      request = next;
    } else {
      /* Request cannot be granted yet, move to the next one */
      prev = request;
      request = request->next;
    }
  }
}

/* Create a lock manager */
lock_manager_t* lock_manager_create(void) {
  /* Allocate the lock manager */
  lock_manager_t* manager = (lock_manager_t*)malloc(sizeof(lock_manager_t));
  if (!manager) {
    return NULL;
  }
  
  /* Set default values */
  g_lock_manager_impl.table_size = 1024; /* Default hash table size */
  /* Global variables for deadlock_detection and timeout_ms already initialized */
  
  /* Initialize the lock mutex */
  pthread_mutex_init(&manager->lock, NULL);
  
  /* Allocate the lock table */
  g_lock_manager_impl.lock_table = (lock_table_entry_t**)calloc(g_lock_manager_impl.table_size, sizeof(lock_table_entry_t*));
  if (!g_lock_manager_impl.lock_table) {
    pthread_mutex_destroy(&manager->lock);
    free(manager);
    return NULL;
  }
  
  return manager;
}

/* Free a lock manager */
void lock_manager_free(lock_manager_t* manager) {
  if (!manager) {
    return;
  }
  
  /* Acquire the lock */
  pthread_mutex_lock(&manager->lock);
  
  /* Free all lock entries */
  for (int i = 0; i < g_lock_manager_impl.table_size; i++) {
    lock_table_entry_t* entry = g_lock_manager_impl.lock_table[i];
    while (entry) {
      lock_table_entry_t* next = entry->next;
      free_lock_entry(entry->lock_entry);
      free(entry);
      entry = next;
    }
  }
  
  /* Free the lock table */
  free(g_lock_manager_impl.lock_table);
  
  /* Release the lock */
  pthread_mutex_unlock(&manager->lock);
  
  /* Destroy the lock */
  pthread_mutex_destroy(&manager->lock);
  
  /* Free the manager */
  free(manager);
}

/* Acquire a lock on a document */
int lock_manager_lock_document(lock_manager_t* manager, const char* collection, 
               const char* document_id, transaction_t* transaction) {
  if (!manager || !collection || !transaction) {
    return 0;
  }
  
  /* Determine lock type based on isolation level */
  lock_type_t lock_type;
  switch (transaction->isolation_level) {
    case ISOLATION_READ_UNCOMMITTED:
      /* No locks needed for reads, exclusive for writes */
      /* This is handled at the transaction level, not here */
      lock_type = LOCK_EXCLUSIVE;
      break;
      
    case ISOLATION_READ_COMMITTED:
      /* Shared locks for reads, exclusive for writes */
      /* This is also handled at the transaction level */
      lock_type = LOCK_EXCLUSIVE;
      break;
      
    case ISOLATION_SERIALIZABLE:
      /* Exclusive locks for everything in serializable mode */
      lock_type = LOCK_EXCLUSIVE;
      break;
      
    default:
      lock_type = LOCK_EXCLUSIVE;
      break;
  }
  
  /* Create a lock key for the document */
  lock_key_t* key = create_lock_key(collection, document_id);
  if (!key) {
    return 0;
  }
  
  /* Acquire the lock manager lock */
  pthread_mutex_lock(&manager->lock);
  
  /* Find or create a lock entry */
  lock_entry_t* entry = find_lock_entry(manager, key);
  if (!entry) {
    /* Create a new entry */
    entry = create_lock_entry(key);
    if (!entry) {
      pthread_mutex_unlock(&manager->lock);
      free_lock_key(key);
      return 0;
    }
    
    /* Add the entry to the lock table */
    if (!add_lock_entry(manager, entry)) {
      free_lock_entry(entry);
      pthread_mutex_unlock(&manager->lock);
      free_lock_key(key);
      return 0;
    }
  }
  
  /* Free the key since we don't need it anymore */
  free_lock_key(key);
  
  /* Create a lock request */
  lock_request_t* request = create_lock_request(transaction, lock_type);
  if (!request) {
    pthread_mutex_unlock(&manager->lock);
    return 0;
  }
  
  /* Check if the lock can be granted immediately */
  if (is_lock_compatible(entry, request)) {
    /* Add the request to the granted queue */
    add_granted_request(entry, request);
    pthread_mutex_unlock(&manager->lock);
    return 1;
  }
  
  /* Add the request to the waiting queue */
  add_waiting_request(entry, request);
  
  /* Wait for the lock to be granted or timeout */
  struct timespec timeout;
  clock_gettime(CLOCK_REALTIME, &timeout);
  timeout.tv_sec += g_lock_manager_timeout_ms / 1000;
  timeout.tv_nsec += (g_lock_manager_timeout_ms % 1000) * 1000000;
  
  /* Normalize the timespec */
  if (timeout.tv_nsec >= 1000000000) {
    timeout.tv_sec += 1;
    timeout.tv_nsec -= 1000000000;
  }
  
  /* Wait for the lock with timeout */
  int result = 0;
  while (request->status == LOCK_STATUS_WAITING) {
    /* Wait for the condition variable */
    int wait_result = pthread_cond_timedwait(&entry->cond, &manager->lock, &timeout);
    
    /* If we timed out, perform deadlock detection if enabled */
    if (wait_result == ETIMEDOUT) {
      if (g_lock_manager_deadlock_detection) {
        /* Check for deadlocks */
        int deadlock = lock_manager_check_deadlocks(manager);
        if (deadlock) {
          /* Deadlock detected, mark this request as deadlocked */
          request->status = LOCK_STATUS_DEADLOCK;
          result = 0;
          break;
        }
        
        /* If no deadlock, just retry a few times before giving up */
        static int retry_count = 0;
        if (retry_count < 3) {
          retry_count++;
          continue; /* Try again */
        }
        
        retry_count = 0; /* Reset for next time */
      }
      
      /* If deadlock detection is disabled or retries exhausted, just timeout */
      request->status = LOCK_STATUS_ERROR;
      result = 0;
      break;
    }
    
    /* Process waiting requests to see if we got the lock */
    process_waiting_requests(entry);
  }
  
  /* If we got here, either we got the lock or there was an error */
  if (request->status == LOCK_STATUS_GRANTED) {
    result = 1;
  } else {
    /* Remove the request from the waiting queue */
    lock_request_t** prev = &entry->waiting_queue;
    lock_request_t* curr = entry->waiting_queue;
    
    while (curr) {
      if (curr == request) {
        *prev = curr->next;
        free_lock_request(request);
        break;
      }
      
      prev = &curr->next;
      curr = curr->next;
    }
    
    result = 0;
  }
  
  /* Release the lock manager lock */
  pthread_mutex_unlock(&manager->lock);
  
  return result;
}

/* Release a lock on a document */
int lock_manager_unlock_document(lock_manager_t* manager, const char* collection, 
                const char* document_id, transaction_t* transaction) {
  if (!manager || !collection || !transaction) {
    return 0;
  }
  
  /* Create a lock key for the document */
  lock_key_t* key = create_lock_key(collection, document_id);
  if (!key) {
    return 0;
  }
  
  /* Acquire the lock manager lock */
  pthread_mutex_lock(&manager->lock);
  
  /* Find the lock entry */
  lock_entry_t* entry = find_lock_entry(manager, key);
  
  /* Free the key since we don't need it anymore */
  free_lock_key(key);
  
  /* If the entry doesn't exist, we can't unlock it */
  if (!entry) {
    pthread_mutex_unlock(&manager->lock);
    return 0;
  }
  
  /* Remove the request from the granted queue */
  int result = remove_granted_request(entry, transaction);
  
  /* If we released the lock, process waiting requests */
  if (result) {
    process_waiting_requests(entry);
  }
  
  /* If there are no more locks on this resource, remove the entry */
  if (entry->current_lock_type == LOCK_NONE && !entry->waiting_queue) {
    remove_lock_entry(manager, &entry->key);
  }
  
  /* Release the lock manager lock */
  pthread_mutex_unlock(&manager->lock);
  
  return result;
}

/* Acquire a shared (read) lock on a document */
int lock_manager_lock_document_shared(lock_manager_t* manager, const char* collection, 
                  const char* document_id, transaction_t* transaction) {
  if (!manager || !collection || !transaction) {
    return 0;
  }
  
  /* Create a lock key for the document */
  lock_key_t* key = create_lock_key(collection, document_id);
  if (!key) {
    return 0;
  }
  
  /* Acquire the lock manager lock */
  pthread_mutex_lock(&manager->lock);
  
  /* Find or create a lock entry */
  lock_entry_t* entry = find_lock_entry(manager, key);
  if (!entry) {
    /* Create a new entry */
    entry = create_lock_entry(key);
    if (!entry) {
      pthread_mutex_unlock(&manager->lock);
      free_lock_key(key);
      return 0;
    }
    
    /* Add the entry to the lock table */
    if (!add_lock_entry(manager, entry)) {
      free_lock_entry(entry);
      pthread_mutex_unlock(&manager->lock);
      free_lock_key(key);
      return 0;
    }
  }
  
  /* Free the key since we don't need it anymore */
  free_lock_key(key);
  
  /* Create a lock request for a shared lock */
  lock_request_t* request = create_lock_request(transaction, LOCK_SHARED);
  if (!request) {
    pthread_mutex_unlock(&manager->lock);
    return 0;
  }
  
  /* Check if the lock can be granted immediately */
  if (entry->current_lock_type != LOCK_EXCLUSIVE) {
    /* Add the request to the granted queue */
    add_granted_request(entry, request);
    pthread_mutex_unlock(&manager->lock);
    return 1;
  }
  
  /* Add the request to the waiting queue */
  add_waiting_request(entry, request);
  
  /* Wait for the lock to be granted or timeout */
  struct timespec timeout;
  clock_gettime(CLOCK_REALTIME, &timeout);
  timeout.tv_sec += g_lock_manager_timeout_ms / 1000;
  timeout.tv_nsec += (g_lock_manager_timeout_ms % 1000) * 1000000;
  
  /* Normalize the timespec */
  if (timeout.tv_nsec >= 1000000000) {
    timeout.tv_sec += 1;
    timeout.tv_nsec -= 1000000000;
  }
  
  /* Wait for the lock with timeout */
  int result = 0;
  while (request->status == LOCK_STATUS_WAITING) {
    /* Wait for the condition variable */
    int wait_result = pthread_cond_timedwait(&entry->cond, &manager->lock, &timeout);
    
    /* If we timed out, perform deadlock detection if enabled */
    if (wait_result == ETIMEDOUT) {
      if (g_lock_manager_deadlock_detection) {
        /* Check for deadlocks */
        int deadlock = lock_manager_check_deadlocks(manager);
        if (deadlock) {
          /* Deadlock detected, mark this request as deadlocked */
          request->status = LOCK_STATUS_DEADLOCK;
          result = 0;
          break;
        }
        
        /* If no deadlock, just retry a few times before giving up */
        static int retry_count = 0;
        if (retry_count < 3) {
          retry_count++;
          continue; /* Try again */
        }
        
        retry_count = 0; /* Reset for next time */
      }
      
      /* If deadlock detection is disabled or retries exhausted, just timeout */
      request->status = LOCK_STATUS_ERROR;
      result = 0;
      break;
    }
    
    /* Process waiting requests to see if we got the lock */
    process_waiting_requests(entry);
  }
  
  /* If we got here, either we got the lock or there was an error */
  if (request->status == LOCK_STATUS_GRANTED) {
    result = 1;
  } else {
    /* Remove the request from the waiting queue */
    lock_request_t** prev = &entry->waiting_queue;
    lock_request_t* curr = entry->waiting_queue;
    
    while (curr) {
      if (curr == request) {
        *prev = curr->next;
        free_lock_request(request);
        break;
      }
      
      prev = &curr->next;
      curr = curr->next;
    }
    
    result = 0;
  }
  
  /* Release the lock manager lock */
  pthread_mutex_unlock(&manager->lock);
  
  return result;
}

/* Configure the lock manager */
int lock_manager_configure(lock_manager_t* manager, int deadlock_detection, int timeout_ms) {
  if (!manager) {
    return 0;
  }
  
  /* Acquire the lock */
  pthread_mutex_lock(&manager->lock);
  
  /* Update the configuration */
  g_lock_manager_deadlock_detection = deadlock_detection;
  g_lock_manager_timeout_ms = timeout_ms;
  
  /* Release the lock */
  pthread_mutex_unlock(&manager->lock);
  
  return 1;
}

/* Get statistics about the lock manager */
json_value_t* lock_manager_get_stats(lock_manager_t* manager) {
  if (!manager) {
    return NULL;
  }
  
  /* Acquire the lock */
  pthread_mutex_lock(&manager->lock);
  
  /* Create a JSON object for the statistics */
  json_value_t* stats = json_create_object();
  if (!stats) {
    pthread_mutex_unlock(&manager->lock);
    return NULL;
  }
  
  /* Count locks and resources */
  int total_resources = 0;
  int shared_locks = 0;
  int exclusive_locks = 0;
  int waiting_requests = 0;
  
  for (int i = 0; i < g_lock_manager_impl.table_size; i++) {
    lock_table_entry_t* entry = g_lock_manager_impl.lock_table[i];
    while (entry) {
      total_resources++;
      
      /* Count granted locks */
      lock_request_t* request = entry->lock_entry->granted_queue;
      while (request) {
        if (request->lock_type == LOCK_SHARED) {
          shared_locks++;
        } else if (request->lock_type == LOCK_EXCLUSIVE) {
          exclusive_locks++;
        }
        request = request->next;
      }
      
      /* Count waiting requests */
      request = entry->lock_entry->waiting_queue;
      while (request) {
        waiting_requests++;
        request = request->next;
      }
      
      entry = entry->next;
    }
  }
  
  /* Set the statistics */
  json_object_set(stats, "total_resources", json_create_integer(total_resources));
  json_object_set(stats, "shared_locks", json_create_integer(shared_locks));
  json_object_set(stats, "exclusive_locks", json_create_integer(exclusive_locks));
  json_object_set(stats, "waiting_requests", json_create_integer(waiting_requests));
  json_object_set(stats, "deadlock_detection", json_create_boolean(g_lock_manager_deadlock_detection));
  json_object_set(stats, "timeout_ms", json_create_integer(g_lock_manager_timeout_ms));
  
  /* Release the lock */
  pthread_mutex_unlock(&manager->lock);
  
  return stats;
}

/* Transaction states for deadlock detection */
typedef enum {
  DL_NOT_VISITED, /* Transaction not yet visited in DFS */
  DL_IN_PROGRESS, /* Transaction is being processed in current DFS path */
  DL_VISITED    /* Transaction has been fully processed */
} deadlock_visit_state_t;

/* Wait-for graph node for deadlock detection */
typedef struct {
  transaction_t* transaction;      /* Transaction represented by this node */
  deadlock_visit_state_t state;     /* Visit state for cycle detection */
  transaction_t** waiting_for;     /* Array of transactions this one is waiting for */
  int waiting_count;          /* Number of transactions in waiting_for array */
  int waiting_capacity;         /* Capacity of waiting_for array */
} wait_for_node_t;

/* Add a waiting transaction to the node */
static int add_waiting_for(wait_for_node_t* node, transaction_t* waiting_for_tx) {
  if (!node || !waiting_for_tx) {
    return 0;
  }
  
  /* Check if we already have this transaction in the waiting_for list */
  for (int i = 0; i < node->waiting_count; i++) {
    if (node->waiting_for[i] == waiting_for_tx) {
      return 1; /* Already present */
    }
  }
  
  /* Check if we need to expand the array */
  if (node->waiting_count >= node->waiting_capacity) {
    int new_capacity = node->waiting_capacity * 2;
    if (new_capacity == 0) new_capacity = 4; /* Initial capacity */
    
    transaction_t** new_array = (transaction_t**)realloc(node->waiting_for, 
                              new_capacity * sizeof(transaction_t*));
    if (!new_array) {
      return 0; /* Memory allocation failed */
    }
    
    node->waiting_for = new_array;
    node->waiting_capacity = new_capacity;
  }
  
  /* Add the transaction to the waiting_for list */
  node->waiting_for[node->waiting_count++] = waiting_for_tx;
  return 1;
}

/* Build the wait-for graph for deadlock detection */
static wait_for_node_t* build_wait_for_graph(lock_manager_t* manager, int* node_count) {
  if (!manager || !node_count) {
    return NULL;
  }
  
  /* Create mapping from transactions to nodes */
  /* First pass: count active transactions */
  int max_transactions = 0;
  for (int i = 0; i < g_lock_manager_impl.table_size; i++) {
    lock_table_entry_t* entry = g_lock_manager_impl.lock_table[i];
    while (entry) {
      /* Count transactions in granted queue */
      lock_request_t* request = entry->lock_entry->granted_queue;
      while (request) {
        max_transactions++;
        request = request->next;
      }
      
      /* Count transactions in waiting queue */
      request = entry->lock_entry->waiting_queue;
      while (request) {
        max_transactions++;
        request = request->next;
      }
      
      entry = entry->next;
    }
  }
  
  if (max_transactions == 0) {
    *node_count = 0;
    return NULL; /* No transactions to analyze */
  }
  
  /* Allocate nodes array */
  wait_for_node_t* nodes = (wait_for_node_t*)calloc(max_transactions, sizeof(wait_for_node_t));
  if (!nodes) {
    *node_count = 0;
    return NULL;
  }
  
  /* Map of transaction pointers to node indices */
  transaction_t** tx_to_node = (transaction_t**)calloc(max_transactions, sizeof(transaction_t*));
  if (!tx_to_node) {
    free(nodes);
    *node_count = 0;
    return NULL;
  }
  
  /* Initialize nodes */
  int actual_count = 0;
  
  /* Second pass: identify all active transactions and create nodes */
  for (int i = 0; i < g_lock_manager_impl.table_size; i++) {
    lock_table_entry_t* entry = g_lock_manager_impl.lock_table[i];
    while (entry) {
      /* Process transactions in granted queue */
      lock_request_t* request = entry->lock_entry->granted_queue;
      while (request) {
        /* Check if we already have a node for this transaction */
        int found = 0;
        for (int j = 0; j < actual_count; j++) {
          if (nodes[j].transaction == request->transaction) {
            found = 1;
            break;
          }
        }
        
        /* If not found, create a new node */
        if (!found && actual_count < max_transactions) {
          nodes[actual_count].transaction = request->transaction;
          nodes[actual_count].state = DL_NOT_VISITED;
          nodes[actual_count].waiting_for = NULL;
          nodes[actual_count].waiting_count = 0;
          nodes[actual_count].waiting_capacity = 0;
          
          tx_to_node[actual_count] = request->transaction;
          actual_count++;
        }
        
        request = request->next;
      }
      
      /* Process transactions in waiting queue */
      request = entry->lock_entry->waiting_queue;
      while (request) {
        /* Check if we already have a node for this transaction */
        int found = 0;
        for (int j = 0; j < actual_count; j++) {
          if (nodes[j].transaction == request->transaction) {
            found = 1;
            break;
          }
        }
        
        /* If not found, create a new node */
        if (!found && actual_count < max_transactions) {
          nodes[actual_count].transaction = request->transaction;
          nodes[actual_count].state = DL_NOT_VISITED;
          nodes[actual_count].waiting_for = NULL;
          nodes[actual_count].waiting_count = 0;
          nodes[actual_count].waiting_capacity = 0;
          
          tx_to_node[actual_count] = request->transaction;
          actual_count++;
        }
        
        request = request->next;
      }
      
      entry = entry->next;
    }
  }
  
  /* Third pass: build the wait-for edges */
  for (int i = 0; i < g_lock_manager_impl.table_size; i++) {
    lock_table_entry_t* entry = g_lock_manager_impl.lock_table[i];
    while (entry) {
      /* Process waiting transactions */
      lock_request_t* waiting = entry->lock_entry->waiting_queue;
      while (waiting) {
        /* Find node index for this waiting transaction */
        int waiting_idx = -1;
        for (int j = 0; j < actual_count; j++) {
          if (nodes[j].transaction == waiting->transaction) {
            waiting_idx = j;
            break;
          }
        }
        
        if (waiting_idx >= 0) {
          /* This transaction is waiting; add edges to all transactions 
            that hold conflicting locks */
          lock_request_t* granted = entry->lock_entry->granted_queue;
          while (granted) {
            /* Only add edge if the lock types are conflicting */
            if ((granted->lock_type == LOCK_EXCLUSIVE) || 
              (waiting->lock_type == LOCK_EXCLUSIVE)) {
              /* Find node index for the transaction it's waiting for */
              int granted_idx = -1;
              for (int j = 0; j < actual_count; j++) {
                if (nodes[j].transaction == granted->transaction) {
                  granted_idx = j;
                  break;
                }
              }
              
              if (granted_idx >= 0) {
                /* Add edge: waiting -> granted */
                add_waiting_for(&nodes[waiting_idx], granted->transaction);
              }
            }
            
            granted = granted->next;
          }
        }
        
        waiting = waiting->next;
      }
      
      entry = entry->next;
    }
  }
  
  /* Cleanup */
  free(tx_to_node);
  
  *node_count = actual_count;
  return nodes;
}

/* Detect cycles in the wait-for graph using DFS */
static int detect_cycle(wait_for_node_t* nodes, int node_count, int start_idx, transaction_t** victim) {
  if (!nodes || start_idx < 0 || start_idx >= node_count) {
    return 0;
  }
  
  /* Mark the current node as being visited */
  nodes[start_idx].state = DL_IN_PROGRESS;
  
  /* Visit all adjacent vertices */
  for (int i = 0; i < nodes[start_idx].waiting_count; i++) {
    /* Find the node index for the transaction we're waiting for */
    int wait_idx = -1;
    for (int j = 0; j < node_count; j++) {
      if (nodes[j].transaction == nodes[start_idx].waiting_for[i]) {
        wait_idx = j;
        break;
      }
    }
    
    if (wait_idx >= 0) {
      /* If adjacent vertex is already in the recursion stack, we found a cycle */
      if (nodes[wait_idx].state == DL_IN_PROGRESS) {
        /* Choose the victim based on some policy */
        /* For now, we'll choose the transaction with the lowest ID as the victim */
        if (victim) {
          *victim = nodes[start_idx].transaction;
        }
        return 1;
      }
      
      /* If not visited yet, recursively check for cycles */
      if (nodes[wait_idx].state == DL_NOT_VISITED) {
        if (detect_cycle(nodes, node_count, wait_idx, victim)) {
          return 1;
        }
      }
    }
  }
  
  /* Mark the node as fully visited */
  nodes[start_idx].state = DL_VISITED;
  
  return 0;
}

/* Free the wait-for graph */
static void free_wait_for_graph(wait_for_node_t* nodes, int node_count) {
  if (!nodes) {
    return;
  }
  
  for (int i = 0; i < node_count; i++) {
    if (nodes[i].waiting_for) {
      free(nodes[i].waiting_for);
    }
  }
  
  free(nodes);
}

/* Select a victim transaction for deadlock resolution */
static transaction_t* select_deadlock_victim(wait_for_node_t* nodes, int node_count) {
  if (!nodes || node_count <= 0) {
    return NULL;
  }
  
  transaction_t* victim = NULL;
  time_t youngest_start_time = 0;
  
  /* Select the youngest transaction (most recently started) as the victim */
  for (int i = 0; i < node_count; i++) {
    if (nodes[i].transaction && nodes[i].state == DL_IN_PROGRESS) {
      /* This transaction is part of a cycle */
      time_t start_time = nodes[i].transaction->start_time;
      
      /* Initialize youngest_start_time if this is the first transaction we're checking */
      if (youngest_start_time == 0 || start_time > youngest_start_time) {
        youngest_start_time = start_time;
        victim = nodes[i].transaction;
      }
    }
  }
  
  return victim;
}

/* Abort a transaction due to deadlock */
static void abort_transaction_for_deadlock(transaction_t* transaction) {
  if (!transaction) {
    return;
  }
  
  pthread_mutex_lock(&transaction->lock);
  
  /* Only abort if the transaction is still active */
  if (transaction->state == TRANSACTION_ACTIVE) {
    /* Mark the transaction for abort */
    transaction->state = TRANSACTION_ABORTING;
    printf("Aborting transaction %s due to deadlock\n", transaction->id);
  }
  
  pthread_mutex_unlock(&transaction->lock);
}

/* Check for deadlocks in the lock manager and resolve them if found */
int lock_manager_check_deadlocks(lock_manager_t* manager) {
  if (!manager) {
    return 0;
  }
  
  /* If deadlock detection is disabled, just return */
  if (!g_lock_manager_deadlock_detection) {
    return 0;
  }
  
  /* Build the wait-for graph */
  int node_count = 0;
  wait_for_node_t* nodes = build_wait_for_graph(manager, &node_count);
  
  if (!nodes || node_count == 0) {
    return 0; /* No transactions or error building the graph */
  }
  
  /* Use DFS to detect cycles in the graph */
  int deadlock_detected = 0;
  transaction_t* victim = NULL;
  
  for (int i = 0; i < node_count; i++) {
    if (nodes[i].state == DL_NOT_VISITED) {
      if (detect_cycle(nodes, node_count, i, NULL)) {
        deadlock_detected = 1;
        break;
      }
    }
  }
  
  if (deadlock_detected) {
    /* Select a victim transaction to break the deadlock */
    victim = select_deadlock_victim(nodes, node_count);
    
    if (victim) {
      /* Log the deadlock resolution */
      printf("Deadlock detected! Transaction %s selected as victim.\n", victim->id);
      
      /* Abort the victim transaction to resolve the deadlock */
      abort_transaction_for_deadlock(victim);
      
      /* Mark all nodes as not visited for another DFS pass */
      for (int i = 0; i < node_count; i++) {
        nodes[i].state = DL_NOT_VISITED;
      }
      
      /* Check if there are still deadlocks after aborting the victim */
      /* This is important for complex deadlock scenarios */
      int still_deadlocked = 0;
      for (int i = 0; i < node_count; i++) {
        if (nodes[i].state == DL_NOT_VISITED && 
          nodes[i].transaction != victim) { /* Skip the aborted transaction */
          if (detect_cycle(nodes, node_count, i, NULL)) {
            still_deadlocked = 1;
            break;
          }
        }
      }
      
      /* If there are still deadlocks, recursively resolve them */
      if (still_deadlocked) {
        printf("Complex deadlock detected - resolving recursively\n");
        /* Free the current wait-for graph and call the function again */
        free_wait_for_graph(nodes, node_count);
        return lock_manager_check_deadlocks(manager);
      }
    }
  }
  
  /* Cleanup */
  free_wait_for_graph(nodes, node_count);
  
  return deadlock_detected;
}