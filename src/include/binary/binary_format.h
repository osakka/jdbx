#ifndef BINARY_FORMAT_H
#define BINARY_FORMAT_H

#include <stdint.h>
#include <stddef.h>

/* Binary format version */
#define BINARY_FORMAT_VERSION 1

/* Magic number for binary format (JSDB) */
#define BINARY_FORMAT_MAGIC 0x4A534442

/* Binary format type codes */
typedef enum {
    /* Basic types */
    BIN_TYPE_NULL       = 0,
    BIN_TYPE_BOOLEAN    = 1,
    BIN_TYPE_INTEGER    = 2,
    BIN_TYPE_DOUBLE     = 3,
    BIN_TYPE_STRING     = 4,
    
    /* Container types */
    BIN_TYPE_ARRAY      = 5,
    BIN_TYPE_OBJECT     = 6,
    
    /* Special types */
    BIN_TYPE_TIMESTAMP  = 7,
    BIN_TYPE_BINARY     = 8,
    BIN_TYPE_UUID       = 9,
    
    /* Database structure types */
    BIN_TYPE_DATABASE   = 10,
    BIN_TYPE_COLLECTION = 11,
    BIN_TYPE_DOCUMENT   = 12,
    BIN_TYPE_INDEX      = 13,
    
    /* Extension type */
    BIN_TYPE_EXTENSION  = 127
} binary_type_t;

/* Binary format header */
typedef struct {
    uint32_t magic;          /* Magic number (JSDB) */
    uint16_t version;        /* Format version */
    uint16_t flags;          /* Format flags */
    uint64_t timestamp;      /* Creation timestamp */
    uint64_t db_size;        /* Total database size in bytes */
    uint32_t collection_count; /* Number of collections */
    uint32_t checksum;       /* Header checksum */
} __attribute__((packed)) binary_header_t;

/* Collection header */
typedef struct {
    uint32_t name_length;    /* Length of collection name */
    uint32_t document_count; /* Number of documents in collection */
    uint64_t data_offset;    /* Offset to collection data */
    uint64_t index_offset;   /* Offset to index data */
    uint32_t index_count;    /* Number of indexes */
    uint32_t checksum;       /* Collection header checksum */
} __attribute__((packed)) binary_collection_header_t;

/* Index header */
typedef struct {
    uint32_t name_length;    /* Length of index name */
    uint32_t field_length;   /* Length of indexed field path */
    uint8_t type;            /* Index type */
    uint32_t entry_count;    /* Number of entries in the index */
    uint64_t data_offset;    /* Offset to index data */
    uint32_t checksum;       /* Index header checksum */
} __attribute__((packed)) binary_index_header_t;

/* Document header */
typedef struct {
    uint32_t id_length;      /* Length of document ID */
    uint32_t data_length;    /* Length of document data */
    uint64_t data_offset;    /* Offset to document data */
    uint32_t checksum;       /* Document header checksum */
} __attribute__((packed)) binary_document_header_t;

/* Binary format value */
typedef struct {
    uint8_t type;            /* Type code */
    uint32_t size;           /* Size of value data in bytes */
    /* Data follows immediately after this header */
} __attribute__((packed)) binary_value_header_t;

/* Functions for binary format operations */

/* Binary serialization */
int binary_serialize_database(const char* path, void* db);
int binary_serialize_collection(void* collection, void* buffer, size_t* size);
int binary_serialize_document(void* document, void* buffer, size_t* size);
int binary_serialize_index(void* index, void* buffer, size_t* size);

/* Binary deserialization */
void* binary_deserialize_database(const char* path);
void* binary_deserialize_collection(void* buffer, size_t size);
void* binary_deserialize_document(void* buffer, size_t size);
void* binary_deserialize_index(void* buffer, size_t size);

/* Calculate checksum for binary data */
uint32_t binary_calculate_checksum(const void* data, size_t size);

/* Verify binary format header */
int binary_verify_header(const binary_header_t* header);

/* Convert values between JSON and binary formats */
int binary_value_from_json(void* json, void* buffer, size_t* size);
void* binary_value_to_json(void* buffer, size_t size);

/* Utility functions */
void* binary_malloc(size_t size);
void binary_free(void* ptr);
int binary_resize_buffer(void** buffer, size_t current_size, size_t new_size);

#endif /* BINARY_FORMAT_H */