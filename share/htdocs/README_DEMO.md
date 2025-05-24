# JSONdb Demo Application - Product Catalog

This demo showcases JSONdb as a backend for a simple e-commerce product catalog application.

## Features

- **Product Listing**: Browse products stored in JSONdb
- **Search**: Search products by name, SKU, or description
- **Category Filtering**: Filter products by category
- **Shopping Cart**: Add products to cart (stored in localStorage)
- **Product Details**: View detailed product information
- **Responsive Design**: Works on desktop and mobile devices

## Setup

1. **Ensure JSONdb server is running**:
   ```bash
   cd /opt/jsondb
   build/jsondb_runtime.sh start
   ```

2. **Create demo data** (if not already created):
   ```bash
   # The demo.html page expects a collection named 'demo_products'
   # Some sample products have already been created
   ```

3. **Access the applications** (served by JSONdb's integrated web server):
   - Admin Dashboard: http://localhost:5000/
   - Demo Application: http://localhost:5000/demo.html
   - Metrics Dashboard: http://localhost:5000/metrics_demo.html
   - Default admin credentials: admin/admin

## Architecture

```
┌─────────────────┐     ┌────────────────────────────┐
│   Web Browser   │────▶│     JSONdb Server          │
│                 │     │     (Port 5000)            │
└─────────────────┘     │                            │
                        │ ┌────────────────────────┐ │
                        │ │  Integrated Web Server │ │
                        │ │  /share/htdocs/        │ │
                        │ └────────────────────────┘ │
                        │                            │
                        │ ┌────────────────────────┐ │
                        │ │    RESTful API         │ │
                        │ │    /api/*              │ │
                        │ └────────────────────────┘ │
                        │                            │
                        │ ┌────────────────────────┐ │
                        │ │  Database Collections  │ │
                        │ │  - demo_products       │ │
                        │ │  - users (RBAC)        │ │
                        │ │  - roles (RBAC)        │ │
                        │ └────────────────────────┘ │
                        └────────────────────────────┘
```

## API Endpoints Used

- `GET /api/collections/demo_products/documents` - List all products
- `POST /api/collections/demo_products/documents` - Create new product
- `GET /api/collections/demo_products/documents/:id` - Get product details
- `PUT /api/collections/demo_products/documents/:id` - Update product
- `DELETE /api/collections/demo_products/documents/:id` - Delete product

## Extending the Demo

1. **Add more products**: Use the admin dashboard or API to add products to the `demo_products` collection

2. **Implement real checkout**: Create an `orders` collection and save order data:
   ```javascript
   // Example order document
   {
     "customer": {
       "name": "John Doe",
       "email": "john@example.com"
     },
     "items": [...cart items...],
     "total": 149.97,
     "status": "pending",
     "created_at": "2025-05-24T17:00:00Z"
   }
   ```

3. **Add user accounts**: Integrate with JSONdb's RBAC system for user authentication

4. **Implement inventory tracking**: Update product stock levels when orders are placed

5. **Add product images**: Store image URLs in product documents

## JSONdb Features Demonstrated

- **Document Storage**: Products stored as JSON documents
- **Collections**: Organized data in the `demo_products` collection
- **RESTful API**: Standard HTTP methods for CRUD operations
- **Real-time Updates**: Changes made through admin dashboard reflect immediately
- **No Schema Required**: Products can have different fields (features, material, capacity)
- **JavaScript-friendly**: Natural JSON format works seamlessly with frontend JavaScript

## Troubleshooting

1. **"Error loading products"**: Ensure JSONdb server is running on port 5000
2. **CORS errors**: JSONdb server includes CORS headers by default
3. **Authentication issues**: The demo works without authentication, but you can add auth tokens if needed

## Next Steps

This demo shows the basics of using JSONdb as a backend. You can:

- Build more complex queries using JSONdb's query language
- Implement data validation using JavaScript validators
- Add data transformations for computed fields
- Create indexes for better search performance
- Use the transaction API for atomic operations

Visit the admin dashboard to explore more JSONdb features!