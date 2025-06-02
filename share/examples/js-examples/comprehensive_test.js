#!/usr/bin/env node

/**
 * Comprehensive Native JavaScript Storage System Test
 * Tests all API endpoints and functionality of the native JS storage system
 */

const https = require('https');
const http = require('http');

// Configuration
const CONFIG = {
    host: 'localhost',
    port: 5000,
    useHttps: false,
    // For testing, we'll use system user credentials
    token: null // Will be set after authentication
};

// Sample JavaScript scripts for testing
const SAMPLE_SCRIPTS = {
    validator: {
        name: "product_validator",
        description: "Validates product documents for required fields and business rules",
        type: "validator",
        collection_pattern: "products",
        trigger_type: 1, // JS_TRIGGER_DOCUMENT_TAG
        trigger_tags: ["validate", "product"],
        script_code: `
function validateDocument(doc, operation) {
    const errors = [];
    
    // Required fields validation
    if (!doc.name || doc.name.trim().length === 0) {
        errors.push("Product name is required");
    }
    
    if (typeof doc.price !== 'number' || doc.price <= 0) {
        errors.push("Product price must be a positive number");
    }
    
    if (!doc.category || doc.category.trim().length === 0) {
        errors.push("Product category is required");
    }
    
    // Business rules validation
    if (doc.price > 10000) {
        errors.push("Product price cannot exceed $10,000");
    }
    
    if (doc.stock && doc.stock < 0) {
        errors.push("Stock quantity cannot be negative");
    }
    
    // Return validation result
    if (errors.length > 0) {
        console.log("Validation failed:", errors);
        return false;
    }
    
    console.log("Product validation passed");
    return true;
}`,
        rbac_permissions: ["READ", "VALIDATE"]
    },
    
    transformer: {
        name: "product_transformer",
        description: "Transforms product documents with timestamps, SKU generation, and normalization",
        type: "transformer", 
        collection_pattern: "products",
        trigger_type: 3, // JS_TRIGGER_OPERATION
        trigger_tags: ["transform", "product"],
        script_code: `
function transformDocument(doc, operation) {
    const now = new Date().toISOString();
    
    // Add timestamps
    if (operation === 'insert') {
        doc.created_at = now;
        doc.updated_at = now;
        
        // Generate SKU if not provided
        if (!doc.sku) {
            const category = (doc.category || 'GEN').substring(0, 3).toUpperCase();
            const random = Math.floor(Math.random() * 10000).toString().padStart(4, '0');
            doc.sku = category + '-' + random;
        }
        
        // Set default values
        if (doc.stock === undefined) {
            doc.stock = 0;
        }
        if (!doc.status) {
            doc.status = 'active';
        }
    } else if (operation === 'update') {
        doc.updated_at = now;
    }
    
    // Normalize product name
    if (doc.name) {
        doc.name = doc.name.trim().replace(/\\s+/g, ' ');
        // Capitalize first letter of each word
        doc.name = doc.name.replace(/\\b\\w/g, l => l.toUpperCase());
    }
    
    // Calculate sale price if discount is provided
    if (doc.price && doc.discount && typeof doc.discount === 'number') {
        doc.sale_price = parseFloat((doc.price * (1 - doc.discount / 100)).toFixed(2));
    }
    
    // Set stock status
    doc.in_stock = (doc.stock || 0) > 0;
    
    console.log("Product transformed:", doc.name);
    return doc;
}`,
        rbac_permissions: ["READ", "WRITE"]
    },
    
    function: {
        name: "inventory_analytics",
        description: "Analyzes inventory data and generates comprehensive reports",
        type: "function",
        collection_pattern: "products", 
        trigger_type: 4, // JS_TRIGGER_MANUAL
        trigger_tags: ["analytics", "inventory"],
        script_code: `
function userFunction(args) {
    const { collection, minStock, maxPrice } = args;
    
    console.log("Starting inventory analytics for collection:", collection);
    
    // Validate parameters
    if (!collection) {
        return { error: "Collection name is required" };
    }
    
    const minimumStock = minStock || 5;
    const maximumPrice = maxPrice || Infinity;
    
    // Simulate getting products (in real implementation, this would use database functions)
    const products = [
        { _id: "1", name: "Laptop", category: "Electronics", price: 999, stock: 10, status: "active" },
        { _id: "2", name: "Mouse", category: "Electronics", price: 25, stock: 2, status: "active" },
        { _id: "3", name: "Keyboard", category: "Electronics", price: 75, stock: 8, status: "active" },
        { _id: "4", name: "Book", category: "Education", price: 15, stock: 0, status: "inactive" }
    ];
    
    // Analytics processing
    const categories = {};
    const lowStockItems = [];
    let totalValue = 0;
    
    products.forEach(product => {
        if (product.status === 'inactive') return;
        
        const category = product.category || 'uncategorized';
        if (!categories[category]) {
            categories[category] = {
                count: 0,
                total_value: 0,
                avg_price: 0,
                low_stock: 0
            };
        }
        
        categories[category].count++;
        
        const price = product.price || 0;
        if (price <= maximumPrice) {
            const value = price * (product.stock || 0);
            categories[category].total_value += value;
            totalValue += value;
        }
        
        if ((product.stock || 0) < minimumStock) {
            categories[category].low_stock++;
            lowStockItems.push({
                id: product._id,
                name: product.name,
                stock: product.stock || 0,
                category: category
            });
        }
    });
    
    // Calculate averages
    Object.keys(categories).forEach(category => {
        const categoryProducts = products.filter(p => (p.category || 'uncategorized') === category);
        const totalPrice = categoryProducts.reduce((sum, p) => sum + (p.price || 0), 0);
        categories[category].avg_price = categoryProducts.length > 0 
            ? parseFloat((totalPrice / categoryProducts.length).toFixed(2))
            : 0;
    });
    
    // Sort low stock items
    lowStockItems.sort((a, b) => a.stock - b.stock);
    
    console.log("Analytics completed, found", lowStockItems.length, "low stock items");
    
    return {
        inventory_summary: {
            total_products: products.length,
            total_value: parseFloat(totalValue.toFixed(2)),
            categories: categories,
            low_stock_count: lowStockItems.length,
            low_stock_items: lowStockItems
        },
        parameters: {
            min_stock_threshold: minimumStock,
            max_price_considered: maximumPrice
        },
        generated_at: new Date().toISOString()
    };
}`,
        rbac_permissions: ["READ", "EXECUTE"]
    }
};

