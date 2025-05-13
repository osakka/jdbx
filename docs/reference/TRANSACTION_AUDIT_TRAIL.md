# Transaction Audit Trail

This document describes the enhanced transaction logging system with audit trail capabilities in the JSON database.

## Overview

The transaction audit trail extends the basic transaction logging system to provide comprehensive tracking of all database changes for security, compliance, and operational monitoring purposes. Key features include:

1. **Enhanced Logging Levels**: Configure the detail level of transaction logs
2. **Dedicated Audit Trail**: Separate audit logs for compliance and security
3. **Document History Tracking**: Track the complete change history of any document
4. **Operational Reports**: Generate insights into transaction patterns and usage
5. **Log Management**: Archive and compact logs based on retention policies

## Architecture

The audit trail system builds on the transaction logging infrastructure with additional capabilities:

```
┌─────────────────────┐    ┌───────────────────┐
│  Transaction System │───▶│  Transaction Log  │
└─────────────────────┘    └───────────────────┘
                                    │
                                    ▼
┌─────────────────────┐    ┌───────────────────┐
│ API/Admin Interface │◀───┤  Audit System     │
└─────────────────────┘    └───────────────────┘
                                    │
                                    ▼
                           ┌───────────────────┐
                           │   Audit Trail     │
                           └───────────────────┘
```

## Log Structure

### Enhanced Transaction Log

The transaction log format has been extended to include a new entry type, "AUDIT":

```
TIMESTAMP|TYPE|TRANSACTION_ID|DATA_JSON
```

Where `TYPE` can now be one of:
- "STATE" - Transaction state changes
- "OPERATION" - Document operations
- "AUDIT" - Detailed audit information

### Audit Trail Format

The dedicated audit trail file uses a simpler format:

```
TIMESTAMP|TRANSACTION_ID|DATA_JSON
```

The `DATA_JSON` for audit entries contains comprehensive information:

```json
{
  "state": "committed",
  "user_id": "user123",
  "client_ip": "192.168.1.100",
  "application_name": "inventory-system",
  "is_retry": false,
  "retry_count": 0,
  "error_code": 0,
  "duration_ms": 125,
  "operation_count": 3,
  "isolation_level": "serializable"
}
```

## Configuration Options

The audit trail system is highly configurable:

### Log Levels

- **0 (OFF)**: No logging (not recommended for production)
- **1 (BASIC)**: Log essential information - state changes and operations
- **2 (DETAILED)**: Log all information including full document contents and detailed audit entries

### Audit Trail Options

- **Audit Enabled**: Enable or disable the dedicated audit trail
- **Auto Archive**: Automatically archive old logs based on retention period
- **Retention Days**: Number of days to retain logs before archiving (1-365)

## API Endpoints

### Get Transaction Logs Status

```
GET /api/transactions/logs
```

Returns statistics about the transaction log and audit trail, including file sizes, entry counts, and current configuration.

**Response Example:**
```json
{
  "file_size": 1275392,
  "last_modified": 1714645982,
  "audit_file_size": 872644,
  "audit_last_modified": 1714645975,
  "log_level": 2,
  "audit_enabled": true,
  "auto_archive": true,
  "retention_days": 30,
  "total_entries": 15392,
  "state_entries": 1423,
  "operation_entries": 13642,
  "audit_entries": 327
}
```

### Configure Logging

```
POST /api/transactions/logs/configure
```

Configure transaction logging parameters.

**Request Body:**
```json
{
  "log_level": 2,
  "audit_enabled": true,
  "auto_archive": true,
  "retention_days": 30
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Configuration updated",
  "log_level": 2,
  "audit_enabled": true,
  "auto_archive": true,
  "retention_days": 30
}
```

### Archive Logs

```
POST /api/transactions/logs/archive
```

Archive transaction logs to a specified directory.

**Request Body:**
```json
{
  "archive_dir": "/var/archives/jsondb"
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Transaction logs archived successfully"
}
```

### Get Transaction Report

```
GET /api/transactions/logs/report?start_time=1609459200&end_time=1612137600
```

Generate a report of transactions within a specified time period.

**Response Example:**
```json
{
  "start_time": 1609459200,
  "end_time": 1612137600,
  "stats": {
    "total_transactions": 1237,
    "active_transactions": 3,
    "committed_transactions": 1198,
    "aborted_transactions": 36,
    "total_operations": 8742,
    "insert_operations": 2341,
    "update_operations": 5982,
    "delete_operations": 419,
    "errors": 12
  },
  "transactions": [
    {
      "id": "tx_a1b2c3",
      "timestamp": 1610123456,
      "state": "committed",
      "user_id": "admin"
    },
    // ... more transactions
  ]
}
```

### Get Document History

```
GET /api/transactions/logs/document-history?collection=users&document_id=user123
```

Retrieve the complete history of a document, showing all changes over time.

**Response Example:**
```json
{
  "collection": "users",
  "document_id": "user123",
  "change_count": 5,
  "changes": [
    {
      "timestamp": 1609459821,
      "transaction_id": "tx_f9e8d7",
      "operation": "insert",
      "after_state": {
        "_id": "user123",
        "name": "John Doe",
        "email": "john@example.com"
      }
    },
    {
      "timestamp": 1609892456,
      "transaction_id": "tx_a7b8c9",
      "operation": "update",
      "before_state": {
        "_id": "user123",
        "name": "John Doe",
        "email": "john@example.com"
      },
      "after_state": {
        "_id": "user123",
        "name": "John Doe",
        "email": "john.doe@company.com"
      }
    },
    // ... more changes
  ]
}
```

## Implementation Details

### Log File Management

- Both transaction log and audit files are protected by mutex locks to ensure thread safety
- Log files are created with appropriate permissions (0755 for directories)
- `fsync()` is used after writes to ensure durability
- Headers in log files document their format and creation time

### Archive Process

When logs are archived:
1. A timestamped directory is created in the archive location
2. Both transaction log and audit files are copied to the archive
3. Original files are truncated but headers are preserved
4. Archive information is recorded in the new headers

### Log Compaction Logic

The log compaction algorithm:
1. First pass: identifies all completed transactions (committed or aborted)
2. Second pass: preserves only:
   - All entries for active transactions
   - Final state entries for completed transactions
   - All audit entries (for compliance purposes)

### Security Considerations

- Sensitive data in document content may be logged depending on log level
- Proper file permissions should be set on log and audit files
- Consider encryption for audit files in highly sensitive environments
- API endpoints for log configuration and retrieval should be restricted to admin users

## Limitations and Future Work

### Current Limitations

- No built-in encryption for log files
- No real-time log streaming or alerting
- Document history tracking can become resource-intensive for frequently modified documents

### Planned Enhancements

1. **Log encryption** for enhanced security
2. **Real-time log monitoring** with alerts for suspicious activities
3. **Advanced filtering** for more targeted reporting
4. **Visualization tools** for audit data analysis
5. **Integration with external logging systems** (e.g., Splunk, ELK stack)