#!/usr/bin/env node

// Generate large test dataset for JSONdb performance testing
// Target: ~500MB of test documents with efficient batch processing

const fs = require('fs');
const crypto = require('crypto');

// Configuration
const API_URL = 'http://localhost:5000/api';
const COLLECTION_NAME = 'large_test_data';
const TARGET_SIZE_MB = 500;
const BATCH_SIZE = 50; // Smaller batches for better success rate
const ESTIMATED_DOC_SIZE = 5000; // Bytes per document (approx)

// Sample data generators (simplified for faster generation)
const SAMPLE_NAMES = ['Alice Johnson', 'Bob Smith', 'Carol Williams', 'David Brown', 'Emma Davis'];
const SAMPLE_COMPANIES = ['TechCorp Inc', 'DataSoft LLC', 'CloudVision Systems', 'InnovateTech'];
const SAMPLE_DEPARTMENTS = ['Engineering', 'Sales', 'Marketing', 'HR', 'Finance'];

function randomChoice(array) {
    return array[Math.floor(Math.random() * array.length)];
}

function generateLargeText(length = 2000) {
    // More efficient text generation
    const baseText = 'Lorem ipsum dolor sit amet consectetur adipiscing elit sed do eiusmod tempor incididunt ut labore et dolore magna aliqua ';
    let text = '';
    while (text.length < length) {
        text += baseText;
    }
    return text.substring(0, length);
}

function generateTestDocument(index) {
    const name = randomChoice(SAMPLE_NAMES);
    
    return {
        // Simplified but still substantial document
        id: `test-doc-${index}`,
        name: `${name} ${index}`,
        email: `user${index}@example.com`,
        company: randomChoice(SAMPLE_COMPANIES),
        department: randomChoice(SAMPLE_DEPARTMENTS),
        
        // Large text fields to reach target size
        description: generateLargeText(1000),
        notes: generateLargeText(1500),
        documentation: generateLargeText(2000),
        
        // Arrays to add bulk
        projects: Array.from({length: 5}, (_, i) => ({
            name: `Project ${i}`,
            description: generateLargeText(300),
            status: 'active'
        })),
        
        skills: Array.from({length: 10}, (_, i) => `Skill ${i}`),
        tags: Array.from({length: 8}, (_, i) => `tag${i}`),
        
        // Metadata
        createdAt: new Date().toISOString(),
        version: '1.0.0',
        index: index
    };
}

async function makeRequest(url, method = 'GET', data = null) {
    const http = require('http');
    const urlLib = require('url');
    
    return new Promise((resolve, reject) => {
        const parsed = urlLib.parse(url);
        
        const options = {
            hostname: parsed.hostname,
            port: parsed.port || 80,
            path: parsed.path,
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

async function createCollection() {
    console.log(`Creating collection: ${COLLECTION_NAME}`);
    
    try {
        const response = await makeRequest(`${API_URL}/collections`, 'POST', {
            name: COLLECTION_NAME,
            description: 'Large test dataset for performance testing'
        });
        
        if (response.status === 200 || response.status === 201) {
            console.log('✅ Collection created successfully');
        } else if (response.status === 409) {
            console.log('ℹ️  Collection already exists, continuing...');
        } else {
            console.log(`⚠️  Unexpected response: ${response.status}`, response.data);
        }
    } catch (error) {
        console.error('❌ Error creating collection:', error.message);
        throw error;
    }
}

async function insertSingleDocument(doc, retries = 3) {
    for (let i = 0; i < retries; i++) {
        try {
            const response = await makeRequest(`${API_URL}/collections/${COLLECTION_NAME}/documents`, 'POST', doc);
            if (response.status === 200 || response.status === 201) {
                return true;
            }
        } catch (error) {
            if (i === retries - 1) {
                console.error(`Failed to insert document after ${retries} retries:`, error.message);
            }
            await new Promise(resolve => setTimeout(resolve, 1000)); // Wait 1 second before retry
        }
    }
    return false;
}

async function getCollectionSize() {
    try {
        const response = await makeRequest(`${API_URL}/collections/${COLLECTION_NAME}/documents`);
        if (response.status === 200) {
            return response.data.count || 0;
        }
    } catch (error) {
        console.error('Error getting collection size:', error.message);
    }
    return 0;
}

async function main() {
    console.log('🚀 Starting large test data generation...');
    console.log(`Target size: ${TARGET_SIZE_MB}MB`);
    console.log(`Estimated documents needed: ${Math.ceil((TARGET_SIZE_MB * 1024 * 1024) / ESTIMATED_DOC_SIZE)}`);
    
    try {
        // Create collection
        await createCollection();
        
        let totalInserted = 0;
        let estimatedSize = 0;
        const targetBytes = TARGET_SIZE_MB * 1024 * 1024;
        let docIndex = 0;
        
        console.log('\n📝 Generating and inserting documents...');
        
        while (estimatedSize < targetBytes) {
            // Generate smaller batches and process sequentially
            const batchPromises = [];
            const batchStart = docIndex;
            
            for (let i = 0; i < BATCH_SIZE && estimatedSize < targetBytes; i++) {
                const doc = generateTestDocument(docIndex++);
                batchPromises.push(insertSingleDocument(doc));
                estimatedSize += ESTIMATED_DOC_SIZE; // Update estimate immediately
            }
            
            // Wait for all documents in this batch
            const results = await Promise.all(batchPromises);
            const successful = results.filter(r => r).length;
            totalInserted += successful;
            
            const progress = (estimatedSize / targetBytes * 100).toFixed(1);
            console.log(`📊 Batch ${Math.floor(batchStart / BATCH_SIZE) + 1}: ${successful}/${BATCH_SIZE} docs, Total: ${totalInserted}, ~${(estimatedSize / 1024 / 1024).toFixed(1)}MB (${progress}%)`);
            
            // Small delay between batches
            await new Promise(resolve => setTimeout(resolve, 500));
            
            // Check if we should continue
            if (successful < BATCH_SIZE * 0.5) {
                console.log('⚠️  Too many failures, stopping...');
                break;
            }
        }
        
        // Get final count from server
        console.log('\n🔍 Getting final collection statistics...');
        const finalCount = await getCollectionSize();
        
        console.log('\n✅ Test data generation completed!');
        console.log(`📈 Documents inserted: ${totalInserted}`);
        console.log(`📊 Server reported count: ${finalCount} documents`);
        console.log(`💾 Estimated data size: ~${(estimatedSize / 1024 / 1024).toFixed(1)}MB`);
        console.log(`🔗 Collection: ${COLLECTION_NAME}`);
        
    } catch (error) {
        console.error('❌ Error during test data generation:', error);
        process.exit(1);
    }
}

if (require.main === module) {
    main();
}