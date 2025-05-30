# JSONdb API Documentation

## Main API Reference

- **[REST API Documentation](api-rest.md)** - Complete REST API reference for JSONdb server

## Specialized API Documentation

- **[JavaScript API](JAVASCRIPT_API.md)** - JavaScript integration and extension API
- **[RBAC API](RBAC_API.md)** - Database-based Role-Based Access Control API

## Deprecated Documentation

The following files have been replaced by the consolidated REST API documentation:

- ~~API.md~~ - Deprecated, use [api-rest.md](api-rest.md)
- ~~API_CORRECTED.md~~ - Deprecated, use [api-rest.md](api-rest.md)

## Quick Start

The JSONdb server runs on port 5000 by default. To get started:

1. Start the server: `build/jsondb_runtime.sh start`
2. Access the API at: `http://localhost:5000`
3. View the admin interface at: `http://localhost:5000/admin`

For authentication, use the `/api/auth/login` endpoint to get a JWT token, then include it in the `Authorization` header:

```
Authorization: Bearer <your-jwt-token>
```

See the [REST API Documentation](api-rest.md) for complete endpoint details and examples.