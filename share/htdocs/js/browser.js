// JSONdb Browser JavaScript - Email Style
const API_BASE_URL = '';
let authToken = localStorage.getItem('jsondb_auth_token');
let currentCollection = null;
let currentDocument = null;
let collections = [];
let documents = [];

// Check authentication
if (!authToken) {
    window.location.href = '/login.html';
}

// Initialize
document.addEventListener('DOMContentLoaded', function() {
    loadCollections();
});

// Logout function
function logout() {
    localStorage.removeItem('jsondb_auth_token');
    localStorage.removeItem('jsondb_refresh_token');
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
    
    try {
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
        
        return await response.json();
    } catch (error) {
        console.error('API Error:', error);
        throw error;
    }
}

// Load collections
async function loadCollections() {
    try {
        const data = await apiRequest('/api/collections');
        collections = data.collections || [];
        renderCollections();
    } catch (error) {
        console.error('Error loading collections:', error);
        document.getElementById('collectionsList').innerHTML = `
            <div class="empty-state">
                <i class="bi bi-exclamation-circle"></i>
                <p>Error loading collections</p>
            </div>
        `;
    }
}

// Render collections
function renderCollections() {
    const container = document.getElementById('collectionsList');
    const searchTerm = document.getElementById('collectionSearch').value.toLowerCase();
    
    const filteredCollections = collections.filter(col => 
        col.toLowerCase().includes(searchTerm)
    );
    
    if (filteredCollections.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <i class="bi bi-folder-x"></i>
                <p>No collections found</p>
            </div>
        `;
        return;
    }
    
    container.innerHTML = filteredCollections.map(collection => `
        <div class="collection-item ${collection === currentCollection ? 'active' : ''}" 
             onclick="selectCollection('${collection}')">
            <div>
                <i class="bi bi-collection icon"></i>
                <span>${collection}</span>
            </div>
            <span class="count" id="count-${collection}">-</span>
        </div>
    `).join('');
    
    // Load document counts for each collection
    filteredCollections.forEach(loadCollectionCount);
}

// Load collection document count
async function loadCollectionCount(collection) {
    try {
        const data = await apiRequest(`/api/collections/${collection}/documents`);
        const count = data.documents ? data.documents.length : 0;
        const countElement = document.getElementById(`count-${collection}`);
        if (countElement) {
            countElement.textContent = count;
        }
    } catch (error) {
        console.error(`Error loading count for ${collection}:`, error);
    }
}

// Select collection
async function selectCollection(collection) {
    currentCollection = collection;
    currentDocument = null;
    
    // Update active state
    document.querySelectorAll('.collection-item').forEach(item => {
        item.classList.remove('active');
    });
    event.currentTarget.classList.add('active');
    
    // Clear document viewer
    document.getElementById('contentViewer').innerHTML = `
        <div class="empty-state">
            <i class="bi bi-file-earmark-text"></i>
            <p>Select a document to view its content</p>
        </div>
    `;
    document.getElementById('documentTitle').textContent = 'No document selected';
    document.getElementById('editBtn').disabled = true;
    document.getElementById('deleteBtn').disabled = true;
    
    // Load documents
    await loadDocuments();
}

// Load documents
async function loadDocuments() {
    if (!currentCollection) return;
    
    const container = document.getElementById('documentsList');
    container.innerHTML = `
        <div class="loading">
            <div class="spinner-border text-primary" role="status">
                <span class="visually-hidden">Loading...</span>
            </div>
        </div>
    `;
    
    try {
        const data = await apiRequest(`/api/collections/${currentCollection}/documents`);
        documents = data.documents || [];
        renderDocuments();
        document.getElementById('documentCount').textContent = documents.length;
    } catch (error) {
        console.error('Error loading documents:', error);
        container.innerHTML = `
            <div class="empty-state">
                <i class="bi bi-exclamation-circle"></i>
                <p>Error loading documents</p>
            </div>
        `;
    }
}

// Render documents
function renderDocuments() {
    const container = document.getElementById('documentsList');
    const searchTerm = document.getElementById('documentSearch').value.toLowerCase();
    
    const filteredDocuments = documents.filter(doc => {
        const searchStr = JSON.stringify(doc).toLowerCase();
        return searchStr.includes(searchTerm);
    });
    
    if (filteredDocuments.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <i class="bi bi-file-x"></i>
                <p>No documents found</p>
            </div>
        `;
        return;
    }
    
    container.innerHTML = filteredDocuments.map(doc => {
        const docId = doc._id || doc.id || 'unknown';
        const title = getDocumentTitle(doc);
        const preview = getDocumentPreview(doc);
        const isActive = currentDocument && currentDocument._id === docId;
        
        return `
            <div class="document-item ${isActive ? 'active' : ''}" 
                 onclick="selectDocument('${docId}')">
                <div class="document-title">
                    <span>${title}</span>
                    <i class="bi bi-chevron-right"></i>
                </div>
                <div class="document-preview">${preview}</div>
                <div class="document-meta">ID: ${docId}</div>
            </div>
        `;
    }).join('');
}

