// Example transformer for the 'products' collection
function transformDocument(doc, operation) {
  // Add timestamps
  const now = new Date().toISOString();
  
  if (operation === 'insert') {
    // For new documents
    doc.created_at = now;
    doc.updated_at = now;
    
    // Set default values
    if (doc.stock === undefined) {
      doc.stock = 0;
    }
    
    if (!doc.status) {
      doc.status = 'active';
    }
  } 
  
  if (operation === 'update') {
    // Update timestamp when document is modified
    doc.updated_at = now;
  }
  
  // Generate SKU if not provided
  if (operation === 'insert' && !doc.sku) {
    const category = doc.category || 'GEN';
    const categoryPrefix = category.substring(0, 3).toUpperCase();
    const randomPart = Math.floor(Math.random() * 10000).toString().padStart(4, '0');
    doc.sku = `${categoryPrefix}-${randomPart}`;
  }
  
  // Normalize product name
  if (doc.name) {
    doc.name = doc.name.trim();
    
    // Capitalize first letter of each word
    doc.name = doc.name.replace(/\b\w/g, l => l.toUpperCase());
  }
  
  // Calculate sale price if discount is provided
  if (doc.price && doc.discount && typeof doc.discount === 'number') {
    doc.sale_price = parseFloat((doc.price * (1 - doc.discount / 100)).toFixed(2));
  }
  
  // Check stock status
  if (operation === 'update' || operation === 'insert') {
    if (doc.stock <= 0) {
      doc.in_stock = false;
    } else {
      doc.in_stock = true;
    }
  }
  
  return doc;
}

// To register this transformer:
// 1. Save this file as /transforms/products.js
// OR
// 2. Use the HTTP API:
/*
curl -X POST http://localhost:5000/api/js/transformers/register \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d '{
    "collection": "products",
    "code": "function transformDocument(doc, operation) { ... }"
  }'
*/