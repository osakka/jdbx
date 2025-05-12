// Simple JavaScript function to test the QuickJS integration
function testFunction(x, y) {
    return {
        sum: x + y,
        product: x * y,
        difference: x - y,
        quotient: x / y
    };
}

// Test database operations
function testDb() {
    // Check if db object is available
    if (typeof db === "undefined") {
        return { error: "Database object not available" };
    }
    
    // Try to access a collection
    try {
        const collection = db.getCollection("test_collection");
        return { success: true, message: "Database access successful" };
    } catch (err) {
        return { error: err.toString() };
    }
}

// Return a result for testing
({ 
    mathResult: testFunction(10, 5),
    dbTest: testDb()
})