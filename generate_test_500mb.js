#!/usr/bin/env node

// Generate 500MB test data in a new collection
const http = require('http');
const crypto = require('crypto');

const API_URL = 'http://localhost:5000/api';
const COLLECTION_NAME = 'test_500mb';
const TARGET_SIZE_MB = 500;
const DOC_SIZE_KB = 50; // 50KB documents for good balance

// Generate random but valid content
function generateContent(sizeKB) {
    const words = ['lorem', 'ipsum', 'dolor', 'sit', 'amet', 'consectetur', 'adipiscing', 
                   'elit', 'sed', 'do', 'eiusmod', 'tempor', 'incididunt', 'ut', 'labore'];
    const targetChars = sizeKB * 1024;
    let content = '';
    
    while (content.length < targetChars) {
        const sentence = [];
        const sentenceLength = 10 + Math.floor(Math.random() * 10);
        for (let i = 0; i < sentenceLength; i++) {
            sentence.push(words[Math.floor(Math.random() * words.length)]);
        }
        content += sentence.join(' ') + '. ';
    }
    
    return content.substring(0, targetChars);
}

function generateDocument(index) {
    const now = new Date();
    const id = crypto.randomBytes(8).toString('hex');
    
    return {
        title: `Test Document ${index}`,
        documentId: `doc-${index}-${id}`,
        type: 'performance_test',
        index: index,
        
        // Realistic fields
        author: {
            name: `User ${index % 100}`,
            email: `user${index % 100}@example.com`,
            department: ['Engineering', 'Sales', 'Marketing', 'Support'][index % 4]
        },
        
        // Large content fields
        content: generateContent(20),
        summary: generateContent(5),
        notes: generateContent(10),
        metadata: generateContent(5),
        
        // Structured data
        tags: [`batch${Math.floor(index/100)}`, 'test', 'performance', `v${index % 5}`],
        
        statistics: {
            views: Math.floor(Math.random() * 1000),
            likes: Math.floor(Math.random() * 100),
            shares: Math.floor(Math.random() * 50),
            comments: Math.floor(Math.random() * 20)
        },
        
        timestamps: {
            created: now.toISOString(),
            modified: now.toISOString(),
            version: 1
        },
        
        // Additional fields
        category: ['Technical', 'Business', 'General', 'Archive'][index % 4],
        priority: ['Low', 'Medium', 'High', 'Critical'][index % 4],
        status: 'active',
        
        relatedDocs: Array.from({length: 3}, (_, i) => `doc-${Math.max(0, index - i - 1)}`),
        
        settings: {
            public: true,
            searchable: true,
            archived: false
        }
    };
}

async function request(url, method = 'GET', data = null) {
    return new Promise((resolve, reject) => {
        const urlObj = new URL(url);
        const options = {
            hostname: urlObj.hostname,
            port: urlObj.port || 80,
            path: urlObj.pathname,
            method: method,
            headers: { 'Content-Type': 'application/json' },
            timeout: 30000
        };
        
        let body = '';
        const req = http.request(options, (res) => {
            res.on('data', chunk => body += chunk);
            res.on('end', () => {
                try {
                    resolve({ status: res.statusCode, data: JSON.parse(body) });
                } catch (e) {
                    resolve({ status: res.statusCode, data: body });
                }
            });
        });
        
        req.on('timeout', () => req.destroy());
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
        const res = await request(`${API_URL}/collections`, 'POST', {
            name: COLLECTION_NAME,
            description: '500MB test dataset'
        });
        
        if (res.status === 201 || res.status === 200) {
            console.log('✅ Collection created');
            return true;
        } else if (res.status === 409 || (res.data && res.data.error && res.data.error.includes('exists'))) {
            console.log('ℹ️  Collection exists');
            return true;
        } else if (res.status === 400 && res.data && res.data.error === 'Invalid request') {
            // Collection might already exist, continue
            console.log('ℹ️  Collection may already exist, continuing...');
            return true;
        }
        
        console.error('❌ Failed to create collection:', res.data);
        return false;
    } catch (e) {
        console.error('❌ Error creating collection:', e.message);
        return false;
    }
}

async function insertDocuments(documents) {
    let successful = 0;
    
    for (const doc of documents) {
        try {
            const res = await request(`${API_URL}/collections/${COLLECTION_NAME}/documents`, 'POST', doc);
            if (res.status === 200 || res.status === 201) {
                successful++;
            }
        } catch (e) {
            // Continue on error
        }
    }
    
    return successful;
}

async function getStats() {
    try {
        const res = await request(`${API_URL}/collections`);
        if (res.status === 200) {
            const col = res.data.collections.find(c => c.name === COLLECTION_NAME);
            return col ? col.documentCount : 0;
        }
    } catch (e) {}
    return 0;
}

async function main() {
    console.log('🚀 500MB Test Data Generator');
    console.log(`📊 Target: ${TARGET_SIZE_MB}MB`);
    console.log(`📄 Document size: ~${DOC_SIZE_KB}KB`);
    
    // Create collection
    if (!await createCollection()) {
        return;
    }
    
    // Check existing documents
    const existing = await getStats();
    console.log(`📈 Existing documents: ${existing}`);
    
    const docsPerMB = Math.floor(1024 / DOC_SIZE_KB);
    const totalNeeded = TARGET_SIZE_MB * docsPerMB;
    const toAdd = Math.max(0, totalNeeded - existing);
    
    if (toAdd === 0) {
        console.log('✅ Target already reached!');
        return;
    }
    
    console.log(`📝 Documents to add: ${toAdd}\n`);
    
    const batchSize = 10;
    let total = existing;
    let added = 0;
    const startTime = Date.now();
    
    for (let i = 0; i < toAdd; i += batchSize) {
        const batch = [];
        const count = Math.min(batchSize, toAdd - i);
        
        for (let j = 0; j < count; j++) {
            batch.push(generateDocument(total + j));
        }
        
        process.stdout.write(`Batch ${Math.floor(i/batchSize) + 1}: `);
        const inserted = await insertDocuments(batch);
        added += inserted;
        total += inserted;
        
        const sizeMB = (total * DOC_SIZE_KB / 1024).toFixed(1);
        const progress = (sizeMB / TARGET_SIZE_MB * 100).toFixed(1);
        
        console.log(`${inserted}/${count} docs | Total: ${total} (~${sizeMB}MB, ${progress}%)`);
        
        // Stop if failing
        if (inserted === 0 && i > 0) {
            console.log('⚠️  Stopping due to failures');
            break;
        }
        
        // Small delay
        await new Promise(r => setTimeout(r, 200));
    }
    
    const duration = ((Date.now() - startTime) / 1000).toFixed(1);
    const finalSize = (total * DOC_SIZE_KB / 1024).toFixed(1);
    
    console.log('\n' + '='.repeat(50));
    console.log(`✅ Completed in ${duration}s`);
    console.log(`📈 Documents added: ${added}`);
    console.log(`📊 Total documents: ${total}`);
    console.log(`💾 Estimated size: ~${finalSize}MB`);
    console.log(`🎯 Target reached: ${finalSize >= TARGET_SIZE_MB ? 'YES' : 'NO'}`);
}

main().catch(console.error);