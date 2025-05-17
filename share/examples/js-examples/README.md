# JavaScript Examples for JSONdb

This directory contains example JavaScript files for working with JSONdb's JavaScript functionality.

## Contents

- **validator_example.js**: Example validator for the products collection
- **transformer_example.js**: Example transformer for the products collection  
- **function_example.js**: Example custom function for inventory analysis
- **query_example.js**: Example JavaScript query for complex product filtering

## How to Use These Examples

### 1. Validators

Validators ensure documents meet specific requirements before they're saved to the database.

To use the validator example:

1. Copy the file to your validators directory:
   ```
   cp validator_example.js /path/to/your/validators/products.js
   ```

2. Or register via the HTTP API:
   ```
   curl -X POST http://localhost:5000/api/js/validators/register \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer YOUR_TOKEN" \
     -d @validator_example.js
   ```

### 2. Transformers

Transformers modify documents during operations like insert, update, or query.

To use the transformer example:

1. Copy the file to your transforms directory:
   ```
   cp transformer_example.js /path/to/your/transforms/products.js
   ```

2. Or register via the HTTP API:
   ```
   curl -X POST http://localhost:5000/api/js/transformers/register \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer YOUR_TOKEN" \
     -d @transformer_example.js
   ```

### 3. Custom Functions

Custom functions provide reusable data processing logic.

To use the function example:

1. Copy the file to your functions directory:
   ```
   cp function_example.js /path/to/your/functions/inventoryAnalysis.js
   ```

2. Or register via the HTTP API:
   ```
   curl -X POST http://localhost:5000/api/js/functions/register \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer YOUR_TOKEN" \
     -d @function_example.js
   ```

3. Call the function:
   ```
   curl -X POST http://localhost:5000/api/js/functions/inventoryAnalysis \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer YOUR_TOKEN" \
     -d '{"collection": "products", "minStock": 5}'
   ```

### 4. JavaScript Queries

JavaScript queries allow for complex data filtering and manipulation.

To use the query example:

```
curl -X POST http://localhost:5000/api/js/query \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d @query_example.js
```

## Learn More

For more information about JavaScript functionality in JSONdb, see:

- [JavaScript Functions Guide](/docs/guides/javascript_functions.md)
- [JavaScript API Reference](/docs/api/JAVASCRIPT_API.md)