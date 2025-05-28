#include "binary/binary_format.h"
#include "database/database.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/**
 * Binary format implementation for indexes
 * 
 * This file provides binary serialization/deserialization for index structures,
 * which significantly improves performance for large indexes.
 */

/* Binary index entry structure */
typedef struct {
  uint32_t doc_id_length;
  uint32_t key_length;
  /* document_id and key_value data follow */
} __attribute__((packed)) binary_index_entry_t;

/**
 * Calculate the size required for binary serialization of an index
 * 
 * @param index Index to calculate size for
 * @return Size in bytes
 */
size_t binary_index_size(index_t* index) {
  if (!index) return 0;
  
  size_t size = sizeof(binary_index_header_t);
  size += strlen(index->name);
  size += strlen(index->field_path);
  
  /* Add size for each entry */
  for (size_t i = 0; i < index->num_buckets; i++) {
    index_entry_t* entry = index->buckets[i];
    
    while (entry) {
      size += sizeof(binary_index_entry_t);
      size += strlen(entry->document_id);
      size += strlen(entry->key_value);
      
      entry = entry->next;
    }
  }
  
  return size;
}

/**
 * Serialize an index to binary format
 *
 * @param index Index to serialize
 * @param buffer Buffer to serialize into
 * @param size Size of the buffer (in/out parameter)
 * @return 1 on success, 0 on failure
 */
int binary_serialize_index(void* idx, void* buffer, size_t* size) {
  if (!idx || !buffer || !size) return 0;
  
  index_t* index = (index_t*)idx;
  
  /* Check if buffer is large enough */
  size_t required_size = binary_index_size(index);
  if (*size < required_size) {
    *size = required_size;
    return 0;
  }
  
  uint8_t* pos = (uint8_t*)buffer;
  
  /* Write index header */
  binary_index_header_t* header = (binary_index_header_t*)pos;
  header->name_length = (uint32_t)strlen(index->name);
  header->field_length = (uint32_t)strlen(index->field_path);
  header->type = (uint8_t)index->type;
  
  /* Count entries */
  uint32_t entry_count = 0;
  for (size_t i = 0; i < index->num_buckets; i++) {
    index_entry_t* entry = index->buckets[i];
    while (entry) {
      entry_count++;
      entry = entry->next;
    }
  }
  
  header->entry_count = entry_count;
  header->data_offset = sizeof(binary_index_header_t) + header->name_length + header->field_length;
  
  /* Calculate checksum */
  header->checksum = binary_calculate_checksum(header, sizeof(binary_index_header_t) - sizeof(uint32_t));
  
  pos += sizeof(binary_index_header_t);
  
  /* Write name */
  memcpy(pos, index->name, header->name_length);
  pos += header->name_length;
  
  /* Write field path */
  memcpy(pos, index->field_path, header->field_length);
  pos += header->field_length;
  
  /* Write entries */
  for (size_t i = 0; i < index->num_buckets; i++) {
    index_entry_t* entry = index->buckets[i];
    
    while (entry) {
      /* Write entry header */
      binary_index_entry_t* entry_header = (binary_index_entry_t*)pos;
      entry_header->doc_id_length = (uint32_t)strlen(entry->document_id);
      entry_header->key_length = (uint32_t)strlen(entry->key_value);
      
      pos += sizeof(binary_index_entry_t);
      
      /* Write document ID */
      memcpy(pos, entry->document_id, entry_header->doc_id_length);
      pos += entry_header->doc_id_length;
      
      /* Write key value */
      memcpy(pos, entry->key_value, entry_header->key_length);
      pos += entry_header->key_length;
      
      entry = entry->next;
    }
  }
  
  /* Update size */
  *size = (size_t)(pos - (uint8_t*)buffer);
  
  return 1;
}

/**
 * Deserialize an index from binary format
 *
 * @param buffer Buffer containing serialized index
 * @param size Size of the buffer
 * @return Deserialized index or NULL on failure
 */
