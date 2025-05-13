/**
 * Database insertion script for JSON Database
 * This script inserts sample data into the database
 */

// Print the available methods on the database object to understand the API
function checkDbMethods() {
  console.log("Checking available database methods...");
  
  if (typeof db === 'undefined') {
    console.log("ERROR: Database object 'db' is not defined!");
    return false;
  }
  
  console.log("Database object exists. Available methods:");
  for (const prop in db) {
    if (typeof db[prop] === 'function') {
      console.log(`- ${prop}`);
    }
  }
  
  return true;
}

// Insert users data into the database
function insertUsers() {
  console.log("\nInserting users...");
  
  const users = [
    {
      id: 1,
      username: "john_doe",
      email: "john@example.com",
      role: "admin"
    },
    {
      id: 2,
      username: "jane_smith",
      email: "jane@example.com",
      role: "user"
    },
    {
      id: 3,
      username: "bob_johnson",
      email: "bob@example.com",
      role: "user"
    }
  ];
  
  try {
    // Try different methods based on the API
    if (typeof db.insertDocument === 'function') {
      users.forEach(user => {
        db.insertDocument("users", user);
      });
    } else if (typeof db.insert === 'function') {
      users.forEach(user => {
        db.insert("users", user);
      });
    } else if (typeof db.createCollection === 'function') {
      // First create the collection if it doesn't exist
      if (!db.collectionExists || !db.collectionExists("users")) {
        db.createCollection("users");
      }
      
      // Then insert the documents
      users.forEach(user => {
        db.createDocument("users", user);
      });
    } else {
      console.log("ERROR: Could not find appropriate insertion method!");
      return false;
    }
    
    console.log("Users inserted successfully!");
    return true;
  } catch (e) {
    console.log("ERROR: Failed to insert users: " + e.message);
    return false;
  }
}

// Insert products data into the database
function insertProducts() {
  console.log("\nInserting products...");
  
  const products = [
    {
      id: 101,
      name: "Laptop",
      price: 999.99,
      category: "electronics",
      in_stock: true
    },
    {
      id: 102,
      name: "Smartphone",
      price: 699.99,
      category: "electronics",
      in_stock: true
    },
    {
      id: 103,
      name: "Headphones",
      price: 149.99,
      category: "accessories",
      in_stock: false
    }
  ];
  
  try {
    // Try different methods based on the API
    if (typeof db.insertDocument === 'function') {
      products.forEach(product => {
        db.insertDocument("products", product);
      });
    } else if (typeof db.insert === 'function') {
      products.forEach(product => {
        db.insert("products", product);
      });
    } else if (typeof db.createCollection === 'function') {
      // First create the collection if it doesn't exist
      if (!db.collectionExists || !db.collectionExists("products")) {
        db.createCollection("products");
      }
      
      // Then insert the documents
      products.forEach(product => {
        db.createDocument("products", product);
      });
    } else {
      console.log("ERROR: Could not find appropriate insertion method!");
      return false;
    }
    
    console.log("Products inserted successfully!");
    return true;
  } catch (e) {
    console.log("ERROR: Failed to insert products: " + e.message);
    return false;
  }
}

// Insert orders data into the database
function insertOrders() {
  console.log("\nInserting orders...");
  
  const orders = [
    {
      id: 1001,
      user_id: 2,
      products: [101, 103],
      total: 1149.98,
      date: "2023-05-10"
    },
    {
      id: 1002,
      user_id: 3,
      products: [102],
      total: 699.99,
      date: "2023-05-11"
    }
  ];
  
  try {
    // Try different methods based on the API
    if (typeof db.insertDocument === 'function') {
      orders.forEach(order => {
        db.insertDocument("orders", order);
      });
    } else if (typeof db.insert === 'function') {
      orders.forEach(order => {
        db.insert("orders", order);
      });
    } else if (typeof db.createCollection === 'function') {
      // First create the collection if it doesn't exist
      if (!db.collectionExists || !db.collectionExists("orders")) {
        db.createCollection("orders");
      }
      
      // Then insert the documents
      orders.forEach(order => {
        db.createDocument("orders", order);
      });
    } else {
      console.log("ERROR: Could not find appropriate insertion method!");
      return false;
    }
    
    console.log("Orders inserted successfully!");
    return true;
  } catch (e) {
    console.log("ERROR: Failed to insert orders: " + e.message);
    return false;
  }
}

// Query the database to verify data was inserted
function queryData() {
  console.log("\nQuerying data to verify insertion...");
  
  try {
    if (typeof db.getCollection === 'function') {
      const users = db.getCollection("users");
      const products = db.getCollection("products");
      const orders = db.getCollection("orders");
      
      console.log(`Users: ${users ? users.length : 0} documents`);
      console.log(`Products: ${products ? products.length : 0} documents`);
      console.log(`Orders: ${orders ? orders.length : 0} documents`);
    } else if (typeof db.queryDocuments === 'function') {
      const users = db.queryDocuments("users", {});
      const products = db.queryDocuments("products", {});
      const orders = db.queryDocuments("orders", {});
      
      console.log(`Users: ${users ? users.length : 0} documents`);
      console.log(`Products: ${products ? products.length : 0} documents`);
      console.log(`Orders: ${orders ? orders.length : 0} documents`);
    } else if (typeof db.find === 'function') {
      const users = db.find("users", {});
      const products = db.find("products", {});
      const orders = db.find("orders", {});
      
      console.log(`Users: ${users ? users.length : 0} documents`);
      console.log(`Products: ${products ? products.length : 0} documents`);
      console.log(`Orders: ${orders ? orders.length : 0} documents`);
    } else {
      console.log("ERROR: Could not find appropriate query method!");
      return false;
    }
    
    return true;
  } catch (e) {
    console.log("ERROR: Failed to query data: " + e.message);
    return false;
  }
}

// Main function to execute everything
function main() {
  console.log("Starting database insertion...");
  
  // First check available methods
  if (!checkDbMethods()) {
    return { success: false, message: "Failed to verify database methods" };
  }
  
  // Insert data
  const usersResult = insertUsers();
  const productsResult = insertProducts();
  const ordersResult = insertOrders();
  
  // Query data to verify
  if (usersResult && productsResult && ordersResult) {
    queryData();
  }
  
  return {
    success: usersResult && productsResult && ordersResult,
    message: "Database insertion completed"
  };
}

// Execute the main function
const result = main();
result;