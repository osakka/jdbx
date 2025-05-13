/**
 * Simple test script for the JSON Database
 */

// Check if the database object exists
if (typeof db === 'undefined') {
    console.log("ERROR: Database object 'db' is not defined!");
    throw new Error("Database object 'db' is not defined");
}

// List available methods on the database object
console.log("Database methods available:");
for (const prop in db) {
    if (typeof db[prop] === 'function') {
        console.log(`- ${prop}`);
    }
}

// Create a test collection
console.log("\nCreating test collection...");
if (typeof db.createCollection === 'function') {
    // First delete if exists
    if (typeof db.deleteCollection === 'function' && db.collectionExists("test_collection")) {
        db.deleteCollection("test_collection");
        console.log("Deleted existing test_collection");
    }
    
    db.createCollection("test_collection");
    console.log("Created test_collection");
} else if (typeof db.getCollection === 'function') {
    console.log("Using getCollection to get or create collection");
    const collection = db.getCollection("test_collection");
    console.log("Collection ready");
} else {
    console.log("WARNING: No method found to create collections");
}

// Insert test data
console.log("\nInserting test data...");
try {
    if (typeof db.insertDocument === 'function') {
        db.insertDocument("test_collection", { id: 1, name: "Test Item 1", value: 42 });
        db.insertDocument("test_collection", { id: 2, name: "Test Item 2", value: 84 });
        console.log("Inserted 2 documents using insertDocument()");
    } else if (typeof db.insert === 'function') {
        db.insert("test_collection", { id: 1, name: "Test Item 1", value: 42 });
        db.insert("test_collection", { id: 2, name: "Test Item 2", value: 84 });
        console.log("Inserted 2 documents using insert()");
    } else {
        console.log("WARNING: No method found to insert documents");
    }
} catch (e) {
    console.log("Error inserting data: " + e.message);
}

// Query test data
console.log("\nQuerying test data...");
try {
    let results;
    if (typeof db.queryDocuments === 'function') {
        results = db.queryDocuments("test_collection", {});
        console.log(`Found ${results.length} documents using queryDocuments()`);
    } else if (typeof db.find === 'function') {
        results = db.find("test_collection", {});
        console.log(`Found ${results.length} documents using find()`);
    } else if (typeof db.getCollection === 'function') {
        results = db.getCollection("test_collection");
        console.log(`Collection has ${results.length} documents using getCollection()`);
    } else {
        console.log("WARNING: No method found to query documents");
    }
    
    // Print the results
    if (results && results.length) {
        console.log("Documents:");
        for (let i = 0; i < results.length; i++) {
            console.log(JSON.stringify(results[i]));
        }
    }
} catch (e) {
    console.log("Error querying data: " + e.message);
}

console.log("\nTest complete!");