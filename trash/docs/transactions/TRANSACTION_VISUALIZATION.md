# Transaction Visualization

This document outlines the transaction visualization capabilities in the JSON database admin interface.

## Overview

The transaction visualization system provides comprehensive tools for monitoring, analyzing, and visualizing database transactions. It helps database administrators understand transaction patterns, identify performance issues, and troubleshoot concurrency problems.

The system offers three different visualization perspectives:

1. **Timeline Visualization**: Shows transaction state changes over time
2. **Metrics Visualization**: Provides quantitative analysis of transaction patterns
3. **Relationship Visualization**: Displays the connections between transactions and documents

## Architecture

The transaction visualization system consists of three main components:

### 1. Backend API (Transaction Visualization API)

Located in `/src/transaction_visualization.c`, the backend API provides endpoints for:

- `/api/visualization/transaction-history`: Returns transaction state changes and operations over time
- `/api/visualization/transaction-metrics`: Provides aggregated metrics based on various dimensions
- `/api/visualization/transaction-relationships`: Returns the graph data showing connections between transactions and documents

These endpoints process the transaction log data and format it for visualization in the admin UI.

### 2. Admin UI Components

The transaction visualization UI is integrated into the admin interface with:

- Transaction visualization tab in the admin sidebar
- Three visualization views (Timeline, Metrics, Relationships)
- Interactive filters and controls
- Real-time transaction statistics

### 3. Visualization Libraries

The system uses:

- Chart.js for timeline and metrics visualizations
- vis.js for network graph visualization of transaction relationships

## Features

### Transaction Timeline

The timeline visualization shows transaction state changes over time:

- Line chart of transaction states (active, committing, committed, aborting, aborted)
- Color-coded transaction lines for easy tracking
- Interactive tooltips showing detailed information
- Filtering by time range, user, and more
- Detailed table of transaction operations

### Transaction Metrics

The metrics visualization provides quantitative analysis of transactions:

- Aggregated metrics by time, user, isolation level, or collection
- Bar charts showing transaction counts, commits, aborts, and more
- Average duration metrics
- Success rate analysis
- Operational pattern identification

### Transaction Relationships

The relationship visualization displays the connections between transactions and documents:

- Interactive network graph of transaction and document nodes
- Color-coded operation types (insert, update, delete)
- Transaction state indication
- Filtering by collection or document ID
- Zoom and pan capabilities for exploring complex graphs

## Usage Examples

### Monitoring Transaction Activity

1. Open the Transactions visualization tab in the admin interface
2. View the real-time transaction statistics at the top
3. Use the timeline visualization to see recent transaction activity
4. Identify patterns of commits and aborts

### Analyzing Performance Issues

1. Switch to the Metrics visualization
2. Select "By Time" dimension to see transaction patterns over time
3. Look for periods of high abort rates or long durations
4. Identify users or collections with performance issues

### Troubleshooting Deadlocks

1. Switch to the Relationship visualization
2. Filter to the time period where deadlocks occurred
3. Examine the transaction-document relationships
4. Identify documents involved in multiple transactions that might cause contention

## Performance Considerations

The visualization system processes transaction log data, which can grow large over time. To ensure optimal performance:

1. Use time-based filtering to limit the amount of data processed
2. Consider enabling log compaction to reduce log size while preserving important entries
3. Use the metrics view for long-term trend analysis rather than raw log data
4. For very large transaction volumes, consider implementing server-side aggregation

## Customization

The visualization system can be extended in several ways:

1. **Additional Dimensions**: Add new dimensions to the metrics visualization
2. **Custom Charts**: Implement specialized charts for specific use cases
3. **Export Capabilities**: Add functionality to export visualization data
4. **Alerting**: Integrate with alerting systems based on transaction patterns

## API Reference

### Transaction History API

```
GET /api/visualization/transaction-history
```

Parameters:
- `start_time`: Unix timestamp for start of time range (optional)
- `end_time`: Unix timestamp for end of time range (optional)
- `user_id`: Filter by user ID (optional)
- `limit`: Maximum number of transactions to return (default: 100)

### Transaction Metrics API

```
GET /api/visualization/transaction-metrics
```

Parameters:
- `dimension`: Dimension to aggregate by (time, user, isolation, collection)
- `start_time`: Unix timestamp for start of time range (optional)
- `end_time`: Unix timestamp for end of time range (optional)

### Transaction Relationships API

```
GET /api/visualization/transaction-relationships
```

Parameters:
- `start_time`: Unix timestamp for start of time range (optional)
- `end_time`: Unix timestamp for end of time range (optional)
- `collection`: Filter by collection name (optional)
- `document_id`: Filter by document ID (optional)