// HTTP(S) request helper
function makeRequest(options, data = null) {
    return new Promise((resolve, reject) => {
        const client = CONFIG.useHttps ? https : http;
        
        // Handle self-signed certificates for testing
        if (CONFIG.useHttps) {
            process.env["NODE_TLS_REJECT_UNAUTHORIZED"] = 0;
        }
        
        const requestOptions = {
            hostname: CONFIG.host,
            port: CONFIG.port,
            path: options.path,
            method: options.method || 'GET',
            headers: {
                'Content-Type': 'application/json',
                ...options.headers
            }
        };
        
        if (CONFIG.token) {
            requestOptions.headers['Authorization'] = `Bearer ${CONFIG.token}`;
        }
        
        const req = client.request(requestOptions, (res) => {
            let body = '';
            res.on('data', (chunk) => body += chunk);
            res.on('end', () => {
                try {
                    const parsed = JSON.parse(body);
                    resolve({ status: res.statusCode, headers: res.headers, data: parsed });
                } catch (e) {
                    resolve({ status: res.statusCode, headers: res.headers, data: body });
                }
            });
        });
        
        req.on('error', reject);
        
        if (data) {
            req.write(JSON.stringify(data));
        }
        
        req.end();
    });
}

// Test functions
async function testServerHealth() {
    console.log("\n🏥 Testing server health...");
    
    try {
        const response = await makeRequest({ path: '/api/health' });
        
        if (response.status === 200) {
            console.log("✅ Server is healthy");
            console.log("   Status:", response.data.status);
            console.log("   Uptime:", response.data.uptime_seconds, "seconds");
        } else {
            console.log("❌ Server health check failed:", response.status);
        }
    } catch (error) {
        console.error("❌ Health check error:", error.message);
        throw new Error("Server is not accessible");
    }
}

async function testAuthentication() {
    console.log("\n🔐 Testing authentication...");
    
    try {
        const response = await makeRequest({
            path: '/api/auth/login',
            method: 'POST'
        }, {
            username: 'admin',
            password: 'admin' // Default credentials
        });
        
        if (response.status === 200 && response.data.token) {
            CONFIG.token = response.data.token;
            console.log("✅ Authentication successful");
            console.log("   Token length:", CONFIG.token.length);
        } else {
            console.log("❌ Authentication failed:", response.status, response.data);
            throw new Error("Cannot authenticate");
        }
    } catch (error) {
        console.error("❌ Authentication error:", error.message);
        throw error;
    }
}

