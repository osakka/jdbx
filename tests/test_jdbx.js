// Test JDBX functionality
console.log("Testing JDBX database...");

// Create a test document
const testDoc = {
    name: "Test User",
    email: "test@example.com",
    age: 30,
    data: "x".repeat(3000), // Large data to test overflow pages
    timestamp: new Date().toISOString()
};

console.log("Creating test document:", JSON.stringify(testDoc).substring(0, 100) + "...");

// Test CRUD operations
try {
    // Insert
    const result = db.insert("test_collection", testDoc);
    console.log("Insert result:", result ? "SUCCESS" : "FAILED");
    
    if (result && result._id) {
        // Retrieve
        const retrieved = db.find_by_id("test_collection", result._id);
        console.log("Retrieve result:", retrieved ? "SUCCESS" : "FAILED");
        
        // Update
        const updated = db.update("test_collection", result._id, {
            $set: { updated: true, age: 31 }
        });
        console.log("Update result:", updated ? "SUCCESS" : "FAILED");
        
        // Find
        const found = db.find("test_collection", { name: "Test User" });
        console.log("Find result:", found && found.length > 0 ? "SUCCESS" : "FAILED");
        
        // Delete
        const deleted = db.delete("test_collection", result._id);
        console.log("Delete result:", deleted ? "SUCCESS" : "FAILED");
    }
    
    // List collections
    const collections = db.list_collections();
    console.log("Collections:", collections);
    
    console.log("\nJDBX test completed successfully!");
    
} catch (e) {
    console.error("Test failed:", e);
}