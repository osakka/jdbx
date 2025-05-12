# JavaScript API for JSON Database

This document describes the JavaScript API for interacting with the JSON database. The JSON database provides a built-in JavaScript engine (using QuickJS) that allows you to execute JavaScript code to interact with the database.

## Overview

The JavaScript API provides two ways to interact with the database:

1. **Direct Database API** - Core functions exposed through the global `db` object.
2. **Helper Library** - An optional, higher-level API that provides a more object-oriented interface.

## Using JavaScript with the Database

There are several ways to execute JavaScript code with the database:

1. **Command Line** - Use the `-js_eval` or `-js_eval_file` options:
   ```
   bin/jsondb_server -js_eval "db.getCollection('users')"
   bin/jsondb_server -js_eval_file path/to/script.js
   ```

2. **HTTP API** - Send JavaScript code to the `/api/js/eval` endpoint.

3. **Database Functions** - Define and register JavaScript functions that can be called from other parts of the system.

## Core Database API

The JSON database exposes a global `db` object with the following methods:

### Collection Operations

#### `db.getCollection(collectionName)`

Returns all documents in the specified collection.

```javascript
const users = db.getCollection('users');
console.log(`Found ${users.length} users`);
```

### Document Operations

#### `db.getDocument(collectionName, id)`

Retrieves a document by its ID.

```javascript
const user = db.getDocument('users', 'user123');
if (user) {
  console.log(`Found user: ${user.name}`);
}
```

#### `db.insertDocument(collectionName, document)`

Inserts a document into a collection. Creates the collection if it doesn't exist.

```javascript
const newUser = {
  id: 'user456', // ID is optional - one will be generated if not provided
  name: 'Jane Doe',
  email: 'jane@example.com'
};

const result = db.insertDocument('users', newUser);
console.log(`Inserted document with ID: ${result.id}`);
```

#### `db.updateDocument(collectionName, id, document)`

Updates a document.

```javascript
const updatedUser = {
  id: 'user456', // ID must match the id parameter
  name: 'Jane Smith',
  email: 'jane.smith@example.com'
};

const result = db.updateDocument('users', 'user456', updatedUser);
if (result) {
  console.log(`Updated user: ${result.name}`);
}
```

#### `db.deleteDocument(collectionName, id)`

Deletes a document.

```javascript
const deleted = db.deleteDocument('users', 'user456');
console.log(`Document deleted: ${deleted}`);
```

### Query Operations

#### `db.queryDocuments(collectionName, query)`

Queries documents in a collection.

```javascript
// Simple query - exact match
const activeUsers = db.queryDocuments('users', { status: 'active' });

// Complex query - multiple conditions
const results = db.queryDocuments('users', {
  age: { $gt: 30 },
  status: 'active'
});
```

## Helper Library

The database includes a JavaScript helper library (`db_helpers.js`) that provides a more convenient object-oriented interface.

### Loading the Helper Library

```javascript
// Load the helper library
load("functions/db_helpers.js");
```

### Using the Collection Class

The `Collection` class provides a wrapper around the database operations for a specific collection.

```javascript
// Create a collection object
const users = new Collection('users');

// Insert a document
const user = users.insert({ name: 'Alice', email: 'alice@example.com' });

// Get a document
const retrievedUser = users.get(user.id);

// Update a document
users.update(user.id, { name: 'Alice Smith', email: 'alice.smith@example.com' });

// Query documents
const activeUsers = users.query({ status: 'active' });

// Get all documents
const allUsers = users.findAll();

// Delete a document
users.delete(user.id);
```

### Using the JsonDB (DB) Class

The `JsonDB` class (aliased as `DB`) provides static methods for working with any collection.

```javascript
// Insert a document
const user = DB.insert('users', { name: 'Bob', email: 'bob@example.com' });

// Get a document
const retrievedUser = DB.get('users', user.id);

// Other operations
DB.update('users', user.id, { name: 'Bob Smith' });
DB.query('users', { status: 'active' });
DB.findAll('users');
DB.delete('users', user.id);
```

## Validators and Transformers

The database supports JavaScript validators and transformers for collections.

### Document Validators

Create a JavaScript file named `validators/{collection_name}.js` that exports a `validateDocument` function.

```javascript
// validators/users.js
function validateDocument(doc) {
  if (!doc.email) {
    addError('email', 'Email is required');
  }
  
  if (!doc.name || doc.name.length < 2) {
    addError('name', 'Name must be at least 2 characters');
  }
  
  return isValid; // Global variable set by the system
}
```

