# Cascading Versioning Policy System

## Overview

The JDBX cascading versioning policy system provides a hierarchical approach to document versioning that cascades from Library → Collection → Document levels. Each level can override settings from its parent, providing flexible versioning control.

## Architecture

### Policy Hierarchy

```
Library (Base Policy)
  └── Collection (Inherits + Overrides)
       └── Document (Inherits + Overrides)
```

### Default Policy

```c
{
    enabled: false,
    max_versions: 10,
    retention_days: 30,
    version_on_update: true,
    version_on_delete: true,
    compression_enabled: false,
    archive_old_versions: false,
    archive_after_days: 90
}
```

## Policy Settings

### Basic Settings
- **enabled**: Whether versioning is active
- **max_versions**: Maximum versions to retain (0 = unlimited)
- **retention_days**: Days to keep versions (0 = forever)

### Triggers
- **version_on_update**: Create version when document updated
- **version_on_delete**: Create version when document deleted

### Advanced Settings
- **compression_enabled**: Compress version snapshots
- **archive_old_versions**: Move old versions to archive
- **archive_after_days**: Days before archiving

## Implementation

### Key Components

1. **versioning_policy.c/h**: Core versioning logic
   - Policy loading and cascade logic
   - Version creation and management
   - Cleanup and retention enforcement

2. **Version Storage**: Versions stored in `{library}/versions` collection
   - Full document snapshots
   - Operation metadata (insert/update/delete/restore)
   - Timestamps and compression flags

### API Functions

```c
// Get policies at different levels
versioning_policy_t* versioning_policy_get_library(db, library_name);
versioning_policy_t* versioning_policy_get_collection(db, library_name, collection_name);
versioning_policy_t* versioning_policy_get_document(db, library_name, collection_name, document);

// Version operations
int versioning_create_version(db, library_name, collection_name, document, operation);
json_value_t* versioning_get_history(db, library_name, collection_name, document_id);
int versioning_restore_version(db, library_name, collection_name, version_id);
int versioning_cleanup_old_versions(db, library_name, collection_name, document_id, policy);
```

## Usage Examples

### Library-Level Policy

```json
{
  "type": "library",
  "name": "production",
  "settings": {
    "versioning": {
      "enabled": true,
      "maxVersions": 10,
      "retentionDays": 90,
      "compressionEnabled": true
    }
  }
}
```

### Collection Override

```json
{
  "type": "collection",
  "library": "production",
  "name": "audit_logs",
  "settings": {
    "versioning": {
      "maxVersions": 0,         // Override: unlimited versions
      "retentionDays": 365,     // Override: 1 year retention
      "archiveOldVersions": true // New setting
    }
  }
}
```

### Document-Level Override

```json
{
  "name": "Critical Config",
  "_versioning": {
    "versionOnDelete": false  // Don't version deletes for this doc
  }
}
```

## Version Document Format

```json
{
  "type": "version",
  "document_id": "abc-123",
  "collection": "configs",
  "library": "production",
  "operation": "update",
  "versioned_at": "2025-06-09T23:49:44Z",
  "snapshot": {
    // Complete document snapshot
  },
  "compressed": false
}
```

## Integration Points

### Database Operations
- Insert: Creates version if policy enabled
- Update: Creates version if version_on_update = true
- Delete: Creates version if version_on_delete = true
- Restore: Creates version of current state before restore

### Unified Documents
- Libraries and collections store versioning settings
- Policies loaded from documents collection
- Settings cascade through hierarchy

## Performance Considerations

1. **Policy Caching**: Policies cached to avoid repeated queries
2. **Async Cleanup**: Version cleanup runs asynchronously
3. **Compression**: Optional compression for large documents
4. **Archival**: Old versions can be moved to archive storage

## Future Enhancements

1. **Differential Versioning**: Store only changes, not full snapshots
2. **Version Comparison**: Built-in diff functionality
3. **Batch Operations**: Version multiple documents atomically
4. **S3 Archival**: Archive old versions to S3-compatible storage
5. **Version Branching**: Create version branches for experiments