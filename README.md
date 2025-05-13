# JSON Database Server

A lightweight, multithreaded JSON database server with REST API support built with TCC (Tiny C Compiler).

## Features

- JSON document database with collections
- RESTful API for data management
- Configuration via REST API
- Role-based access control (RBAC)
- JWT authentication
- SSL/TLS support
- Multithreaded architecture
- Minimal dependencies (primarily standard libc libraries)
- Schema validation for documents
- Web-based admin interface
- Data visualization tools
- Performance metrics tracking
- System monitoring
- Cross-origin resource sharing (CORS) support
- Document caching with multiple eviction policies (LRU, LFU, FIFO)
- JavaScript extensions via QuickJS
- ACID transactions for atomic multi-document operations

## Table of Contents

- [Overview](#json-database-server)
- [Requirements](#requirements)
- [Installation](#installation)
- [Building from Source](#building-from-source)
- [Running the Server](#running-the-server)
- [API Overview](#api-overview)
- [Client Examples](#client-examples)
- [Security](#security)
- [Configuration](#configuration)
- [Architecture](#architecture)
- [Contributing](#contributing)
- [License](#license)

## Requirements

- TCC (Tiny C Compiler)
- POSIX-compliant operating system (Linux, macOS, etc.)
- Development packages for:
  - UUID generation (`libuuid-dev`)
  - SSL support (`libssl-dev`)
  - Cryptography functions (`libcrypto-dev`)

### Installing Dependencies

#### On Debian/Ubuntu

```bash
sudo apt-get update
sudo apt-get install tcc libuuid-dev libssl-dev
```

#### On CentOS/RHEL

```bash
sudo yum install tcc uuid-devel openssl-devel
```

#### On macOS

```bash
brew install tcc ossp-uuid openssl
```

## Installation

### Using Pre-built Binaries

Download the latest release from the releases page and extract it:

```bash
tar -xzf jsondb-1.0.0.tar.gz
cd jsondb-1.0.0
```

### Building from Source

Clone the repository and build from source:

```bash
git clone https://github.com/yourusername/jsondb.git
cd jsondb
cd src
make
```

This will create the executable in the `build/bin` directory.

## Running the Server

### Start the Server

```bash
cd src
make run
```

Or directly:

```bash
./build/bin/jsondb_server
```

To run as a daemon in the background:

```bash
./bin/jsondb_server -daemon
```

To check server status:

```bash
./bin/jsondb_server -status
```

To stop a running server:

```bash
./bin/jsondb_server -stop
```

The server runs on port 5000 by default. You can access the API at `http://localhost:5000` and the admin interface at `http://localhost:5000/admin`.

### Accessing the Admin Interface

1. Open a web browser and navigate to `http://localhost:5000/admin`
2. Log in with the default credentials
3. Use the web interface to manage collections, documents, schemas, and monitor system performance

### Default Credentials

The server creates a default admin user on first start:

- Username: `admin`
- Password: `admin`

**Important:** Change the default password immediately in a production environment!

## API Overview

The server provides a comprehensive REST API for managing the database. Here's a brief overview:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/auth/login` | POST | Login with credentials |
| `/api/auth/register` | POST | Register a new user |
| `/api/collections` | GET | List all collections |
| `/api/collections` | POST | Create a collection |
| `/api/collections/:name` | DELETE | Drop a collection |
| `/api/collections/:name/documents` | GET | Query documents |
| `/api/collections/:name/documents` | POST | Create a document |
| `/api/collections/:name/documents/:id` | GET | Get a document |
| `/api/collections/:name/documents/:id` | PUT | Update a document |
| `/api/collections/:name/documents/:id` | DELETE | Delete a document |
| `/api/schemas` | GET | List all schemas |
| `/api/schemas` | POST | Create a schema |
| `/api/schemas/:collection` | GET | Get schema for collection |
| `/api/schemas/:collection` | PUT | Update schema for collection |
| `/api/schemas/:collection` | DELETE | Delete schema for collection |
| `/api/validate` | POST | Validate document against schema |
| `/api/metrics` | GET | Get server metrics |
| `/api/system/info` | GET | Get system information |
| `/api/visualization/collection-stats` | GET | Get collection statistics |
| `/api/visualization/document-types` | GET | Get document type analysis |
| `/api/visualization/field-distribution` | GET | Get field value distribution |
| `/api/js/query` | POST | Execute JavaScript query on collection |
| `/api/js/eval` | POST | Evaluate JavaScript code |
| `/api/js/functions` | POST | Register JavaScript function |
| `/api/cache/stats` | GET | Get cache statistics |
| `/api/cache/configure` | POST | Configure cache settings |
| `/api/cache/clear` | POST | Clear document cache |
| `/api/transactions` | POST | Begin a new transaction |
| `/api/transactions/:id/commit` | POST | Commit a transaction |
| `/api/transactions/:id/rollback` | DELETE | Rollback a transaction |

For a complete list of API endpoints and documentation, see the [API.md](docs/api/API.md) file.

## Client Examples

The repository includes example clients to demonstrate how to interact with the API:

### Python Client

```bash
# Run the Python client example
cd share/examples
./python_client.py
```

This demonstrates the full API workflow from authentication to document operations.

### cURL Examples

```bash
# Run the cURL examples
cd share/examples
./curl_examples.sh
```

This shell script shows how to interact with the API using cURL commands.

## Security

### Authentication

The server uses JWT (JSON Web Token) authentication. To access protected endpoints, you need to:

1. Authenticate using the `/api/auth/login` endpoint
2. Include the received token in subsequent requests via the `Authorization` header:
   ```
   Authorization: Bearer YOUR_TOKEN_HERE
   ```

### RBAC (Role-Based Access Control)

The server implements a role-based access control system:

- **Users** have one or more roles
- **Roles** have specific permissions for resources
- **Permissions** define what actions can be performed

### SSL/TLS Support

For production use, enable SSL/TLS by:

1. Generating certificates (or using your existing ones)
2. Updating the configuration to enable SSL and point to your certificate files
3. Restarting the server

## Configuration

The server configuration is stored in the database and can be modified via the API or command-line arguments. All file paths are now relative to the server binary location for better portability.

### Default Configuration

- Port: 5000
- SSL: Disabled
- Database Path: `data/jsondb/db.json` (relative to binary location)
- RBAC Path: `data/jsondb/rbac.json` (relative to binary location)
- PID File: `data/run/jsondb_server.pid` (relative to binary location)
- Log File: `data/log/jsondb/server.log` (relative to binary location)
- JWT Secret: `change-this-secret-in-production`

### Command-line Options

```bash
Usage: jsondb_server [options]

Options:
  -port <number>     Set the server port (default: 5000)
  -host <address>    Set the host name or address to advertise
  -js <file>         Execute a JavaScript file and exit
  -start             Start the server (default if no action specified)
  -stop              Stop the running server instance
  -status            Check the current status of the server
  -daemon, -d        Run server as a daemon in the background
  -pid <file>        Use custom PID file location (relative to binary)
  -log <file>        Use custom log file location (relative to binary)
  -log-level <level> Set logging level (none, error, warning, info, debug, trace)
  -help, --help, -h  Display this help message
  -version, -v       Display version information
```

You can also update some configuration via the `/api/config` endpoint (admin access required).

## Architecture

The server is built around these core components:

- **HTTP Server**: Multithreaded, with connection pooling
- **JSON Parser**: Fast, memory-efficient JSON parsing
- **Database Engine**: Collection and document management
- **RBAC System**: User, role, and permission management
- **API Layer**: RESTful interface to the database
- **JavaScript Integration**: Extend database functionality with JavaScript

### Project Structure

The project follows a clean, maintainable structure organized into logical components:

- **src/**: Source code organized by component
- **include/**: Header files with clean namespace hierarchy
- **docs/**: Comprehensive documentation
- **examples/**: Usage examples from basic to advanced
- **tests/**: Test suite organized by test type

For a detailed overview of the project structure, see [Project Structure](docs/architecture/project_structure.md).

## Contributing

Contributions are welcome! We have established a detailed workflow to support multiple developers collaborating on this project.

**Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting your first pull request.**

This document contains:
- Repository details and access information
- Branch management and naming conventions
- Pull request workflow and requirements
- Strategies for preventing and resolving merge conflicts
- Git best practices and troubleshooting tips

Following our contribution guidelines ensures a smooth development process and helps maintain code quality.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.