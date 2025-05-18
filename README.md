# JSON Database Server

A lightweight, multithreaded JSON database server with REST API support and JavaScript integration.

## Current Project Status (v1.0.6-database-rbac)

The project has reached a stable state with the following components functioning:

### What's Working
- Core database functionality (collections, documents, CRUD operations)
- RESTful API with proper error handling
- Web-based admin interface
- CORS support with proper handling of preflight requests
- Authentication via JWT tokens with token refresh
- Database-based Role-Based Access Control (RBAC)
  - RBAC API for user, role, and permission management
  - Seamless migration from file-based to database-based RBAC
  - Fine-grained permission system
- Document caching system with invalidation
- Transaction support for atomic operations
  - Transaction isolation levels
  - Transaction visualization for monitoring
  - Transaction logs and metrics
- JavaScript integration via QuickJS
  - Document validators for data integrity
  - Document transformers for data processing
  - Custom JavaScript functions
  - JavaScript query capabilities
- Build system using the Makefile

### Recently Fixed
- Database locking improvements with read-write locks for reduced contention
- Socket binding thread synchronization issues and race conditions
- Server initialization sequence for proper component dependency handling
- CORS implementation for cross-origin requests
- Web interface collection creation and document management
- Unused function warnings and integration of previously uncalled functions
- Repository structure cleanup and organization
- SameSite cookie attributes for improved security

### Recently Added
- Database-based RBAC system with database collections
- RBAC API for user, role, and permission management
- Automatic migration from file-based to database-based RBAC
- Token refresh mechanism for improved authentication
- Comprehensive JavaScript functions support
- Document validators and transformers
- JavaScript query capabilities
- Transaction visualization and monitoring
- Enhanced transaction logging and metrics

### In Progress
- Performance optimization for large document sets
- Enhanced error reporting and diagnostics

### Next Development Phase
- Metrics and monitoring improvements
- Extended test coverage
- Schema validation enhancements
- WebSocket support for real-time updates

## Requirements

- GCC compiler
- POSIX-compliant operating system (Linux, macOS, etc.)
- Development packages for:
  - UUID generation (`libuuid-dev`)
  - SSL support (`libssl-dev`)
  - Cryptography functions (`libcrypto-dev`)
  - QuickJS for JavaScript support

### Installing Dependencies

#### On Debian/Ubuntu

```bash
sudo apt-get update
sudo apt-get install gcc libuuid-dev libssl-dev
```

#### On CentOS/RHEL

```bash
sudo yum install gcc uuid-devel openssl-devel
```

## Building from Source

Clone the repository and build from source:

```bash
git clone https://github.com/yourusername/jsondb.git
cd jsondb
cd src
make
```

This will create the executable in the `build/bin` directory.

## Running the Server

### Using the Runtime Script (Recommended)

The easiest way to manage the server is using the `jsondb_runtime.sh` script:

```bash
# Start the server
./build/jsondb_runtime.sh start

# Check server status
./build/jsondb_runtime.sh status

# Stop the server
./build/jsondb_runtime.sh stop

# Restart the server
./build/jsondb_runtime.sh restart
```

The server runs on port 5000 by default. You can access the API at `http://localhost:5000` and the admin interface at `http://localhost:5000/admin`.

### Accessing the Admin Interface

1. Open a web browser and navigate to `http://localhost:5000/admin`
2. Log in with the default credentials
3. Use the web interface to manage collections, documents, and monitor system performance

### Default Credentials

The server creates a default admin user on first start:

- Username: `admin`
- Password: `admin`

**Important:** Change the default password immediately in a production environment!

## API Overview

