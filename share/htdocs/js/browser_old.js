// Document Browser JavaScript
const API_BASE_URL = '';
let authToken = localStorage.getItem('jsondb_auth_token');
let currentCollection = null;
let currentDocument = null;
let allCollections = [];
let allDocuments = [];
let searchTimeout = null;

// Check authentication
if (!authToken) {
    window.location.href = '/login.html';
}

// Initialize
document.addEventListener('DOMContentLoaded', function() {
    loadCollections();
    setupEventListeners();
});

// Setup event listeners
function setupEventListeners() {
    // Collection search
    document.getElementById('collectionSearch').addEventListener('input', filterCollections);
    
    // Global search
    document.getElementById('globalSearch').addEventListener('input', function(e) {
        clearTimeout(searchTimeout);
        searchTimeout = setTimeout(() => performGlobalSearch(e.target.value), 300);
    });
    
    // Filter tags
    document.querySelectorAll('.filter-tag').forEach(tag => {
        tag.addEventListener('click', function() {
            document.querySelectorAll('.filter-tag').forEach(t => t.classList.remove('active'));
            this.classList.add('active');
            filterDocuments(this.dataset.filter);
        });
    });
    
    // Click outside search results to close
    document.addEventListener('click', function(e) {
        if (!e.target.closest('.search-box')) {
            document.getElementById('searchResults').style.display = 'none';
        }
    });
}

// Logout function
function logout() {
    localStorage.removeItem('jsondb_auth_token');
    window.location.href = '/login.html';
}

// API helper
async function apiRequest(endpoint, options = {}) {
    const defaultOptions = {
        headers: {
            'Authorization': `Bearer ${authToken}`,
            'Content-Type': 'application/json'
        }
    };
    
    const response = await fetch(`${API_BASE_URL}${endpoint}`, {
        ...defaultOptions,
        ...options,
        headers: {
            ...defaultOptions.headers,
            ...options.headers
        }
    });
    
    if (!response.ok) {
        if (response.status === 401) {
            // Token expired or invalid
            localStorage.removeItem('jsondb_auth_token');
            localStorage.removeItem('jsondb_refresh_token');
            window.location.href = '/login.html';
            return;
        }
        const error = await response.json();
        throw new Error(error.error || 'API request failed');
    }
    
    return response.json();
}

// Load collections
async function loadCollections() {
    try {
        const response = await apiRequest('/api/collections');
        allCollections = response.collections || [];
        displayCollections(allCollections);
    } catch (error) {
        console.error('Error loading collections:', error);
    }
}

// Display collections
function displayCollections(collections) {
    const container = document.getElementById('collectionsList');
    container.innerHTML = '';
    
    collections.forEach(async (collection) => {
        const div = document.createElement('div');
        div.className = 'collection-item';
        div.onclick = () => selectCollection(collection);
        
        // Get document count
        try {
            const docsResponse = await apiRequest(`/api/collections/${collection}/documents`);
            const count = docsResponse.documents ? docsResponse.documents.length : 0;
            
            div.innerHTML = `
                <div class="collection-name">${collection}</div>
                <div class="collection-count">${count} documents</div>
            `;
        } catch (error) {
            div.innerHTML = `
                <div class="collection-name">${collection}</div>
                <div class="collection-count">? documents</div>
            `;
        }
        
        container.appendChild(div);
    });
}

// Filter collections
function filterCollections() {
    const search = document.getElementById('collectionSearch').value.toLowerCase();
    const filtered = allCollections.filter(col => col.toLowerCase().includes(search));
    displayCollections(filtered);
}

// Select collection
async function selectCollection(collection) {
    currentCollection = collection;
    
    // Update UI
    document.querySelectorAll('.collection-item').forEach(item => {
        item.classList.remove('active');
        if (item.querySelector('.collection-name').textContent === collection) {
            item.classList.add('active');
        }
    });
    
    document.getElementById('selectedCollection').textContent = collection;
    
    // Load documents
    try {
        const response = await apiRequest(`/api/collections/${collection}/documents`);
        allDocuments = response.documents || [];
        displayDocuments(allDocuments);
        updateCollectionStats();
    } catch (error) {
        console.error('Error loading documents:', error);
        allDocuments = [];
        displayDocuments([]);
    }
}

