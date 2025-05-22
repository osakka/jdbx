# JSONdb Binary Format Documentation

This document describes the binary format implementation for JSONdb, which provides significant performance improvements for large databases.

## Overview

JSONdb binary format is a custom variable-length format designed to optimize storage and access for JSON documents, collections, and indexes. The binary format is especially beneficial for:

- Large databases (>1GB)
- Balanced read/write workloads
- Analytical query patterns
- High throughput applications

The binary format can be used side-by-side with the existing JSON format, with automatic detection of the file format during load operations.

## Format Structure

### File Layout

A binary format database file consists of:

1. Database header
2. Collection headers
3. Collection data (documents)
4. Index headers
5. Index data

### Header Structures

#### Database Header

The database header is always at the beginning of the file and has the following structure:

```c
typedef struct {
    uint32_t magic;          /* Magic number (JSDB) */
    uint16_t version;        /* Format version */
    uint16_t flags;          /* Format flags */
    uint64_t timestamp;      /* Creation timestamp */
    uint64_t db_size;        /* Total database size in bytes */
    uint32_t collection_count; /* Number of collections */
    uint32_t checksum;       /* Header checksum */
} binary_header_t;
```

- `magic`: The magic number 0x4A534442 ("JSDB")
- `version`: The binary format version (currently 1)
- `flags`: Various format flags (currently unused)
- `timestamp`: Creation timestamp in seconds since epoch
- `db_size`: Total size of the database file in bytes
- `collection_count`: Number of collections in the database
- `checksum`: CRC32 checksum of the header

#### Collection Header

Each collection has a header with the following structure:

```c
typedef struct {
    uint32_t name_length;    /* Length of collection name */
    uint32_t document_count; /* Number of documents in collection */
    uint64_t data_offset;    /* Offset to collection data */
    uint64_t index_offset;   /* Offset to index data */
    uint32_t index_count;    /* Number of indexes */
    uint32_t checksum;       /* Collection header checksum */
} binary_collection_header_t;
```

- `name_length`: Length of the collection name in bytes
- `document_count`: Number of documents in the collection
- `data_offset`: Offset to the collection data within the file
- `index_offset`: Offset to the index data within the file
- `index_count`: Number of indexes for this collection
- `checksum`: CRC32 checksum of the header

#### Index Header

Each index has a header with the following structure:

```c
typedef struct {
    uint32_t name_length;    /* Length of index name */
    uint32_t field_length;   /* Length of indexed field path */
    uint8_t type;            /* Index type */
    uint32_t entry_count;    /* Number of entries in the index */
    uint64_t data_offset;    /* Offset to index data */
    uint32_t checksum;       /* Index header checksum */
} binary_index_header_t;
```

- `name_length`: Length of the index name in bytes
- `field_length`: Length of the indexed field path in bytes
- `type`: Type of the index (e.g., unique, non-unique, text, geospatial)
- `entry_count`: Number of entries in the index
- `data_offset`: Offset to the index data within the file
- `checksum`: CRC32 checksum of the header

### Value Encoding

JSON values are encoded using a type-length-value approach:

```c
typedef struct {
    uint8_t type;            /* Type code */
    uint32_t size;           /* Size of value data in bytes */
    /* Data follows immediately after this header */
} binary_value_header_t;
```

- `type`: Type code indicating the JSON type (null, boolean, number, string, array, object)
- `size`: Size of the value data in bytes

The data format depends on the type:

- `BIN_TYPE_NULL`: No additional data
- `BIN_TYPE_BOOLEAN`: Single byte (0 or 1)
- `BIN_TYPE_INTEGER`: 64-bit signed integer
- `BIN_TYPE_DOUBLE`: 64-bit IEEE 754 floating-point number
- `BIN_TYPE_STRING`: Length-prefixed string
- `BIN_TYPE_ARRAY`: Count, followed by elements
- `BIN_TYPE_OBJECT`: Count, followed by key-value pairs

## Performance Considerations

### Size Efficiency

The binary format is more space-efficient than JSON for:

- Numeric values (8 bytes instead of variable-length strings)
- Structural metadata (compact binary headers)
- Field name repetition (reduced through binary structure)

### Access Efficiency

The binary format improves access efficiency through:

- Direct offset-based access to collections and indexes
- Reduced parsing overhead
- Compact in-memory representation

### Serialization/Deserialization

The binary format significantly improves serialization and deserialization speed:

- No text parsing/generation required
- Single-pass serialization and deserialization
- Reduced memory allocation during load/save operations

## Usage Guidelines

### When to Use Binary Format

The binary format is recommended for:

- Databases larger than 100MB
- Applications with high throughput requirements
- Environments with memory constraints
- Analytical workloads with complex queries

### When to Use JSON Format

The JSON format may still be preferred for:

- Small databases
- Development and debugging
- Human-readable data storage
- Applications where schema flexibility is critical

## API Usage

### Initialization

To initialize a database with binary format support:

```c
database_t* db = db_binary_init(path);
```

### Explicit Format Selection

To explicitly use binary format for save/load:

```c
// Save in binary format
binary_serialize_database(path, db);

// Load from binary format
database_t* db = binary_deserialize_database(path);
```

### Automatic Format Detection

The wrapper functions will automatically detect and use the appropriate format:

```c
// Initialize database (auto-detects format)
database_t* db = db_init(path);

// Save database (uses binary format for large databases)
db_save(db);

// Load database (auto-detects format)
db_load(db);
```

## Future Enhancements

Planned enhancements for the binary format:

1. Compression support for large text values
2. Incremental update capability
3. Memory-mapped file support
4. Schema-aware encoding for further size reduction
5. Transaction log in binary format
6. Binary-optimized query execution
7. Direct binary format generation from non-JSON sources

## Summary

The JSONdb binary format provides significant performance improvements for large databases while maintaining compatibility with the existing JSON format. It is especially beneficial for applications with high throughput requirements and large data volumes.