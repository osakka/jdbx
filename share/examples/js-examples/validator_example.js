// Example validator for the 'products' collection
function validateDocument(doc) {
  // Check required fields
  if (!doc.name) {
    addError('name', 'Product name is required');
  }
  
  if (!doc.price) {
    addError('price', 'Product price is required');
  } else if (typeof doc.price !== 'number') {
    addError('price', 'Price must be a number');
  } else if (doc.price < 0) {
    addError('price', 'Price cannot be negative');
  }
  
  if (doc.stock !== undefined) {
    if (typeof doc.stock !== 'number') {
      addError('stock', 'Stock must be a number');
    } else if (doc.stock < 0) {
      addError('stock', 'Stock cannot be negative');
    }
  }
  
  if (doc.category) {
    const validCategories = ['electronics', 'clothing', 'food', 'books', 'other'];
    if (!validCategories.includes(doc.category)) {
      addError('category', `Category must be one of: ${validCategories.join(', ')}`);
    }
  }
  
  // Return validation result
  return isValid;
}

// To register this validator:
// 1. Save this file as /validators/products.js
// OR
// 2. Use the HTTP API:
/*
curl -X POST http://localhost:5000/api/js/validators/register \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d '{
    "collection": "products",
    "code": "function validateDocument(doc) { ... }"
  }'
*/