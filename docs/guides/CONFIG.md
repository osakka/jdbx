# Configuration Guide

This document explains the configuration options for the JSON Database Server.

## Configuration File

The server can be configured using a JSON configuration file. By default, it looks for `config.json` in the current directory. You can specify a different configuration file using the `--config` command-line option:

```bash
./bin/jsondb --config /path/to/config.json
```

If no configuration file is found, the server uses default values.

## Configuration Options

Here's a detailed explanation of each configuration option:

### Server Settings

```json
"server": {
  "port": 8080,
  "host": "0.0.0.0",
  "max_connections": 100,
  "timeout": 30
}
```

- `port`: The port number the server listens on (default: 8080)
- `host`: The host address to bind to (default: "0.0.0.0" - all interfaces)
- `max_connections`: Maximum number of concurrent connections (default: 100)
- `timeout`: Connection timeout in seconds (default: 30)

### Database Settings

```json
"database": {
  "path": "db.json",
  "auto_save": true,
  "save_interval": 60
}
```

- `path`: Path to the database file (default: "db.json")
- `auto_save`: Whether to automatically save database changes (default: true)
- `save_interval`: Interval between auto-saves in seconds (default: 60)

### RBAC Settings

```json
"rbac": {
  "path": "rbac.json",
  "auto_save": true,
  "save_interval": 60
}
```

- `path`: Path to the RBAC (users/roles) file (default: "rbac.json")
- `auto_save`: Whether to automatically save RBAC changes (default: true)
- `save_interval`: Interval between auto-saves in seconds (default: 60)

### JWT Settings

```json
"jwt": {
  "secret": "REPLACE_THIS_WITH_A_SECURE_SECRET_KEY",
  "expiration": 86400,
  "algorithm": "HS256",
  "issuer": "jsondb"
}
```

- `secret`: The secret key used to sign JWT tokens (IMPORTANT: change this in production!)
- `expiration`: Token expiration time in seconds (default: 86400 - 1 day)
- `algorithm`: The algorithm used for signing tokens (default: "HS256")
- `issuer`: The issuer field in JWT tokens (default: "jsondb")

### SSL Settings

```json
"ssl": {
  "enabled": false,
  "cert_path": "/path/to/your/certificate.crt",
  "key_path": "/path/to/your/private.key",
  "ca_path": "/path/to/your/ca.crt"
}
```

- `enabled`: Whether to enable SSL/TLS (default: false)
- `cert_path`: Path to the SSL certificate file
- `key_path`: Path to the SSL private key file
- `ca_path`: Path to the certificate authority file (optional)

### Logging Settings

```json
"logging": {
  "level": "info",
  "file": "jsondb.log",
  "console": true,
  "max_size": 10485760,
  "max_files": 5
}
```

- `level`: Log level (options: "debug", "info", "warning", "error")
- `file`: Path to the log file (default: "jsondb.log")
- `console`: Whether to also log to console (default: true)
- `max_size`: Maximum size of log file in bytes before rotation (default: 10MB)
- `max_files`: Maximum number of rotated log files to keep (default: 5)

### CORS Settings

```json
"cors": {
  "enabled": true,
  "allowed_origins": ["*"],
  "allowed_methods": ["GET", "POST", "PUT", "DELETE", "OPTIONS"],
  "allowed_headers": ["Content-Type", "Authorization"],
  "expose_headers": ["Content-Length"],
  "allow_credentials": false,
  "max_age": 86400
}
```

- `enabled`: Whether to enable CORS support (default: true)
- `allowed_origins`: List of allowed origins, use ["*"] for any origin (default: ["*"])
- `allowed_methods`: List of allowed HTTP methods (default: all common methods)
- `allowed_headers`: List of headers that can be used during the request (default: common headers)
- `expose_headers`: List of headers exposed to the browser (default: ["Content-Length"])
- `allow_credentials`: Whether to allow cookies in CORS requests (default: false)
- `max_age`: How long results can be cached in seconds (default: 86400 - 1 day)

### Rate Limiting Settings

```json
"throttling": {
  "enabled": false,
  "rate": 100,
  "burst": 50,
  "ip_whitelist": ["127.0.0.1"]
}
```

- `enabled`: Whether to enable rate limiting (default: false)
- `rate`: Maximum number of requests per minute (default: 100)
- `burst`: Maximum burst size (default: 50)
- `ip_whitelist`: List of IPs exempted from rate limiting (default: localhost only)

### Admin Settings

```json
"admin": {
  "username": "admin",
  "password": "CHANGE_THIS_PASSWORD_IMMEDIATELY"
}
```

- `username`: Default admin username (default: "admin")
- `password`: Default admin password (IMPORTANT: change this in production!)

## Environment Variables

The server also supports configuration via environment variables. Environment variables override settings in the configuration file.

Example environment variables:

- `JSONDB_PORT` - Server port
- `JSONDB_HOST` - Server host
- `JSONDB_DB_PATH` - Database file path
- `JSONDB_JWT_SECRET` - JWT secret key
- `JSONDB_SSL_ENABLED` - Enable SSL (true/false)

## Creating a Configuration File

1. Copy the example configuration file:
   ```bash
   cp config.json.example config.json
   ```

2. Edit the configuration file to match your needs:
   ```bash
   nano config.json
   ```

3. Start the server with your configuration:
   ```bash
   ./bin/jsondb
   ```

## Security Considerations

When deploying in production:

- **ALWAYS** change the JWT secret key to a strong, unique value
- **ALWAYS** change the default admin password
- Enable SSL/TLS for secure connections
- Consider enabling rate limiting to prevent abuse
- Restrict allowed origins in CORS settings
- Use a proper firewall to control access to the server port