async function testStoreScript(scriptType, scriptData) {
    console.log(`\n💾 Testing script storage (${scriptType})...`);
    
    try {
        const response = await makeRequest({
            path: '/api/js/native/store',
            method: 'POST'
        }, scriptData);
        
        if (response.status === 201) {
            console.log("✅ Script stored successfully");
            console.log("   Script ID:", response.data.script_id);
            console.log("   Script type:", response.data.script_type);
            return response.data.script_id;
        } else {
            console.log("❌ Script storage failed:", response.status);
            console.log("   Error:", response.data);
            return null;
        }
    } catch (error) {
        console.error("❌ Script storage error:", error.message);
        return null;
    }
}

async function testGetScript(scriptId) {
    console.log(`\n📖 Testing script retrieval (${scriptId})...`);
    
    try {
        const response = await makeRequest({
            path: `/api/js/native/script/${scriptId}`
        });
        
        if (response.status === 200) {
            console.log("✅ Script retrieved successfully");
            console.log("   Name:", response.data.name);
            console.log("   Type:", response.data.type);
            console.log("   Version:", response.data.version);
            return response.data;
        } else {
            console.log("❌ Script retrieval failed:", response.status);
            return null;
        }
    } catch (error) {
        console.error("❌ Script retrieval error:", error.message);
        return null;
    }
}

async function testListScripts() {
    console.log("\n📋 Testing script listing...");
    
    try {
        const response = await makeRequest({
            path: '/api/js/native/list?type=validator'
        });
        
        if (response.status === 200) {
            console.log("✅ Script listing successful");
            console.log("   Found scripts:", response.data.count);
            response.data.scripts.forEach(script => {
                console.log(`   - ${script.name} (${script.type})`);
            });
            return response.data.scripts;
        } else {
            console.log("❌ Script listing failed:", response.status);
            return [];
        }
    } catch (error) {
        console.error("❌ Script listing error:", error.message);
        return [];
    }
}

async function testExecuteScript(scriptId) {
    console.log(`\n⚡ Testing script execution (${scriptId})...`);
    
    try {
        const response = await makeRequest({
            path: '/api/js/native/execute',
            method: 'POST'
        }, {
            script_id: scriptId,
            input_data: {
                collection: "products",
                minStock: 5,
                maxPrice: 1000
            }
        });
        
        if (response.status === 200) {
            console.log("✅ Script execution successful");
            console.log("   Execution time:", response.data.execution_time_ms, "ms");
            console.log("   Result preview:", JSON.stringify(response.data.result, null, 2).substring(0, 200) + "...");
            return response.data;
        } else {
            console.log("❌ Script execution failed:", response.status);
            console.log("   Error:", response.data);
            return null;
        }
    } catch (error) {
        console.error("❌ Script execution error:", error.message);
        return null;
    }
}

async function testGetMetrics() {
    console.log("\n📊 Testing metrics retrieval...");
    
    try {
        const response = await makeRequest({
            path: '/api/js/native/metrics'
        });
        
        if (response.status === 200) {
            console.log("✅ Metrics retrieval successful");
            console.log("   Total executions:", response.data.total_executions);
            console.log("   Success rate:", (response.data.success_rate * 100).toFixed(1) + "%");
            if (response.data.avg_execution_time_ms) {
                console.log("   Avg execution time:", response.data.avg_execution_time_ms.toFixed(2), "ms");
            }
            return response.data;
        } else {
            console.log("❌ Metrics retrieval failed:", response.status);
            return null;
        }
    } catch (error) {
        console.error("❌ Metrics retrieval error:", error.message);
        return null;
    }
}

async function testTaggedExecution() {
    console.log("\n🏷️ Testing tagged function execution...");
    
    try {
        const response = await makeRequest({
            path: '/api/js/native/execute-tagged',
            method: 'POST'
        }, {
            collection: "products",
            tag: "analytics",
            input_data: {
                collection: "products",
                minStock: 3,
                maxPrice: 500
            }
        });
        
        if (response.status === 200) {
            console.log("✅ Tagged execution successful");
            console.log("   Functions executed:", response.data.count);
            response.data.results.forEach(result => {
                console.log(`   - ${result.function_name}: ${result.success ? 'SUCCESS' : 'FAILED'}`);
            });
            return response.data;
        } else {
            console.log("❌ Tagged execution failed:", response.status);
            return null;
        }
    } catch (error) {
        console.error("❌ Tagged execution error:", error.message);
        return null;
    }
}