// Display documents
function displayDocuments(documents) {
    const container = document.getElementById('documentList');
    
    if (documents.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <i class="bi bi-file-earmark"></i>
                <p>No documents in this collection</p>
            </div>
        `;
        return;
    }
    
    container.innerHTML = '';
    documents.forEach(doc => {
        const div = document.createElement('div');
        div.className = 'document-item';
        div.onclick = () => selectDocument(doc);
        
        // Create a preview of the document
        const preview = getDocumentPreview(doc);
        div.innerHTML = `
            <div style="font-weight: 500;">${doc._id || 'Unnamed Document'}</div>
            <div style="font-size: 0.875rem; color: #6c757d;">${preview}</div>
        `;
        
        container.appendChild(div);
    });
}

// Get document preview
function getDocumentPreview(doc) {
    const keys = Object.keys(doc).filter(k => k !== '_id');
    if (keys.length === 0) return 'Empty document';
    
    const preview = keys.slice(0, 3).map(key => {
        const value = doc[key];
        const valueStr = typeof value === 'object' ? JSON.stringify(value) : String(value);
        return `${key}: ${valueStr.length > 30 ? valueStr.substring(0, 30) + '...' : valueStr}`;
    }).join(', ');
    
    return keys.length > 3 ? preview + ', ...' : preview;
}

// Select document
function selectDocument(doc) {
    currentDocument = doc;
    
    // Update UI
    document.querySelectorAll('.document-item').forEach(item => {
        item.classList.remove('active');
        if (item.querySelector('div').textContent.includes(doc._id)) {
            item.classList.add('active');
        }
    });
    
    // Display document
    document.getElementById('documentTitle').textContent = doc._id || 'Document';
    document.getElementById('documentContent').textContent = JSON.stringify(doc, null, 2);
    
    // Re-highlight
    Prism.highlightElement(document.getElementById('documentContent'));
}

// Filter documents by type
function filterDocuments(filter) {
    if (!allDocuments) return;
    
    let filtered = allDocuments;
    
    if (filter !== 'all') {
        filtered = allDocuments.filter(doc => {
            // Check if document contains fields of the specified type
            return Object.values(doc).some(value => {
                const type = Array.isArray(value) ? 'array' : typeof value;
                return type === filter;
            });
        });
    }
    
    displayDocuments(filtered);
}

// Update collection stats
function updateCollectionStats() {
    const stats = document.getElementById('collectionStats');
    
    if (!allDocuments || allDocuments.length === 0) {
        stats.innerHTML = '';
        return;
    }
    
    // Calculate stats
    const totalSize = JSON.stringify(allDocuments).length;
    const avgSize = Math.round(totalSize / allDocuments.length);
    
    stats.innerHTML = `
        <span class="stats-badge">${allDocuments.length} docs</span>
        <span class="stats-badge">${formatSize(totalSize)} total</span>
        <span class="stats-badge">${formatSize(avgSize)} avg</span>
    `;
}

// Format size
function formatSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

// Global search
async function performGlobalSearch(query) {
    if (!query.trim()) {
        document.getElementById('searchResults').style.display = 'none';
        return;
    }
    
    const resultsContainer = document.getElementById('searchResults');
    resultsContainer.innerHTML = '<div class="p-3 text-center">Searching...</div>';
    resultsContainer.style.display = 'block';
    
    try {
        const results = [];
        
        // Search through all collections
        for (const collection of allCollections) {
            try {
                const response = await apiRequest(`/api/collections/${collection}/documents`);
                const documents = response.documents || [];
                
                documents.forEach(doc => {
                    const docStr = JSON.stringify(doc).toLowerCase();
                    if (docStr.includes(query.toLowerCase())) {
                        results.push({
                            collection,
                            document: doc,
                            matches: findMatches(doc, query)
                        });
                    }
                });
            } catch (error) {
                console.error(`Error searching collection ${collection}:`, error);
            }
        }
        
        displaySearchResults(results);
    } catch (error) {
        console.error('Error performing search:', error);
        resultsContainer.innerHTML = '<div class="p-3 text-center text-danger">Search failed</div>';
    }
}

// Find matches in document
function findMatches(doc, query) {
    const matches = [];
    const searchInObject = (obj, path = '') => {
        Object.entries(obj).forEach(([key, value]) => {
            const currentPath = path ? `${path}.${key}` : key;
            
            if (typeof value === 'string' && value.toLowerCase().includes(query.toLowerCase())) {
                matches.push({ path: currentPath, value });
            } else if (typeof value === 'object' && value !== null) {
                searchInObject(value, currentPath);
            }
        });
    };
    
    searchInObject(doc);
    return matches;
}

// Display search results
function displaySearchResults(results) {
    const container = document.getElementById('searchResults');
    
    if (results.length === 0) {
        container.innerHTML = '<div class="p-3 text-center text-muted">No results found</div>';
        return;
    }
    
    container.innerHTML = '';
    results.slice(0, 10).forEach(result => {
        const div = document.createElement('div');
        div.className = 'search-result-item';
        div.onclick = () => {
            selectCollection(result.collection);
            selectDocument(result.document);
            document.getElementById('searchResults').style.display = 'none';
            document.getElementById('globalSearch').value = '';
        };
        
        const matchText = result.matches[0] ? 
            `${result.matches[0].path}: ${highlightMatch(result.matches[0].value, document.getElementById('globalSearch').value)}` : 
            'Match found';
        
        div.innerHTML = `
            <div class="search-result-collection">${result.collection} / ${result.document._id || 'Document'}</div>
            <div class="search-result-match">${matchText}</div>
        `;
        
        container.appendChild(div);
    });
    
    if (results.length > 10) {
        container.innerHTML += `<div class="p-2 text-center text-muted">...and ${results.length - 10} more results</div>`;
    }
}

// Highlight match
function highlightMatch(text, query) {
    const regex = new RegExp(`(${query})`, 'gi');
    return text.replace(regex, '<span class="highlight">$1</span>');
}

// Document actions
function copyDocument() {
    if (!currentDocument) return;
    
    navigator.clipboard.writeText(JSON.stringify(currentDocument, null, 2))
        .then(() => alert('Document copied to clipboard!'))
        .catch(err => console.error('Failed to copy:', err));
}

function downloadDocument() {
    if (!currentDocument) return;
    
    const blob = new Blob([JSON.stringify(currentDocument, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${currentCollection}_${currentDocument._id || 'document'}.json`;
    a.click();
    URL.revokeObjectURL(url);
}

function editDocument() {
    if (!currentDocument) return;
    
    document.getElementById('documentEditor').value = JSON.stringify(currentDocument, null, 2);
    new bootstrap.Modal(document.getElementById('editModal')).show();
}

async function saveDocument() {
    try {
        const updatedDoc = JSON.parse(document.getElementById('documentEditor').value);
        
        await apiRequest(`/api/collections/${currentCollection}/documents/${currentDocument._id}`, {
            method: 'PUT',
            body: JSON.stringify(updatedDoc)
        });
        
        bootstrap.Modal.getInstance(document.getElementById('editModal')).hide();
        
        // Reload documents
        selectCollection(currentCollection);
        
        alert('Document updated successfully!');
    } catch (error) {
        alert('Error saving document: ' + error.message);
    }
}

async function deleteDocument() {
    if (!currentDocument || !confirm('Are you sure you want to delete this document?')) return;
    
    try {
        await apiRequest(`/api/collections/${currentCollection}/documents/${currentDocument._id}`, {
            method: 'DELETE'
        });
        
        // Clear viewer
        currentDocument = null;
        document.getElementById('documentTitle').textContent = 'No document selected';
        document.getElementById('documentContent').textContent = '{\n  "message": "Document deleted"\n}';
        Prism.highlightElement(document.getElementById('documentContent'));
        
        // Reload documents
        selectCollection(currentCollection);
        
        alert('Document deleted successfully!');
    } catch (error) {
        alert('Error deleting document: ' + error.message);
    }
}