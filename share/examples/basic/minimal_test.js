/**
 * Minimal test script for the JSON Database
 */

// Check if the database object exists
console.log("Testing database connection...");

if (typeof db === 'undefined') {
    console.log("ERROR: Database object 'db' is not defined!");
} else {
    console.log("SUCCESS: Database object 'db' is available");
    
    // Try to add a simple object to database
    try {
        // Create a simple object
        const testData = {
            id: "test1",
            name: "Test Object",
            timestamp: new Date().toISOString()
        };
        
        console.log("Created test object: " + JSON.stringify(testData));
        
        // Store it
        console.log("Attempting to store data...");
        db.save(testData);
        console.log("Data saved successfully");
    } catch (e) {
        console.log("Error during database operation: " + e.message);
    }
}

console.log("Test complete!");