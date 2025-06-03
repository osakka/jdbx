#!/usr/bin/env node

// Generate large documents to reach 500MB total
const http = require('http');

const API_URL = 'http://localhost:5000/api';
const COLLECTION_NAME = 'large_test_data';
const TARGET_SIZE_MB = 500;
const DOC_SIZE_MB = 1; // 1MB per document for faster generation
const DOC_SIZE_BYTES = DOC_SIZE_MB * 1024 * 1024;

// Generate a large string of specified size
function generateLargeString(sizeInBytes) {
    // Use a more efficient approach - create chunks and repeat
    const chunkSize = 1024; // 1KB chunks
    const chunk = 'A'.repeat(chunkSize);
    const numChunks = Math.floor(sizeInBytes / chunkSize);
    const remainder = sizeInBytes % chunkSize;
    
    let result = '';
    for (let i = 0; i < numChunks; i++) {
        result += chunk;
    }
    if (remainder > 0) {
        result += 'B'.repeat(remainder);
    }
    
    return result;
}

function generateLargeDocument(index) {
    // Calculate sizes for different fields to reach target document size
    const overhead = 2000; // Estimated JSON structure overhead
    const mainContentSize = Math.floor((DOC_SIZE_BYTES - overhead) * 0.7);
    const secondaryContentSize = Math.floor((DOC_SIZE_BYTES - overhead) * 0.2);
    const tertiaryContentSize = Math.floor((DOC_SIZE_BYTES - overhead) * 0.1);
    
    return {
        _id: `large-doc-${Date.now()}-${index}`,
        docIndex: index,
        type: 'performance_test',
        
        // Large content fields
        mainContent: generateLargeString(mainContentSize),
        secondaryContent: generateLargeString(secondaryContentSize),
        tertiaryContent: generateLargeString(tertiaryContentSize),
        
        // Metadata
        metadata: {
            createdAt: new Date().toISOString(),
            sizeBytes: DOC_SIZE_BYTES,
            testRun: 'large_data_500mb',
            serverInfo: {
                target: '500MB test dataset',
                docSize: `${DOC_SIZE_MB}MB`
            }
        },
        
        // Some structured data
        tags: ['performance', 'test', 'large', `batch${Math.floor(index / 10)}`],
        stats: {
            index: index,
            random: Math.random(),
            timestamp: Date.now()
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
            timeout: 60000 // 60 second timeout for large documents
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
                    resolve({ status: res.statusCode, data: { error: 'Parse error', raw: body.substring(0, 200) } });
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

async function getCollectionInfo() {
    try {
        const response = await makeRequest(`${API_URL}/collections/${COLLECTION_NAME}/documents?limit=1`);
        if (response.status === 200) {
            return {
                exists: true,
                count: response.data.count || 0
            };
        }
    } catch (error) {
        console.log('Collection not found, will be created');
    }
    return { exists: false, count: 0 };
}

async function createCollection() {
    try {
        const response = await makeRequest(`${API_URL}/collections`, 'POST', {
            name: COLLECTION_NAME,
            description: 'Large document test collection (500MB target)'
        });
        
        if (response.status === 200 || response.status === 201) {
            console.log('✅ Collection created successfully');
            return true;
        } else if (response.status === 409) {
            console.log('ℹ️  Collection already exists');
            return true;
        }
    } catch (error) {
        console.error('❌ Error creating collection:', error.message);
    }
    return false;
}

async function insertDocument(doc, retries = 3) {
    for (let i = 0; i < retries; i++) {
        try {
            console.log(`  → Inserting document ${doc.docIndex} (${DOC_SIZE_MB}MB)...`);
            const response = await makeRequest(`${API_URL}/collections/${COLLECTION_NAME}/documents`, 'POST', doc);
            
            if (response.status === 200 || response.status === 201) {
                console.log(`  ✓ Document ${doc.docIndex} inserted successfully`);
                return true;
            } else {
                console.log(`  ✗ Failed to insert document ${doc.docIndex}: ${response.status}`, response.data);
            }
        } catch (error) {
            console.error(`  ✗ Error inserting document ${doc.docIndex} (attempt ${i + 1}/${retries}):`, error.message);
            if (i < retries - 1) {
                await new Promise(resolve => setTimeout(resolve, 2000));
            }
        }
    }
    return false;
}

async function main() {
    console.log('🚀 Large Document Generation for JSONdb');
    console.log(`📊 Target: ${TARGET_SIZE_MB}MB total`);
    console.log(`📄 Document size: ${DOC_SIZE_MB}MB each`);
    console.log(`📦 Documents needed: ~${Math.ceil(TARGET_SIZE_MB / DOC_SIZE_MB)}`);
    console.log('');
    
    // Check existing collection
    const collectionInfo = await getCollectionInfo();
    let existingDocs = 0;
    
    if (collectionInfo.exists) {
        existingDocs = collectionInfo.count;
        console.log(`ℹ️  Collection exists with ${existingDocs} documents`);
        console.log(`📊 Estimated existing data: ~${existingDocs * DOC_SIZE_MB}MB`);
    } else {
        console.log('📦 Creating new collection...');
        if (!await createCollection()) {
            console.error('❌ Failed to create collection');
            process.exit(1);
        }
    }
    
    // Calculate how many more documents we need
    const totalDocsNeeded = Math.ceil(TARGET_SIZE_MB / DOC_SIZE_MB);
    const docsToAdd = Math.max(0, totalDocsNeeded - existingDocs);
    
    if (docsToAdd === 0) {
        console.log('✅ Target size already reached!');
        return;
    }
    
    console.log(`\n📝 Need to add ${docsToAdd} more documents to reach target`);
    console.log('🔄 Starting document generation...\n');
    
    let successCount = 0;
    let failCount = 0;
    const startTime = Date.now();
    
    for (let i = 0; i < docsToAdd; i++) {
        const docIndex = existingDocs + i;
        const doc = generateLargeDocument(docIndex);
        
        if (await insertDocument(doc)) {
            successCount++;
        } else {
            failCount++;
            console.log(`⚠️  Failed to insert document ${docIndex}, continuing...`);
        }
        
        // Progress update
        const totalDocs = existingDocs + successCount;
        const estimatedMB = totalDocs * DOC_SIZE_MB;
        const progress = (estimatedMB / TARGET_SIZE_MB * 100).toFixed(1);
        
        console.log(`📊 Progress: ${totalDocs} docs, ~${estimatedMB}MB (${progress}%)\n`);
        
        // Add delay between documents to avoid overwhelming the server
        await new Promise(resolve => setTimeout(resolve, 1000));
        
        // Stop if too many failures
        if (failCount > 5) {
            console.log('❌ Too many failures, stopping...');
            break;
        }
    }
    
    // Final statistics
    const endTime = Date.now();
    const duration = ((endTime - startTime) / 1000).toFixed(1);
    const finalCount = existingDocs + successCount;
    const finalSizeMB = finalCount * DOC_SIZE_MB;
    
    console.log('\n' + '='.repeat(50));
    console.log('✅ Document generation completed!');
    console.log(`⏱️  Duration: ${duration} seconds`);
    console.log(`📈 Documents added: ${successCount}`);
    console.log(`📊 Total documents: ${finalCount}`);
    console.log(`💾 Estimated size: ~${finalSizeMB}MB`);
    console.log(`🎯 Target reached: ${finalSizeMB >= TARGET_SIZE_MB ? 'YES' : 'NO'}`);
    
    if (failCount > 0) {
        console.log(`⚠️  Failed insertions: ${failCount}`);
    }
}

if (require.main === module) {
    main().catch(console.error);
}