The server provides a comprehensive REST API for managing the database. Here's a brief overview of key endpoints:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/auth/login` | POST | Login with credentials |
| `/api/collections` | GET | List all collections |
| `/api/collections` | POST | Create a collection |
| `/api/collections/:name` | DELETE | Drop a collection |
| `/api/collections/:name/documents` | GET | Query documents |
| `/api/collections/:name/documents` | POST | Create a document |
| `/api/collections/:name/documents/:id` | GET | Get a document |
| `/api/collections/:name/documents/:id` | PUT | Update a document |
| `/api/collections/:name/documents/:id` | DELETE | Delete a document |
| `/api/rbac/users` | GET | List all users |
| `/api/rbac/users/:id` | GET | Get user by ID |
| `/api/rbac/users` | POST | Create a new user |
| `/api/rbac/users/:id` | PUT | Update a user |
| `/api/rbac/users/:id` | DELETE | Delete a user |
| `/api/rbac/roles` | GET | List all roles |
| `/api/rbac/roles/:id` | GET | Get role by ID |
| `/api/rbac/roles` | POST | Create a new role |
| `/api/rbac/roles/:id` | DELETE | Delete a role |
| `/api/rbac/roles/:id/users/:user_id` | POST | Add user to role |
| `/api/rbac/roles/:id/users/:user_id` | DELETE | Remove user from role |
| `/api/rbac/roles/:id/permissions` | POST | Grant permission |
| `/api/rbac/roles/:id/permissions` | DELETE | Revoke permission |
| `/api/cache/stats` | GET | Get cache statistics |
| `/api/cache/configure` | POST | Configure cache settings |
| `/api/cache/clear` | POST | Clear document cache |
| `/api/cache/invalidate` | POST | Process cache invalidations |
| `/api/js/eval` | POST | Evaluate JavaScript code |
| `/api/js/functions/register` | POST | Register a JavaScript function |
| `/api/js/functions/:name` | POST | Execute a JavaScript function |
| `/api/js/validators/register` | POST | Register a document validator |
| `/api/js/transformers/register` | POST | Register a document transformer |
| `/api/js/query` | POST | Execute a JavaScript query |
| `/api/auth/refresh` | POST | Refresh authentication token |
| `/api/transactions` | POST | Begin a new transaction |
| `/api/transactions/:id/commit` | POST | Commit a transaction |
| `/api/transactions/:id/rollback` | DELETE | Rollback a transaction |
| `/api/visualization/transaction-history` | GET | View transaction history |
| `/api/visualization/transaction-metrics` | GET | View transaction metrics |
| `/api/visualization/transaction-relationships` | GET | View transaction relationships |

For a complete list of API endpoints and documentation, see the [API.md](docs/api/API.md) file and the [RBAC API Reference](docs/api/RBAC_API.md) for details on the new database-based RBAC system.

## Security

### Authentication

The server uses JWT (JSON Web Token) authentication. To access protected endpoints, you need to:

1. Authenticate using the `/api/auth/login` endpoint
2. Include the received token in subsequent requests via the `Authorization` header:
   ```
   Authorization: Bearer YOUR_TOKEN_HERE
   ```

#### Token Refresh

The server supports token refresh for maintaining sessions without requiring the user to log in again:

1. When logging in, you'll receive both an access token and a refresh token
2. When the access token expires, you can use the `/api/auth/refresh` endpoint with the refresh token to obtain a new access token
3. This approach improves security by limiting the lifetime of access tokens while maintaining session continuity

### Role-Based Access Control (RBAC)

The server uses a database-based RBAC system for fine-grained permission management:

1. All user accounts, roles, and permissions are stored directly in database collections
2. Permissions can be assigned at the database, collection, document, user, or role level
3. The system supports four permission types: READ, WRITE, DELETE, and ADMIN
4. Administrators can manage users, roles, and permissions through the RBAC API
5. The system automatically migrates existing file-based RBAC data to the database on first startup

For detailed information on the RBAC system, see the [Database-Based RBAC Implementation](docs/reference/DATABASE_RBAC.md) document.

### CORS Support

The server includes proper CORS support for cross-origin requests:
- Configurable origins with dynamic validation
- Handling of preflight OPTIONS requests
- Support for credentials with SameSite cookie attributes

## Configuration

The server configuration is stored in configuration files and can be modified via the API or command-line arguments.

### Default Configuration

- Port: 5000
- SSL: Disabled
- Database Path: Handled by the server during runtime
- Log File: Located in build/var/logs

### Command-line Options

```bash
Usage: jsondb_server [options]

Options:
  --port <number>       Set the server port (default: 5000)
  --daemon              Run server as a daemon in the background
  --js_eval <code>      Execute JavaScript code
  --js_eval_file <path> Execute JavaScript from file
  --help, -h            Display this help message
  --version, -v         Display version information
```

## Project Structure

The project follows a clean, maintainable structure organized into logical components:

- **src/**: Source code organized by components
  - **components/**: Core functionality separated by domain
  - **include/**: Header files with clean namespace hierarchy
- **build/**: Build artifacts and runtime environment
- **share/**: Example code and web interface
  - **examples/**: Example code snippets and demos
  - **js-examples/**: JavaScript function examples
- **functions/**: JavaScript user functions
- **validators/**: JavaScript document validators
- **transforms/**: JavaScript document transformers
- **etc/**: Configuration files
- **docs/**: Documentation organized by topic

## Development Process

For development, follow these guidelines:

1. Build using the Makefile in the src directory
2. Use the provided scripts for testing
3. Follow Git commit message conventions (type(scope): subject)
4. Tag significant milestones for easy reference

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.