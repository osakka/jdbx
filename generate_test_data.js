#!/usr/bin/env node

// Generate large test dataset for JSONdb performance testing
// Target: ~500MB of test documents

const fs = require('fs');
const crypto = require('crypto');

// Configuration
const API_URL = 'http://localhost:5000/api';
const COLLECTION_NAME = 'large_test_data';
const TARGET_SIZE_MB = 500;
const BATCH_SIZE = 100; // Documents per batch
const ESTIMATED_DOC_SIZE = 5000; // Bytes per document (approx)

// Sample data generators
const SAMPLE_NAMES = [
    'Alice Johnson', 'Bob Smith', 'Carol Williams', 'David Brown', 'Emma Davis',
    'Frank Miller', 'Grace Wilson', 'Henry Moore', 'Ivy Taylor', 'Jack Anderson',
    'Kate Thomas', 'Luke Jackson', 'Mia White', 'Noah Harris', 'Olivia Martin',
    'Paul Thompson', 'Quinn Garcia', 'Ruby Martinez', 'Sam Robinson', 'Tina Clark'
];

const SAMPLE_COMPANIES = [
    'TechCorp Inc', 'DataSoft LLC', 'CloudVision Systems', 'InnovateTech', 'DigitalSolutions',
    'FutureSoft', 'NextGen Analytics', 'SmartData Corp', 'TechFlow Industries', 'DataStream Inc',
    'CyberTech Solutions', 'CloudFirst LLC', 'DevOps Systems', 'AI Dynamics', 'BigData Corp'
];

const SAMPLE_DEPARTMENTS = [
    'Engineering', 'Sales', 'Marketing', 'HR', 'Finance', 'Operations', 'Support', 'Research'
];

const SAMPLE_SKILLS = [
    'JavaScript', 'Python', 'Java', 'C++', 'React', 'Node.js', 'Docker', 'Kubernetes',
    'AWS', 'Azure', 'MongoDB', 'PostgreSQL', 'Redis', 'Elasticsearch', 'Machine Learning',
    'Data Science', 'DevOps', 'Microservices', 'GraphQL', 'TypeScript'
];

// Generate random data
function randomChoice(array) {
    return array[Math.floor(Math.random() * array.length)];
}

function randomChoices(array, count) {
    const shuffled = [...array].sort(() => 0.5 - Math.random());
    return shuffled.slice(0, count);
}

function generateLargeText(minLength = 1000, maxLength = 3000) {
    const words = [
        'lorem', 'ipsum', 'dolor', 'sit', 'amet', 'consectetur', 'adipiscing', 'elit',
        'sed', 'do', 'eiusmod', 'tempor', 'incididunt', 'ut', 'labore', 'et', 'dolore',
        'magna', 'aliqua', 'enim', 'ad', 'minim', 'veniam', 'quis', 'nostrud',
        'exercitation', 'ullamco', 'laboris', 'nisi', 'aliquip', 'ex', 'ea', 'commodo',
        'consequat', 'duis', 'aute', 'irure', 'in', 'reprehenderit', 'voluptate',
        'velit', 'esse', 'cillum', 'fugiat', 'nulla', 'pariatur', 'excepteur', 'sint',
        'occaecat', 'cupidatat', 'non', 'proident', 'sunt', 'culpa', 'qui', 'officia',
        'deserunt', 'mollit', 'anim', 'id', 'est', 'laborum'
    ];
    
    const length = Math.floor(Math.random() * (maxLength - minLength)) + minLength;
    let text = '';
    
    while (text.length < length) {
        const sentence = [];
        const sentenceLength = Math.floor(Math.random() * 15) + 5;
        
        for (let i = 0; i < sentenceLength; i++) {
            sentence.push(randomChoice(words));
        }
        
        text += sentence.join(' ') + '. ';
    }
    
    return text.trim();
}

