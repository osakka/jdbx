# JavaScript Extension Examples

This directory contains example JavaScript extensions for the JSON Database Server. These examples demonstrate how to use JavaScript to enhance database functionality.

## Contents

- `user_validator.js` - Document validation rules for the "users" collection
- `user_transformer.js` - Document transformation logic for the "users" collection
- `analytics_function.js` - Custom function that provides analytics capabilities

## How to Use

### Loading a Validator

To use a validator, send a POST request to `/api/js/validators`:

```bash
curl -X POST http://localhost:5000/api/js/validators \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -d '{
    "collection": "users",
    "code": "'"$(cat user_validator.js)"'"
  }'
```

### Loading a Transformer

To use a transformer, send a POST request to `/api/js/transformers`:

```bash
curl -X POST http://localhost:5000/api/js/transformers \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -d '{
    "collection": "users",
    "code": "'"$(cat user_transformer.js)"'"
  }'
```

### Registering a Custom Function

To register a custom function, send a POST request to `/api/js/functions`:

```bash
curl -X POST http://localhost:5000/api/js/functions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -d '{
    "name": "analytics",
    "code": "'"$(cat analytics_function.js)"'"
  }'
```

### Calling the Analytics Function

Once registered, you can call the analytics function with different parameters:

#### Count documents:

```bash
curl -X POST http://localhost:5000/api/js/functions/analytics \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -d '{
    "collection": "users",
    "analysis": "count"
  }'
```

#### Group by role and count:

```bash
curl -X POST http://localhost:5000/api/js/functions/analytics \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -d '{
    "collection": "users",
    "analysis": "group",
    "groupBy": "role",
    "metric": "count"
  }'
```

#### Calculate average age by status:

```bash
curl -X POST http://localhost:5000/api/js/functions/analytics \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -d '{
    "collection": "users",
    "analysis": "group",
    "groupBy": "status",
    "metric": "avg",
    "field": "age"
  }'
```

#### Time series analysis of signups:

```bash
curl -X POST http://localhost:5000/api/js/functions/analytics \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -d '{
    "collection": "users",
    "analysis": "timeseries",
    "timeField": "created_at",
    "interval": "month",
    "metric": "count"
  }'
```

#### Distribution analysis of ages:

```bash
curl -X POST http://localhost:5000/api/js/functions/analytics \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -d '{
    "collection": "users",
    "analysis": "distribution",
    "field": "age",
    "buckets": 5
  }'
```

## JavaScript Query Examples

You can also use JavaScript for complex queries:

```bash
curl -X POST http://localhost:5000/api/js/query \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -d '{
    "collection": "users",
    "query": "doc.age > 30 && doc.status === \"active\" && doc.role === \"admin\""
  }'
```

Query for users who haven't logged in recently:

```bash
curl -X POST http://localhost:5000/api/js/query \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -d '{
    "collection": "users",
    "query": "doc.status === \"active\" && (!doc.last_login || new Date(doc.last_login) < new Date(Date.now() - 30*24*60*60*1000))"
  }'
```