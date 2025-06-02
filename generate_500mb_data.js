#!/usr/bin/env node

// Generate 500MB of test data with manageable document sizes
const http = require('http');

const API_URL = 'http://localhost:5000/api';
const COLLECTION_NAME = 'large_test_data';
const TARGET_SIZE_MB = 500;
const DOC_SIZE_KB = 100; // 100KB per document - more manageable
const DOC_SIZE_BYTES = DOC_SIZE_KB * 1024;

// Generate random data
function generateRandomString(length) {
    const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    let result = '';
    for (let i = 0; i < length; i++) {
        result += chars.charAt(Math.floor(Math.random() * chars.length));
    }
    return result;
}

function generateTestDocument(index) {
    // Create a document with multiple fields to reach ~100KB
    const fieldSize = 10000; // 10KB per field
    
    return {
        docId: `test-${Date.now()}-${index}`,
        index: index,
        timestamp: new Date().toISOString(),
        
        // Data fields
        data1: generateRandomString(fieldSize),
        data2: generateRandomString(fieldSize),
        data3: generateRandomString(fieldSize),
        data4: generateRandomString(fieldSize),
        data5: generateRandomString(fieldSize),
        data6: generateRandomString(fieldSize),
        data7: generateRandomString(fieldSize),
        data8: generateRandomString(fieldSize),
        data9: generateRandomString(fieldSize),
        data10: generateRandomString(fieldSize),
        
        // Metadata
        metadata: {
            type: 'test_data',
            batch: Math.floor(index / 100),
            tags: ['performance', 'test', 'large_dataset'],
            stats: {
                random: Math.random(),
                index: index,
                created: Date.now()
            }
        }
    };
}

async function makeRequest(url, method = 'GET', data = null) {
    return new Promise((resolve, reject) => {
        const urlObj = new URL(url);
        
        const options = {
            hostname: urlObj.hostname,
            port: urlObj.port || 80,
            path: urlObj.pathname + urlObj.search,
            method: method,
            headers: {
                'Content-Type': 'application/json'
            },
            timeout: 30000 // 30 second timeout
        };
        
        if (data) {
            const jsonData = JSON.stringify(data);
            options.headers['Content-Length'] = Buffer.byteLength(jsonData);
        }
        
        const req = http.request(options, (res) => {
            let body = '';
            res.on('data', (chunk) => body += chunk);
            res.on('end', () => {
                try {
                    const result = body ? JSON.parse(body) : {};
                    resolve({ status: res.statusCode, data: result });
                } catch (e) {
                    resolve({ status: res.statusCode, data: { raw: body } });
                }
            });
        });
        
        req.on('timeout', () => {
            req.destroy();
            reject(new Error('Request timeout'));
        });
        
        req.on('error', reject);
        
        if (data) {
            req.write(JSON.stringify(data));
        }
        
        req.end();
    });
}

async function getCollectionCount() {
    try {
        const response = await makeRequest(`${API_URL}/collections`);
        if (response.status === 200) {
            const collection = response.data.collections.find(c => c.name === COLLECTION_NAME);
            return collection ? collection.documentCount : 0;
        }
    } catch (error) {
        console.error('Error getting collection count:', error.message);
    }
    return 0;
}

async function insertBatch(documents) {
    const results = await Promise.all(
        documents.map(async (doc) => {
            try {
                const response = await makeRequest(`${API_URL}/collections/${COLLECTION_NAME}/documents`, 'POST', doc);
                return response.status === 200 || response.status === 201;
            } catch (error) {
                return false;
            }
        })
    );
    
    return results.filter(r => r).length;
}

async function main() {
    console.log('🚀 JSONdb 500MB Test Data Generator');
    console.log(`📊 Target: ${TARGET_SIZE_MB}MB`);
    console.log(`📄 Document size: ~${DOC_SIZE_KB}KB each`);
    console.log(`📦 Total documents needed: ~${Math.ceil(TARGET_SIZE_MB * 1024 / DOC_SIZE_KB)}`);
    console.log('');
    
    // Get current count
    const existingCount = await getCollectionCount();
    const existingSizeMB = (existingCount * DOC_SIZE_KB / 1024).toFixed(1);
    console.log(`ℹ️  Collection has ${existingCount} documents (~${existingSizeMB}MB)`);
    
    const targetDocs = Math.ceil(TARGET_SIZE_MB * 1024 / DOC_SIZE_KB);
    const docsNeeded = Math.max(0, targetDocs - existingCount);
    
    if (docsNeeded === 0) {
        console.log('✅ Target size already reached!');
        return;
    }
    
    console.log(`📝 Need to add ${docsNeeded} more documents\n`);
    
    const batchSize = 20; // Process in small batches
    const startTime = Date.now();
    let totalInserted = 0;
    let currentIndex = existingCount;
    
    while (totalInserted < docsNeeded) {
        const remaining = docsNeeded - totalInserted;
        const thisBatchSize = Math.min(batchSize, remaining);
        
        // Generate batch
        const batch = [];
        for (let i = 0; i < thisBatchSize; i++) {
            batch.push(generateTestDocument(currentIndex++));
        }
        
        // Insert batch
        process.stdout.write(`Inserting batch of ${thisBatchSize} documents... `);
        const inserted = await insertBatch(batch);
        totalInserted += inserted;
        
        const totalDocs = existingCount + totalInserted;
        const totalSizeMB = (totalDocs * DOC_SIZE_KB / 1024).toFixed(1);
        const progress = (totalSizeMB / TARGET_SIZE_MB * 100).toFixed(1);
        
        console.log(`✓ ${inserted}/${thisBatchSize} succeeded`);
        console.log(`📊 Progress: ${totalDocs} docs, ~${totalSizeMB}MB (${progress}%)`);
        
        // Delay between batches
        await new Promise(resolve => setTimeout(resolve, 500));
        
        // Check for failures
        if (inserted < thisBatchSize * 0.5) {
            console.log('⚠️  Too many failures, stopping...');
            break;
        }
    }
    
    // Final stats
    const duration = ((Date.now() - startTime) / 1000).toFixed(1);
    const finalCount = existingCount + totalInserted;
    const finalSizeMB = (finalCount * DOC_SIZE_KB / 1024).toFixed(1);
    
    console.log('\n' + '='.repeat(50));
    console.log('✅ Data generation completed!');
    console.log(`⏱️  Duration: ${duration} seconds`);
    console.log(`📈 Documents added: ${totalInserted}`);
    console.log(`📊 Total documents: ${finalCount}`);
    console.log(`💾 Total size: ~${finalSizeMB}MB`);
    console.log(`🎯 Target reached: ${finalSizeMB >= TARGET_SIZE_MB ? 'YES' : 'NO'}`);
}

if (require.main === module) {
    main().catch(console.error);
}