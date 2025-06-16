# JDBX API Documentation

**Version**: 6.3.0  
**Last Updated**: June 16, 2025

> Complete reference for all JDBX APIs and interfaces with unified documents architecture

## API Categories

### REST API
The primary interface for interacting with JDBX:
- **[REST API Reference](rest-api.md)** - Complete HTTP endpoint documentation
- **[Authentication](rest-api.md#authentication)** - JWT-based authentication
- **[Error Handling](rest-api.md#error-handling)** - Error codes and responses

### JavaScript API
Extend JDBX with custom logic:
- **[JavaScript API](javascript-api.md)** - Script execution endpoints
- **[Validators](javascript-api.md#validators)** - Document validation scripts
- **[Transformers](javascript-api.md#transformers)** - Data transformation scripts
- **[Functions](javascript-api.md#functions)** - Custom business logic

### Client Libraries
Official SDKs for various languages:
- **[Client Libraries](client-libraries.md)** - Language-specific implementations
- **[Node.js Client](client-libraries.md#nodejs)** - JavaScript/TypeScript SDK
- **[Python Client](client-libraries.md#python)** - Python SDK
- **[Go Client](client-libraries.md#go)** - Go SDK

### Internal APIs
Lower-level APIs for advanced use cases:
- **[C API](c-api.md)** - Direct database API (advanced)
- **[Plugin API](plugin-api.md)** - Extension development API

## Quick Reference

### Common Endpoints

| Operation | Method | Endpoint |
|-----------|--------|----------|
| Login | POST | `/api/auth/login` |
| List Collections | GET | `/api/collections` |
| Create Document | POST | `/api/collections/{name}/documents` |
| Query Documents | GET | `/api/collections/{name}/documents?query={}` |
| Execute Function | POST | `/api/js/functions/{name}` |

### Authentication Header
```
Authorization: Bearer <jwt-token>
```

### Content Type
```
Content-Type: application/json
```

## API Design Principles

1. **RESTful Design**: Resources are nouns, actions are HTTP verbs
2. **JSON First**: All requests and responses use JSON
3. **Consistent Naming**: Plural resources, kebab-case paths
4. **Meaningful Status Codes**: Proper HTTP status codes for all responses
5. **Comprehensive Errors**: Detailed error messages with codes

## API Versioning

JDBX uses URL-based versioning:
- Current version: `/api/` (v1)
- Future versions: `/api/v2/`, `/api/v3/`, etc.
- Backward compatibility maintained within major versions

## Rate Limiting

Default rate limits:
- **Anonymous**: 100 requests/minute
- **Authenticated**: 1000 requests/minute
- **Admin**: No limit

Custom limits can be configured per user/role.

## Getting Started

1. **Authentication First**: Get a JWT token via `/api/auth/login`
2. **Explore Collections**: List available collections
3. **CRUD Operations**: Create, read, update, delete documents
4. **Advanced Queries**: Use the query language for complex searches
5. **Extend with JS**: Add custom validation and logic

## See Also

- [Authentication Guide](../guides/authentication-guide.md)
- [Query Language Reference](../reference/query-language.md)
- [JavaScript Development Guide](../guides/javascript-development-guide.md)
- [API Examples](../examples/api-examples.md)