function generateTestDocument() {
    const name = randomChoice(SAMPLE_NAMES);
    const [firstName, lastName] = name.split(' ');
    
    return {
        // Personal Information
        firstName,
        lastName,
        fullName: name,
        email: `${firstName.toLowerCase()}.${lastName.toLowerCase()}@${randomChoice(SAMPLE_COMPANIES).toLowerCase().replace(/\s+/g, '')}.com`,
        phone: `+1-${Math.floor(Math.random() * 900) + 100}-${Math.floor(Math.random() * 900) + 100}-${Math.floor(Math.random() * 9000) + 1000}`,
        dateOfBirth: new Date(1970 + Math.floor(Math.random() * 35), Math.floor(Math.random() * 12), Math.floor(Math.random() * 28) + 1).toISOString(),
        
        // Employment Information
        company: randomChoice(SAMPLE_COMPANIES),
        department: randomChoice(SAMPLE_DEPARTMENTS),
        jobTitle: `${randomChoice(['Senior', 'Lead', 'Principal', 'Staff', 'Junior'])} ${randomChoice(['Developer', 'Engineer', 'Analyst', 'Manager', 'Specialist'])}`,
        salary: Math.floor(Math.random() * 200000) + 50000,
        startDate: new Date(2015 + Math.floor(Math.random() * 10), Math.floor(Math.random() * 12), Math.floor(Math.random() * 28) + 1).toISOString(),
        
        // Skills and Experience
        skills: randomChoices(SAMPLE_SKILLS, Math.floor(Math.random() * 8) + 3),
        experienceYears: Math.floor(Math.random() * 20) + 1,
        certifications: Array.from({length: Math.floor(Math.random() * 5)}, () => 
            `${randomChoice(SAMPLE_SKILLS)} Certification ${Math.floor(Math.random() * 3) + 1}`
        ),
        
        // Performance Data
        performanceRatings: Array.from({length: Math.floor(Math.random() * 5) + 1}, () => ({
            year: 2020 + Math.floor(Math.random() * 5),
            rating: Math.floor(Math.random() * 5) + 1,
            feedback: generateLargeText(200, 500)
        })),
        
        // Project History
        projects: Array.from({length: Math.floor(Math.random() * 10) + 2}, (_, i) => ({
            name: `Project ${String.fromCharCode(65 + i)} - ${randomChoice(['Migration', 'Development', 'Analysis', 'Implementation', 'Optimization'])}`,
            description: generateLargeText(300, 800),
            status: randomChoice(['Completed', 'In Progress', 'On Hold', 'Cancelled']),
            startDate: new Date(2020 + Math.floor(Math.random() * 5), Math.floor(Math.random() * 12), Math.floor(Math.random() * 28) + 1).toISOString(),
            technologies: randomChoices(SAMPLE_SKILLS, Math.floor(Math.random() * 5) + 2),
            teamSize: Math.floor(Math.random() * 15) + 3,
            budget: Math.floor(Math.random() * 1000000) + 50000
        })),
        
        // Notes and Comments
        notes: generateLargeText(500, 1500),
        comments: Array.from({length: Math.floor(Math.random() * 8) + 2}, () => ({
            date: new Date(Date.now() - Math.floor(Math.random() * 365 * 24 * 60 * 60 * 1000)).toISOString(),
            author: randomChoice(SAMPLE_NAMES),
            content: generateLargeText(100, 400)
        })),
        
        // Metadata
        id: crypto.randomBytes(16).toString('hex'),
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: '1.0.0',
        tags: randomChoices(['important', 'urgent', 'review', 'archived', 'active', 'training', 'promotion'], Math.floor(Math.random() * 4) + 1),
        
        // Additional large text fields for size
        documentation: generateLargeText(800, 2000),
        technicalNotes: generateLargeText(400, 1000),
        requirements: generateLargeText(300, 800)
    };
}

async function makeRequest(url, method = 'GET', data = null) {
    const https = require('https');
    const http = require('http');
    const urlLib = require('url');
    
    return new Promise((resolve, reject) => {
        const parsed = urlLib.parse(url);
        const isHttps = parsed.protocol === 'https:';
        const lib = isHttps ? https : http;
        
        const options = {
            hostname: parsed.hostname,
            port: parsed.port || (isHttps ? 443 : 80),
            path: parsed.path,
            method: method,
            headers: {
                'Content-Type': 'application/json'
            }
        };
        
        if (data) {
            const jsonData = JSON.stringify(data);
            options.headers['Content-Length'] = Buffer.byteLength(jsonData);
        }
        
        const req = lib.request(options, (res) => {
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

async function insertBatch(documents) {
    const promises = documents.map(async (doc) => {
        try {
            const response = await makeRequest(`${API_URL}/collections/${COLLECTION_NAME}/documents`, 'POST', doc);
            return response.status === 200 || response.status === 201;
        } catch (error) {
            console.error('Error inserting document:', error.message);
            return false;
        }
    });
    
    const results = await Promise.all(promises);
    return results.filter(r => r).length;
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
        
        console.log('\n📝 Generating and inserting documents...');
        
        while (estimatedSize < targetBytes) {
            // Generate batch of documents
            const documents = Array.from({length: BATCH_SIZE}, generateTestDocument);
            
            // Insert batch
            const inserted = await insertBatch(documents);
            totalInserted += inserted;
            
            // Estimate size (rough calculation)
            estimatedSize = totalInserted * ESTIMATED_DOC_SIZE;
            const progress = (estimatedSize / targetBytes * 100).toFixed(1);
            
            console.log(`📊 Progress: ${totalInserted} docs, ~${(estimatedSize / 1024 / 1024).toFixed(1)}MB (${progress}%)`);
            
            // Small delay to avoid overwhelming the server
            await new Promise(resolve => setTimeout(resolve, 100));
        }
        
        // Get final count
        const finalCount = await getCollectionSize();
        
        console.log('\n✅ Test data generation completed!');
        console.log(`📈 Documents inserted: ${totalInserted}`);
        console.log(`📊 Final collection size: ${finalCount} documents`);
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