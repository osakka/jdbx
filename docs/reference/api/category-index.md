# JDBX API Documentation

Complete API reference and guides for JDBX server.

## Quick Links

### Core API
- **[REST API Reference](api.md)** - Complete REST API documentation
- **[OpenAPI Specification](http://localhost:5000/api/openapi.json)** - Machine-readable API spec

### Authentication & Authorization  
- **[Authentication Guide](../guides/authentication-guide.md)** - How to authenticate with the API
- **[RBAC Setup Guide](../guides/rbac-setup.md)** - Role-based access control setup
- **[RBAC Technical Reference](../reference/rbac.md)** - Technical RBAC implementation details

### Core Functionality
- **[Query Language Reference](../reference/query-language.md)** - JDBX query syntax
- **[Database Schema](../reference/database-schema.md)** - Database structure and system collections
- **[Performance Optimization](../reference/performance-optimization.md)** - API performance best practices

## API Endpoints Overview

 < /dev/null |  Category | Endpoints | Description |
|----------|-----------|-------------|
| **Authentication** | `/api/auth/*` | User registration, login, logout, token refresh |
| **Collections** | `/api/collections/*` | Collection management operations |
| **Documents** | `/api/collections/{name}/documents/*` | Document CRUD operations |
| **Users** | `/api/users/*`, `/api/rbac/users/*` | User management |
| **Roles** | `/api/roles/*`, `/api/rbac/roles/*` | Role management |
| **Sessions** | `/api/sessions/*` | Session management |
| **Metrics** | `/api/metrics/*` | System metrics and monitoring |
| **Indexes** | `/api/indexes/*` | Index management and querying |
| **Transactions** | `/api/transactions/*` | Transaction support |
| **JavaScript** | `/api/js/*` | JavaScript extension API |
| **Cache** | `/api/cache/*` | Cache management |
| **System** | `/api/system/*`, `/health` | System information and health |

## Getting Started

### 1. Server Setup
```bash
# Start the JDBX server
cd /opt/jdbx && build/jdbx_runtime.sh start

# Verify server is running
curl http://localhost:5000/health
```

### 2. Authentication
```bash
# Login with default admin credentials
curl -X POST http://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "admin"}'

# Use the returned token in subsequent requests
curl -X GET http://localhost:5000/api/collections \
  -H "Authorization: Bearer <your-token>"
```

### 3. Basic Operations
```bash
# Create a collection
curl -X POST http://localhost:5000/api/collections \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"name": "products"}'

# Insert a document
curl -X POST http://localhost:5000/api/collections/products/documents \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"name": "Widget", "price": 19.99}'

# Query documents
curl -X GET http://localhost:5000/api/collections/products/documents \
  -H "Authorization: Bearer <token>"
```

## Response Formats

All API responses use JSON format with consistent error handling:

**Success Response:**
```json
{
  "data": "response content",
  "...": "additional fields"
}
```

**Error Response:**
```json
{
  "error": "Error message description"
}
```

**HTTP Status Codes:**
- `200 OK` - Request successful
- `201 Created` - Resource created successfully
- `204 No Content` - Success with no response body
- `400 Bad Request` - Invalid request
- `401 Unauthorized` - Authentication required
- `403 Forbidden` - Insufficient permissions
- `404 Not Found` - Resource not found
- `500 Internal Server Error` - Server error

## Authentication Methods

JDBX supports multiple authentication methods:

1. **Session Tokens** - Short-lived tokens (30 minutes) for interactive use
2. **API Tokens** - Long-lived tokens (1 year default) for programmatic access
3. **Admin Authentication** - Special admin login endpoint

All authenticated requests require the `Authorization: Bearer <token>` header.

## Rate Limiting

API requests are rate limited to prevent abuse:
- **General API**: 1000 requests per hour per user
- **Login Attempts**: 5 failed attempts per 15 minutes
- **Token Generation**: 10 API tokens per day per user

## Base URL and Versioning

- **Base URL**: `http://localhost:5000` (default)
- **API Version**: 2.0.8
- **API Prefix**: All API endpoints start with `/api/`
- **Health Check**: `/health` (no authentication required)

## SDKs and Tools

### Command Line Tools
```bash
# Health check
curl http://localhost:5000/health

# Get API specification
curl http://localhost:5000/api/openapi.json

# Interactive API exploration
# Visit http://localhost:5000 for web admin interface
```

### HTTP Clients
The API is compatible with all standard HTTP clients:
- curl
- Postman
- Insomnia
- wget
- HTTPie

## Support and Troubleshooting

### Common Issues
1. **401 Unauthorized** - Check token validity and expiration
2. **403 Forbidden** - Verify user permissions for the operation
3. **Connection Refused** - Ensure server is running on correct port

### Log Analysis
```bash
# Check server logs
cat /opt/jdbx/var/jdbxd.log | tail -50

# Filter for API errors
grep "ERROR\|WARN" /opt/jdbx/var/jdbxd.log
```

### Debug Mode
```bash
# Start server with debug logging
LOG_LEVEL=DEBUG build/jdbx_runtime.sh start
```

## Related Documentation

- [Installation Guide](../guides/installation.md)
- [Configuration Reference](../reference/configuration.md)
- [Security Guidelines](../security/security-guidelines.md)
- [Performance Benchmarks](../reference/performance-benchmarks.md)
- [Admin Interface Guide](../guides/admin-ui-guide.md)

---

**Last Updated**: May 31, 2025  
**API Version**: 2.0.8  
**Documentation Version**: Latest