// Get document title
function getDocumentTitle(doc) {
    // Try common title fields
    if (doc.title) return doc.title;
    if (doc.name) return doc.name;
    if (doc.username) return doc.username;
    if (doc.email) return doc.email;
    if (doc.label) return doc.label;
    
    // For special collections
    if (currentCollection === '_users' && doc.username) return doc.username;
    if (currentCollection === '_roles' && doc.name) return doc.name;
    
    // Default to ID
    return doc._id || doc.id || 'Untitled Document';
}

// Get document preview
function getDocumentPreview(doc) {
    const copy = { ...doc };
    delete copy._id;
    delete copy.id;
    
    // Remove the field used as title
    if (doc.title) delete copy.title;
    if (doc.name) delete copy.name;
    if (doc.username) delete copy.username;
    
    const str = JSON.stringify(copy);
    return str.length > 100 ? str.substring(0, 100) + '...' : str;
}

// Select document
async function selectDocument(docId) {
    const doc = documents.find(d => (d._id || d.id) === docId);
    if (!doc) return;
    
    currentDocument = doc;
    
    // Update active state
    document.querySelectorAll('.document-item').forEach(item => {
        item.classList.remove('active');
    });
    event.currentTarget.classList.add('active');
    
    // Update toolbar
    document.getElementById('documentTitle').textContent = getDocumentTitle(doc);
    document.getElementById('editBtn').disabled = false;
    document.getElementById('deleteBtn').disabled = false;
    
    // Display document
    displayDocument(doc);
}

// Display document
function displayDocument(doc) {
    const container = document.getElementById('contentViewer');
    const jsonStr = JSON.stringify(doc, null, 2);
    
    container.innerHTML = `
        <div class="json-viewer">
            <pre><code class="language-json">${escapeHtml(jsonStr)}</code></pre>
        </div>
    `;
    
    // Apply syntax highlighting
    Prism.highlightAll();
}

// Filter collections
function filterCollections() {
    renderCollections();
}

// Filter documents
function filterDocuments() {
    renderDocuments();
}

// Refresh collections
function refreshCollections() {
    loadCollections();
}

// Edit document
function editDocument() {
    if (!currentDocument || !currentCollection) return;
    
    // For now, show alert - could open a modal editor
    alert('Edit functionality coming soon!');
}

// Delete document
async function deleteDocument() {
    if (!currentDocument || !currentCollection) return;
    
    const docId = currentDocument._id || currentDocument.id;
    if (!confirm(`Are you sure you want to delete this document?\n\nID: ${docId}`)) return;
    
    try {
        await apiRequest(`/api/collections/${currentCollection}/documents/${docId}`, {
            method: 'DELETE'
        });
        
        // Reload documents
        await loadDocuments();
        
        // Clear viewer
        currentDocument = null;
        document.getElementById('contentViewer').innerHTML = `
            <div class="empty-state">
                <i class="bi bi-file-earmark-text"></i>
                <p>Select a document to view its content</p>
            </div>
        `;
        document.getElementById('documentTitle').textContent = 'No document selected';
        document.getElementById('editBtn').disabled = true;
        document.getElementById('deleteBtn').disabled = true;
        
    } catch (error) {
        console.error('Error deleting document:', error);
        alert('Error deleting document: ' + error.message);
    }
}

// Copy to clipboard
function copyToClipboard() {
    if (!currentDocument) return;
    
    const jsonStr = JSON.stringify(currentDocument, null, 2);
    navigator.clipboard.writeText(jsonStr).then(() => {
        // Show temporary success message
        const btn = event.currentTarget;
        const originalHtml = btn.innerHTML;
        btn.innerHTML = '<i class="bi bi-check"></i> Copied!';
        setTimeout(() => {
            btn.innerHTML = originalHtml;
        }, 2000);
    }).catch(err => {
        console.error('Error copying to clipboard:', err);
        alert('Failed to copy to clipboard');
    });
}

// Escape HTML
function escapeHtml(text) {
    const map = {
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#039;'
    };
    return text.replace(/[&<>"']/g, m => map[m]);
}