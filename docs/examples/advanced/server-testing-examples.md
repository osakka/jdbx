# JSON Database Server Testing

This document provides information on how to use the `tests/server/test_server.sh` script to build, run, and test the JSON Database Server with a focus on transaction visualization capabilities.

> **IMPORTANT NOTE:** The testing scripts described in this document are currently reference implementations. Due to build issues in the current project state, these scripts may not be fully functional. The information provided here serves as documentation for when the build issues are resolved.

## Server Management

For everyday server management, use the `jdbx_runtime.sh` script:

```bash
# Start the server
./build/jdbx_runtime.sh start

# Check server status
./build/jdbx_runtime.sh status

# Stop the server
./build/jdbx_runtime.sh stop

# Restart the server
./build/jdbx_runtime.sh restart
```

See the [Server Usage Guide](/docs/reference/SERVER_USAGE.md) for complete details.

## Quick Start for Testing

To build and run a full test of the server:

```bash
./tests/server/test_server.sh
```

## Command Line Options

The script supports several command line options:

```bash
./tests/server/test_server.sh [options]
```

### Available Options

- `--skip-build`: Skip the build step and use the existing server binary
- `--basic-only`: Test only basic API functionality
- `--transaction-only`: Test only transaction API functionality
- `--visualization-only`: Test only visualization API functionality
- `--stress-test <n> <ops>`: Run stress test with `n` transactions and `ops` operations per transaction
- `--help`: Show the help message

### Examples

Test only visualization features:
```bash
./tests/server/test_server.sh --visualization-only
```

Run a stress test with 10 transactions, 5 operations each:
```bash
./tests/server/test_server.sh --stress-test 10 5
```

Skip the build process and run only transaction tests:
```bash
./tests/server/test_server.sh --skip-build --transaction-only
```

## Transaction Visualization Features

The script tests the following transaction visualization capabilities:

### Visualization Formats

1. **Timeline Visualization**: Shows transaction state changes over time
   ```bash
   curl "http://localhost:8080/api/visualization/transaction-history?format=timeline"
   ```

2. **Lifecycle Visualization**: Displays the complete lifecycle of individual transactions
   ```bash
   curl "http://localhost:8080/api/visualization/transaction-history?format=lifecycle"
   ```

3. **Heatmap Visualization**: Shows resource contention patterns
   ```bash
   curl "http://localhost:8080/api/visualization/transaction-history?format=heatmap"
   ```

4. **Distribution Visualization**: Displays statistical distribution of transaction performance
   ```bash
   curl "http://localhost:8080/api/visualization/transaction-history?format=distribution"
   ```

5. **Dependency Graph Visualization**: Shows transaction dependencies
   ```bash
   curl "http://localhost:8080/api/visualization/transaction-history?format=dependency"
   ```

6. **Sankey Diagram Visualization**: Shows transaction flow data
   ```bash
   curl "http://localhost:8080/api/visualization/transaction-history?format=sankey"
   ```

### Graph Export Formats

The script also tests exporting transaction graphs in various formats:

1. **DOT/GraphViz Format**: For use with GraphViz tools
   ```bash
   curl "http://localhost:8080/api/visualization/transaction-export?format=dot" > transactions.dot
   ```

2. **GraphML Format**: XML-based graph format
   ```bash
   curl "http://localhost:8080/api/visualization/transaction-export?format=graphml" > transactions.graphml
   ```

3. **Cytoscape.js JSON Format**: For use with Cytoscape.js
   ```bash
   curl "http://localhost:8080/api/visualization/transaction-export?format=cytoscape" > transactions-cytoscape.json
   ```

4. **D3.js JSON Format**: For use with D3.js visualizations
   ```bash
   curl "http://localhost:8080/api/visualization/transaction-export?format=d3" > transactions-d3.json
   ```

## Interpreting Test Results

- ✓ Green check marks indicate successful tests
- ⚠ Yellow warnings indicate tests that completed but may need manual verification
- ✗ Red X marks indicate failed tests

For visualization formats, the script checks whether the proper format data structure is returned. However, visual verification of the data is still recommended for a complete validation.

## Additional Notes

- The server runs on port 8080 by default
- Test data is stored in `./tests/data`
- The script automatically detects whether the server binary is named `jdbxd` or `jdbx`
- The script ensures the server is properly shut down even if tests fail