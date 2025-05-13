// JavaScript script to load data into the JSON Database

// Load the sample data from file
const fs = require('fs');
const path = require('path');

// Read the sample data
const sampleData = JSON.parse(fs.readFileSync('../data/sample_data.json', 'utf8'));

// Function to load data into the database
function loadData() {
  console.log("Starting data load process...");
  
  // Clear any existing data
  db.deleteCollection("users");
  db.deleteCollection("products");
  db.deleteCollection("orders");
  
  // Create collections if they don't exist
  if (!db.collectionExists("users")) {
    console.log("Creating users collection...");
    db.createCollection("users");
  }
  
  if (!db.collectionExists("products")) {
    console.log("Creating products collection...");
    db.createCollection("products");
  }
  
  if (!db.collectionExists("orders")) {
    console.log("Creating orders collection...");
    db.createCollection("orders");
  }
  
  // Insert users
  console.log("Inserting users...");
  sampleData.users.forEach(user => {
    db.insert("users", user);
  });
  
  // Insert products
  console.log("Inserting products...");
  sampleData.products.forEach(product => {
    db.insert("products", product);
  });
  
  // Insert orders
  console.log("Inserting orders...");
  sampleData.orders.forEach(order => {
    db.insert("orders", order);
  });
  
  // Create indexes for faster queries
  console.log("Creating indexes...");
  db.createIndex("users", "id");
  db.createIndex("products", "id");
  db.createIndex("orders", "id");
  db.createIndex("orders", "user_id");
  
  console.log("Data load completed successfully!");
  
  // Query to verify data was loaded
  const userCount = db.count("users");
  const productCount = db.count("products");
  const orderCount = db.count("orders");
  
  console.log(`Verification: Database contains ${userCount} users, ${productCount} products, and ${orderCount} orders.`);
  
  // Example query - find all orders for a specific user
  console.log("\nExample query - Orders for user Jane Smith (id: 2):");
  const userOrders = db.find("orders", { user_id: 2 });
  console.log(JSON.stringify(userOrders, null, 2));
}

// Run the load function
loadData();