### Document Transformers

Create a JavaScript file named `transforms/{collection_name}.js` that exports a `transformDocument` function.

```javascript
// transforms/users.js
function transformDocument(doc, operation) {
  // Add timestamps
  if (operation === 'insert') {
    doc.createdAt = new Date().toISOString();
  }
  
  doc.updatedAt = new Date().toISOString();
  
  // Convert email to lowercase
  if (doc.email) {
    doc.email = doc.email.toLowerCase();
  }
  
  return doc;
}
```

## User-Defined Functions

You can register custom JavaScript functions that can be called from other parts of the system.

```javascript
// Define a function
function calculateTotal(items) {
  return items.reduce((total, item) => total + (item.price * item.quantity), 0);
}

// Register the function
db.registerFunction('calculateTotal', calculateTotal.toString());

// Later, call the function
const result = db.callFunction('calculateTotal', [
  { price: 10, quantity: 2 },
  { price: 15, quantity: 1 }
]);
// result = 35
```

## Examples

### Basic CRUD Operations

```javascript
// Load the helper library
load("functions/db_helpers.js");

// Create a collection
const products = new Collection('products');

// Insert documents
const product1 = products.insert({
  name: 'Laptop',
  price: 999,
  stock: 50
});

const product2 = products.insert({
  name: 'Smartphone',
  price: 699,
  stock: 100
});

// Find all products
const allProducts = products.findAll();
console.log(`Total products: ${allProducts.length}`);

// Query products
const expensiveProducts = products.query({ price: { $gt: 800 } });
console.log(`Expensive products: ${expensiveProducts.length}`);

// Update a product
products.update(product1.id, {
  name: 'High-end Laptop',
  price: 1299,
  stock: 30
});

// Delete a product
products.delete(product2.id);
```

### Data Processing

```javascript
// Load products and calculate total inventory value
const products = new Collection('products');
const allProducts = products.findAll();

// Calculate total inventory value
const totalValue = allProducts.reduce((sum, product) => {
  return sum + (product.price * product.stock);
}, 0);

console.log(`Total inventory value: $${totalValue}`);

// Group products by category
const categoryCounts = {};
allProducts.forEach(product => {
  const category = product.category || 'Uncategorized';
  categoryCounts[category] = (categoryCounts[category] || 0) + 1;
});

console.log('Products by category:');
Object.entries(categoryCounts).forEach(([category, count]) => {
  console.log(`- ${category}: ${count} products`);
});
```

### Working with Multiple Collections

```javascript
// Set up collections
const orders = new Collection('orders');
const customers = new Collection('customers');
const products = new Collection('products');

// Create an order with references
const newOrder = {
  customer_id: 'cust123',
  items: [
    { product_id: 'prod456', quantity: 2 },
    { product_id: 'prod789', quantity: 1 }
  ],
  date: new Date().toISOString()
};

// Save the order
const order = orders.insert(newOrder);

// Retrieve order with customer and product details
const orderDetails = orders.get(order.id);
const customer = customers.get(orderDetails.customer_id);

// Get product details
const orderProducts = orderDetails.items.map(item => {
  const product = products.get(item.product_id);
  return {
    ...product,
    quantity: item.quantity,
    subtotal: product.price * item.quantity
  };
});

// Calculate order total
const orderTotal = orderProducts.reduce((total, item) => {
  return total + item.subtotal;
}, 0);

// Create full order summary
const orderSummary = {
  order_id: orderDetails.id,
  date: orderDetails.date,
  customer: customer,
  items: orderProducts,
  total: orderTotal
};

console.log(JSON.stringify(orderSummary, null, 2));
```

## Performance Considerations

- JavaScript code is executed in the context of the database server, so extensive calculations can affect server performance.
- For large data sets, use queries to filter data at the database level rather than loading all data and filtering in JavaScript.
- Consider using batch operations for bulk updates to minimize the number of database calls.

## Error Handling

All JavaScript database functions can throw exceptions. Use try/catch blocks to handle errors:

```javascript
try {
  const result = db.insertDocument('users', { name: 'John' });
  console.log(`Inserted user: ${result.id}`);
} catch (error) {
  console.error(`Error inserting user: ${error.message}`);
}
```

## Future Enhancements

The following features are planned for future releases:

- Indexed queries for better performance
- Transaction support in JavaScript
- Aggregation framework
- Real-time event notifications
- Extended query language with more operators