// Main test runner
async function runComprehensiveTest() {
    console.log("🚀 Starting Comprehensive Native JavaScript Storage System Test");
    console.log("=" * 80);
    
    const testResults = {
        passed: 0,
        failed: 0,
        scriptIds: {}
    };
    
    try {
        // Phase 1: Basic connectivity and authentication
        await testServerHealth();
        testResults.passed++;
        
        await testAuthentication();
        testResults.passed++;
        
        // Phase 2: Script management
        console.log("\n" + "=".repeat(50));
        console.log("📝 PHASE 2: SCRIPT MANAGEMENT TESTING");
        console.log("=".repeat(50));
        
        // Store test scripts
        for (const [type, script] of Object.entries(SAMPLE_SCRIPTS)) {
            const scriptId = await testStoreScript(type, script);
            if (scriptId) {
                testResults.scriptIds[type] = scriptId;
                testResults.passed++;
            } else {
                testResults.failed++;
            }
        }
        
        // Test script retrieval
        for (const [type, scriptId] of Object.entries(testResults.scriptIds)) {
            const script = await testGetScript(scriptId);
            if (script) {
                testResults.passed++;
            } else {
                testResults.failed++;
            }
        }
        
        // Test script listing
        const scripts = await testListScripts();
        if (scripts.length > 0) {
            testResults.passed++;
        } else {
            testResults.failed++;
        }
        
        // Phase 3: Script execution
        console.log("\n" + "=".repeat(50));
        console.log("⚡ PHASE 3: SCRIPT EXECUTION TESTING");
        console.log("=".repeat(50));
        
        // Test manual script execution
        if (testResults.scriptIds.function) {
            const result = await testExecuteScript(testResults.scriptIds.function);
            if (result) {
                testResults.passed++;
            } else {
                testResults.failed++;
            }
        }
        
        // Test tagged execution
        const taggedResult = await testTaggedExecution();
        if (taggedResult) {
            testResults.passed++;
        } else {
            testResults.failed++;
        }
        
        // Phase 4: Metrics and monitoring
        console.log("\n" + "=".repeat(50));
        console.log("📊 PHASE 4: METRICS AND MONITORING");
        console.log("=".repeat(50));
        
        const metrics = await testGetMetrics();
        if (metrics) {
            testResults.passed++;
        } else {
            testResults.failed++;
        }
        
        // Final results
        console.log("\n" + "=".repeat(80));
        console.log("🎯 TEST RESULTS SUMMARY");
        console.log("=".repeat(80));
        console.log(`✅ Passed: ${testResults.passed}`);
        console.log(`❌ Failed: ${testResults.failed}`);
        console.log(`📊 Success Rate: ${((testResults.passed / (testResults.passed + testResults.failed)) * 100).toFixed(1)}%`);
        
        if (testResults.failed === 0) {
            console.log("\n🎉 ALL TESTS PASSED! Native JavaScript storage system is fully operational.");
        } else {
            console.log(`\n⚠️  ${testResults.failed} tests failed. Please review the errors above.`);
        }
        
        console.log("\n📋 Stored Script IDs for further testing:");
        Object.entries(testResults.scriptIds).forEach(([type, id]) => {
            console.log(`   ${type}: ${id}`);
        });
        
    } catch (error) {
        console.error("\n💥 Critical test failure:", error.message);
        testResults.failed++;
    }
    
    return testResults;
}

// Run the test if this file is executed directly
if (require.main === module) {
    runComprehensiveTest()
        .then(results => {
            process.exit(results.failed === 0 ? 0 : 1);
        })
        .catch(error => {
            console.error("Test runner failed:", error);
            process.exit(1);
        });
}

module.exports = {
    runComprehensiveTest,
    testServerHealth,
    testAuthentication,
    testStoreScript,
    testGetScript,
    testListScripts,
    testExecuteScript,
    testGetMetrics,
    testTaggedExecution,
    SAMPLE_SCRIPTS,
    CONFIG
};