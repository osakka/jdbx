# JSON Database Server

A lightweight, multithreaded JSON database server with REST API support and JavaScript integration.

## Current Project Status (v1.0.5-js-features)

The project has reached a stable state with the following components functioning:

### What's Working
- Core database functionality (collections, documents, CRUD operations)
- RESTful API with proper error handling
- Web-based admin interface
- CORS support with proper handling of preflight requests
- Authentication via JWT tokens with token refresh
- Document caching system with invalidation
- Transaction support for atomic operations
- JavaScript integration via QuickJS
  - Document validators for data integrity
  - Document transformers for data processing
  - Custom JavaScript functions
  - JavaScript query capabilities
- Build system using the Makefile

### Recently Fixed
- CORS implementation for cross-origin requests
- Web interface collection creation and document management
- Unused function warnings and integration of previously uncalled functions
- Repository structure cleanup and organization
- SameSite cookie attributes for improved security

### Recently Added
- Token refresh mechanism for improved authentication
- Comprehensive JavaScript functions support
- Document validators and transformers
- JavaScript query capabilities

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

For a complete list of API endpoints and documentation, see the [API.md](docs/api/API.md) file.

## Security

### Authentication

The server uses JWT (JSON Web Token) authentication. To access protected endpoints, you need to:

1. Authenticate using the `/api/auth/login` endpoint
2. Include the received token in subsequent requests via the `Authorization` header:
   ```
   Authorization: Bearer YOUR_TOKEN_HERE
   ```

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