void* binary_deserialize_index(void* buffer, size_t size) {
  if (!buffer || size < sizeof(binary_index_header_t)) return NULL;
  
  uint8_t* pos = (uint8_t*)buffer;
  
  /* Read header */
  binary_index_header_t* header = (binary_index_header_t*)pos;
  
  /* Verify header size */
  if (size < sizeof(binary_index_header_t) + header->name_length + header->field_length) {
    LOG_ERROR("Buffer too small for index header");
    return NULL;
  }
  
  /* Verify checksum */
  uint32_t calculated_checksum = binary_calculate_checksum(header, 
                  sizeof(binary_index_header_t) - sizeof(uint32_t));
  
  if (calculated_checksum != header->checksum) {
    LOG_ERROR("Index header checksum mismatch");
    return NULL;
  }
  
  pos += sizeof(binary_index_header_t);
  
  /* Read name */
  char* name = (char*)malloc(header->name_length + 1);
  if (!name) {
    LOG_ERROR("Out of memory");
    return NULL;
  }
  
  memcpy(name, pos, header->name_length);
  name[header->name_length] = '\0';
  pos += header->name_length;
  
  /* Read field path */
  char* field_path = (char*)malloc(header->field_length + 1);
  if (!field_path) {
    LOG_ERROR("Out of memory");
    free(name);
    return NULL;
  }
  
  memcpy(field_path, pos, header->field_length);
  field_path[header->field_length] = '\0';
  pos += header->field_length;
  
  /* Create index */
  size_t num_buckets = 128; /* Default number of buckets */
  index_t* index = (index_t*)malloc(sizeof(index_t));
  if (!index) {
    LOG_ERROR("Out of memory");
    free(name);
    free(field_path);
    return NULL;
  }
  
  /* Initialize index */
  index->name = name;
  index->field_path = field_path;
  index->type = (index_type_t)header->type;
  index->num_buckets = num_buckets;
  index->entries = 0;
  index->next = NULL;
  
  /* Initialize buckets */
  index->buckets = (index_entry_t**)calloc(num_buckets, sizeof(index_entry_t*));
  if (!index->buckets) {
    LOG_ERROR("Out of memory");
    free(name);
    free(field_path);
    free(index);
    return NULL;
  }
  
  /* Initialize lock */
  if (pthread_rwlock_init(&index->lock, NULL) != 0) {
    LOG_ERROR("initialize index lock");
    free(name);
    free(field_path);
    free(index->buckets);
    free(index);
    return NULL;
  }
  
  /* Read entries */
  for (uint32_t i = 0; i < header->entry_count; i++) {
    /* Verify buffer size */
    if (pos + sizeof(binary_index_entry_t) > (uint8_t*)buffer + size) {
      LOG_ERROR("Buffer overrun during index entry deserialization");
      /* Cleanup and return what we have so far */
      return index;
    }
    
    /* Read entry header */
    binary_index_entry_t* entry_header = (binary_index_entry_t*)pos;
    pos += sizeof(binary_index_entry_t);
    
    /* Verify buffer size for strings */
    if (pos + entry_header->doc_id_length + entry_header->key_length > (uint8_t*)buffer + size) {
      LOG_ERROR("Buffer overrun during index entry strings deserialization");
      /* Cleanup and return what we have so far */
      return index;
    }
    
    /* Read document ID */
    char* doc_id = (char*)malloc(entry_header->doc_id_length + 1);
    if (!doc_id) {
      LOG_ERROR("Out of memory");
      continue;
    }
    
    memcpy(doc_id, pos, entry_header->doc_id_length);
    doc_id[entry_header->doc_id_length] = '\0';
    pos += entry_header->doc_id_length;
    
    /* Read key value */
    char* key_value = (char*)malloc(entry_header->key_length + 1);
    if (!key_value) {
      LOG_ERROR("Out of memory");
      free(doc_id);
      continue;
    }
    
    memcpy(key_value, pos, entry_header->key_length);
    key_value[entry_header->key_length] = '\0';
    pos += entry_header->key_length;
    
    /* Create index entry */
    index_entry_t* entry = (index_entry_t*)malloc(sizeof(index_entry_t));
    if (!entry) {
      LOG_ERROR("Out of memory");
      free(doc_id);
      free(key_value);
      continue;
    }
    
    entry->document_id = doc_id;
    entry->key_value = key_value;
    entry->next = NULL;
    
    /* Calculate hash bucket */
    uint32_t hash = 0;
    for (size_t j = 0; j < strlen(key_value); j++) {
      hash = hash * 31 + key_value[j];
    }
    
    size_t bucket = hash % index->num_buckets;
    
    /* Add to bucket */
    entry->next = index->buckets[bucket];
    index->buckets[bucket] = entry;
    
    /* Increment entry count */
    index->entries++;
  }
  
  LOG_INFO("Deserialized index with %zu entries", index->entries);
  
  return index;
}

/**
 * Initialize binary index operations
 * 
 * This function should be called during startup to register
 * binary format handlers for indexes.
 */
void binary_index_init() {
  LOG_INFO("Initializing binary index operations");
  /* Any initialization can be done here */
}