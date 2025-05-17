// Example JavaScript query
// This demonstrates how to use the JS query API to perform complex queries

/*
This query finds products that:
1. Are in stock
2. Have a price between min and max values
3. Match a search term in name or description
4. Are sorted by price (ascending/descending)

To execute this query, use the HTTP API:

curl -X POST http://localhost:5000/api/js/query \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d '{
    "collection": "products",
    "query": "function query() { ... }"
  }'
*/

function query() {
  // These would normally come from query parameters
  const searchTerm = "laptop";
  const minPrice = 500;
  const maxPrice = 2000;
  const sortOrder = "asc"; // or "desc"
  const inStockOnly = true;
  
  // Get all products
  let products = db.getCollection("products");
  
  // Filter products based on criteria
  const filteredProducts = products.filter(product => {
    // Check if product is in stock if needed
    if (inStockOnly && (!product.in_stock || product.stock <= 0)) {
      return false;
    }
    
    // Price range check
    const price = product.price || 0;
    if (price < minPrice || price > maxPrice) {
      return false;
    }
    
    // Search term matching
    if (searchTerm) {
      const name = (product.name || "").toLowerCase();
      const description = (product.description || "").toLowerCase();
      const searchTermLower = searchTerm.toLowerCase();
      
      if (!name.includes(searchTermLower) && !description.includes(searchTermLower)) {
        return false;
      }
    }
    
    // Product passes all filters
    return true;
  });
  
  // Sort products
  if (sortOrder === "asc") {
    filteredProducts.sort((a, b) => (a.price || 0) - (b.price || 0));
  } else {
    filteredProducts.sort((a, b) => (b.price || 0) - (a.price || 0));
  }
  
  // Add pagination metadata
  const result = {
    total: filteredProducts.length,
    products: filteredProducts
  };
  
  return result;
}

// Alternatively for the HTTP API:
/*
curl -X POST http://localhost:5000/api/js/query \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d '{
    "collection": "products",
    "query": "return db.getCollection(\\"products\\").filter(p => p.price >= 500 && p.price <= 2000 && p.in_stock && (p.name.toLowerCase().includes(\\"laptop\\") || p.description.toLowerCase().includes(\\"laptop\\"))).sort((a, b) => a.price - b.price);"
  }'
*/