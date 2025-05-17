// Example custom function for inventory analysis
function userFunction(args) {
  const { collection, minStock, maxPrice } = args;
  
  // Validate parameters
  if (!collection) {
    return { error: "Collection name is required" };
  }
  
  // Set defaults for optional parameters
  const minimumStock = minStock || 5;
  const maximumPrice = maxPrice || Infinity;
  
  // Get all products
  const products = db.getCollection(collection);
  
  // Group products by category
  const categories = {};
  const lowStockItems = [];
  
  products.forEach(product => {
    // Skip inactive products
    if (product.status === 'inactive') return;
    
    // Categorize product
    const category = product.category || 'uncategorized';
    if (!categories[category]) {
      categories[category] = {
        count: 0,
        total_value: 0,
        avg_price: 0,
        low_stock: 0
      };
    }
    
    // Update category statistics
    categories[category].count++;
    
    const price = product.price || 0;
    if (price <= maximumPrice) {
      categories[category].total_value += price * (product.stock || 0);
    }
    
    // Check if item is low on stock
    if ((product.stock || 0) < minimumStock) {
      categories[category].low_stock++;
      lowStockItems.push({
        id: product._id,
        name: product.name,
        sku: product.sku,
        stock: product.stock || 0,
        category: category
      });
    }
  });
  
  // Calculate average prices
  Object.keys(categories).forEach(category => {
    const categoryProducts = products.filter(p => (p.category || 'uncategorized') === category);
    const totalPrice = categoryProducts.reduce((sum, p) => sum + (p.price || 0), 0);
    categories[category].avg_price = categoryProducts.length > 0 
      ? parseFloat((totalPrice / categoryProducts.length).toFixed(2))
      : 0;
  });
  
  // Sort low stock items by stock level (ascending)
  lowStockItems.sort((a, b) => a.stock - b.stock);
  
  // Prepare the report
  return {
    inventory_summary: {
      total_products: products.length,
      categories: categories,
      low_stock_count: lowStockItems.length,
      low_stock_items: lowStockItems
    },
    parameters: {
      min_stock_threshold: minimumStock,
      max_price_considered: maximumPrice
    }
  };
}

// To register this function:
// 1. Save this file as /functions/inventoryAnalysis.js
// OR
// 2. Use the HTTP API:
/*
curl -X POST http://localhost:5000/api/js/functions/register \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d '{
    "name": "inventoryAnalysis",
    "code": "function userFunction(args) { ... }"
  }'
*/

// To call this function:
/*
curl -X POST http://localhost:5000/api/js/functions/inventoryAnalysis \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d '{
    "collection": "products",
    "minStock": 5,
    "maxPrice": 1000
  }'
*/