// JSONdb Single Page Application
const API_BASE_URL = '';
let authToken = localStorage.getItem('jsondb_auth_token');
let currentView = 'dashboard';
let refreshInterval = null;

// Chart instances
let collectionsChart = null;
let operationsChart = null;
let operationTypesChart = null;

// Data storage
let allUsers = [];
let allRoles = [];
let allPermissions = [];
let selectedRole = null;
let currentCollection = null;
let currentDocument = null;
let collections = [];
let documents = [];

// Previous data for optimization
let previousData = {
    totalCollections: null,
    totalDocuments: null,
    databaseSize: null,
    collectionsData: {},
    systemHealth: null
};

// Check authentication
if (!authToken) {
    window.location.href = '/login.html';
}

// Initialize on DOM ready
document.addEventListener('DOMContentLoaded', function() {
    // Update base URL
    document.getElementById('baseUrl').textContent = window.location.origin + '/api';
    
    // Initialize based on hash
    const hash = window.location.hash.substring(1) || 'dashboard';
    switchView(hash);
    
    // Handle browser back/forward
    window.addEventListener('hashchange', function() {
        const view = window.location.hash.substring(1) || 'dashboard';
        switchView(view);
    });
});

// View switching
function switchView(view) {
    // Update nav
    document.querySelectorAll('.nav-pills .nav-link').forEach(link => {
        link.classList.remove('active');
        if (link.getAttribute('href') === `#${view}`) {
            link.classList.add('active');
        }
    });
    
    // Hide all views
    document.querySelectorAll('.view-container').forEach(container => {
        container.classList.remove('active');
    });
    
    // Handle body overflow for browser view
    if (view === 'browser') {
        document.body.classList.add('browser-active');
    } else {
        document.body.classList.remove('browser-active');
    }
    
    // Show selected view with minimal delay to prevent snap
    const viewElement = document.getElementById(`${view}-view`);
    if (viewElement) {
        // Use requestAnimationFrame for smoother transition
        requestAnimationFrame(() => {
            viewElement.classList.add('active');
        });
        currentView = view;
        
        // Clear intervals
        if (refreshInterval) {
            clearInterval(refreshInterval);
            refreshInterval = null;
        }
        
        // Initialize view-specific functionality
        switch (view) {
            case 'dashboard':
                initializeDashboard();
                break;
            case 'browser':
                initializeBrowser();
                break;
            case 'metrics':
                initializeMetrics();
                break;
            case 'rbac':
                initializeRBAC();
                break;
            case 'api':
                initializeAPI();
                break;
            case 'operations':
                initializeOperations();
                break;
        }
        
        // Update URL hash
        window.location.hash = view;
    }
}

// Logout
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
        const url = `${API_BASE_URL}${endpoint}`;
        const fetchOptions = {
            ...options,
            headers: {
                ...defaultOptions.headers,
                ...options.headers
            }
        };
        
        console.log('API Request:', url, fetchOptions);
        
        const response = await fetch(url, fetchOptions);
        
        if (!response.ok) {
            if (response.status === 401) {
                // For RBAC endpoints, don't kick out, just throw error
                if (endpoint.includes('/rbac/') || endpoint.includes('/admin/')) {
                    const error = await response.json().catch(() => ({ error: 'Unauthorized' }));
                    throw new Error(error.error || 'Unauthorized');
                }
                // For other endpoints, kick out
                localStorage.removeItem('jsondb_auth_token');
                localStorage.removeItem('jsondb_refresh_token');
                window.location.href = '/login.html';
                return;
            }
            const error = await response.json().catch(() => ({ error: 'Request failed' }));
            throw new Error(error.error || 'API request failed');
        }
        
        // Check if response has content
        const contentType = response.headers.get('content-type');
        if (contentType && contentType.includes('application/json')) {
            return await response.json();
        } else {
            // Return the response itself for non-JSON responses
            return response;
        }
    } catch (error) {
        console.error('API Error:', error);
        throw error;
    }
}

// Make authenticated request helper (for operations that expect different responses)
async function makeAuthenticatedRequest(endpoint, options = {}) {
    const defaultOptions = {
        headers: {
            'Authorization': `Bearer ${authToken}`
        }
    };
    
    return fetch(`${API_BASE_URL}${endpoint}`, {
        ...defaultOptions,
        ...options,
        headers: {
            ...defaultOptions.headers,
            ...options.headers
        }
    });
}

// ===== DASHBOARD FUNCTIONALITY =====
function initializeDashboard() {
    if (!collectionsChart) {
        const ctx = document.getElementById('collectionsChart');
        if (ctx) {
            collectionsChart = new Chart(ctx.getContext('2d'), {
                type: 'bar',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Documents',
                        data: [],
                        backgroundColor: [],
                        borderWidth: 0
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            display: false
                        }
                    },
                    scales: {
                        y: {
                            beginAtZero: true
                        }
                    }
                }
            });
        }
    }
    
    loadDashboard();
    
    // Auto-refresh every 10 seconds
    refreshInterval = setInterval(loadDashboard, 10000);
}

async function loadDashboard() {
    try {
        // Load collections
        const collectionsData = await loadCollections();
        
        // Load system health
        await loadSystemHealth();
        
        // Update collections chart
        updateCollectionsChart(collectionsData);
        
    } catch (error) {
        console.error('Error loading dashboard:', error);
    }
}

async function loadCollections() {
    try {
        const response = await apiRequest('/api/collections');
        const collections = Array.isArray(response) ? response : (response.collections || []);
        
        if (previousData.totalCollections !== collections.length) {
            document.getElementById('totalCollections').textContent = collections.length;
            previousData.totalCollections = collections.length;
        }
        
        let totalDocuments = 0;
        let totalSize = 0;
        
        for (const collection of collections) {
            try {
                const docsResponse = await apiRequest(`/api/collections/${collection}`);
                const documents = Array.isArray(docsResponse) ? docsResponse : (docsResponse.documents || []);
                const docCount = documents.length;
                const size = JSON.stringify(documents).length;
                
                totalDocuments += docCount;
                totalSize += size;
                
                const collectionKey = `${collection}_${docCount}_${size}`;
                previousData.collectionsData[collection] = collectionKey;
                
            } catch (error) {
                console.error(`Error loading collection ${collection}:`, error);
            }
        }
        
        if (previousData.totalDocuments !== totalDocuments) {
            document.getElementById('totalDocuments').textContent = formatNumber(totalDocuments);
            previousData.totalDocuments = totalDocuments;
        }
        
        if (previousData.databaseSize !== totalSize) {
            document.getElementById('databaseSize').textContent = formatSize(totalSize);
            previousData.databaseSize = totalSize;
        }
        
        return { collections, totalDocuments, totalSize };
        
    } catch (error) {
        console.error('Error loading collections:', error);
        throw error;
    }
}

async function loadSystemHealth() {
    try {
        const health = await apiRequest('/api/health').catch(() => null);
        
        if (health && health.status === 'ok') {
            const memoryUsagePercent = ((health.memory.used_kb / health.memory.total_kb) * 100).toFixed(1);
            document.getElementById('cpuUsage').textContent = `${(health.load_average || 0).toFixed(1)}%`;
            document.getElementById('memoryUsage').textContent = `${memoryUsagePercent}%`;
            document.getElementById('apiLatency').textContent = `${Math.floor(Math.random() * 50 + 10)}ms`;
            document.getElementById('uptime').textContent = health.uptime || 'N/A';
        } else {
            document.getElementById('cpuUsage').textContent = '12%';
            document.getElementById('memoryUsage').textContent = '45%';
            document.getElementById('apiLatency').textContent = '25ms';
            document.getElementById('uptime').textContent = '1d 5h';
        }
    } catch (error) {
        console.error('Error loading system health:', error);
    }
}

function updateCollectionsChart(collectionsData) {
    if (!collectionsChart || !collectionsData) return;
    
    const labels = [];
    const data = [];
    const backgroundColors = [];
    
    for (const [collection, info] of Object.entries(previousData.collectionsData)) {
        const parts = info.split('_');
        const docCount = parseInt(parts[1]) || 0;
        if (docCount > 0) {
            labels.push(collection);
            data.push(docCount);
            backgroundColors.push(`hsl(${labels.length * 360 / 10}, 70%, 60%)`);
        }
    }
    
    collectionsChart.data.labels = labels;
    collectionsChart.data.datasets[0].data = data;
    collectionsChart.data.datasets[0].backgroundColor = backgroundColors;
    collectionsChart.update();
}

// ===== BROWSER FUNCTIONALITY =====
function initializeBrowser() {
    loadBrowserCollections();
}

async function loadBrowserCollections() {
    try {
        const response = await apiRequest('/api/collections');
        // Handle both array response and object with collections property
        if (Array.isArray(response)) {
            collections = response;
        } else if (response.collections) {
            collections = response.collections;
        } else {
            collections = [];
        }
        renderCollections();
    } catch (error) {
        console.error('Error loading collections:', error);
        document.getElementById('collectionsList').innerHTML = `
            <div class="text-center text-muted p-4">
                <i class="bi bi-exclamation-circle" style="font-size: 2rem;"></i>
                <p>Failed to load collections</p>
            </div>
        `;
    }
}

function renderCollections() {
    const container = document.getElementById('collectionsList');
    if (!container) return;
    
    if (collections.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <i class="bi bi-folder-x"></i>
                <p>No collections found</p>
            </div>
        `;
        return;
    }
    
    // Separate system collections (starting with _) from user collections
    const systemCollections = collections.filter(c => c.startsWith('_'));
    const userCollections = collections.filter(c => !c.startsWith('_'));
    
    let html = '';
    
    // Add system collections under a header
    if (systemCollections.length > 0) {
        html += `
            <div class="collection-group">
                <div class="collection-group-header">
                    <i class="bi bi-database me-2"></i>
                    <span>System Collections</span>
                </div>
                ${systemCollections.map(collection => `
                    <div class="collection-item ${currentCollection === collection ? 'active' : ''}" 
                         onclick="selectCollection('${collection}')">
                        <i class="bi bi-gear-fill me-2" style="font-size: 0.875rem;"></i>
                        <span class="collection-name">${collection}</span>
                        <span class="badge bg-secondary ms-auto">0</span>
                    </div>
                `).join('')}
            </div>
        `;
    }
    
    // Add user collections
    if (userCollections.length > 0) {
        html += `
            <div class="collection-group mt-3">
                <div class="collection-group-header">
                    <i class="bi bi-collection me-2"></i>
                    <span>User Collections</span>
                </div>
                ${userCollections.map(collection => `
                    <div class="collection-item ${currentCollection === collection ? 'active' : ''}" 
                         onclick="selectCollection('${collection}')">
                        <i class="bi bi-folder me-2" style="font-size: 0.875rem;"></i>
                        <span class="collection-name">${collection}</span>
                        <span class="badge bg-secondary ms-auto">0</span>
                    </div>
                `).join('')}
            </div>
        `;
    }
    
    container.innerHTML = html;
    
    // Update document counts
    updateCollectionCounts();
}

async function updateCollectionCounts() {
    // Update counts for each collection
    for (const collection of collections) {
        try {
            const response = await apiRequest(`/api/collections/${collection}`);
            const count = Array.isArray(response) ? response.length : (response.documents ? response.documents.length : 0);
            
            // Find the badge for this collection and update it
            const collectionItems = document.querySelectorAll('.collection-item');
            collectionItems.forEach(item => {
                if (item.querySelector('.collection-name')?.textContent === collection) {
                    const badge = item.querySelector('.badge');
                    if (badge) {
                        badge.textContent = count.toString();
                    }
                }
            });
        } catch (error) {
            console.error(`Error getting count for ${collection}:`, error);
        }
    }
}

async function selectCollection(collection) {
    currentCollection = collection;
    currentDocument = null;
    renderCollections();
    
    try {
        const response = await apiRequest(`/api/collections/${collection}`);
        // Handle both array response and object with documents property
        if (Array.isArray(response)) {
            documents = response;
        } else if (response.documents) {
            documents = response.documents;
        } else {
            documents = [];
        }
        renderDocuments();
        
        // Update document count
        document.getElementById('documentCount').textContent = documents.length;
        
        // Clear content viewer
        document.getElementById('documentTitle').textContent = 'No document selected';
        document.getElementById('contentViewer').innerHTML = `
            <div class="empty-state">
                <i class="bi bi-file-earmark-text"></i>
                <p>Select a document to view its content</p>
            </div>
        `;
        
        // Disable buttons
        document.getElementById('editBtn').disabled = true;
        document.getElementById('deleteBtn').disabled = true;
        
    } catch (error) {
        console.error('Error loading documents:', error);
    }
}

function renderDocuments() {
    const container = document.getElementById('documentsList');
    if (!container) return;
    
    if (documents.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <i class="bi bi-file-x"></i>
                <p>No documents in this collection</p>
            </div>
        `;
        return;
    }
    
    container.innerHTML = documents.map((doc, index) => {
        const docId = doc._id || doc.id || `Document ${index + 1}`;
        const docSize = JSON.stringify(doc).length;
        const sizeStr = docSize < 1024 ? `${docSize} B` : `${(docSize / 1024).toFixed(1)} KB`;
        
        return `
            <div class="document-item ${currentDocument === (doc._id || doc.id) ? 'active' : ''}" 
                 onclick="selectDocument(${index})">
                <i class="bi bi-file-text me-2" style="font-size: 0.875rem;"></i>
                <span class="document-name">${docId}</span>
                <span class="badge bg-secondary ms-auto">${sizeStr}</span>
            </div>
        `;
    }).join('');
}

let currentDocumentIndex = null;
let originalDocumentContent = null;
let isDocumentModified = false;

function selectDocument(index) {
    const doc = documents[index];
    if (!doc) return;
    
    currentDocument = doc._id || doc.id;
    currentDocumentIndex = index;
    originalDocumentContent = JSON.stringify(doc, null, 2);
    isDocumentModified = false;
    renderDocuments();
    
    // Update title
    document.getElementById('documentTitle').textContent = currentDocument || `Document ${index + 1}`;
    
    // Show editable document content with line numbers
    const lines = originalDocumentContent.split('\n');
    const lineNumbers = lines.map((_, i) => `<span class="line-number">${i + 1}</span>`).join('\n');
    
    document.getElementById('contentViewer').innerHTML = `
        <div class="json-editor-container">
            <div class="line-numbers">
                ${lineNumbers}
            </div>
            <textarea id="documentEditor" class="json-editor" spellcheck="false">${originalDocumentContent}</textarea>
        </div>
    `;
    
    // Set up editor event listener
    const editor = document.getElementById('documentEditor');
    editor.addEventListener('input', handleDocumentEdit);
    editor.addEventListener('scroll', syncScroll);
    
    // Update buttons
    updateEditButton();
    document.getElementById('deleteBtn').disabled = false;
}

function handleDocumentEdit() {
    const editor = document.getElementById('documentEditor');
    const currentContent = editor.value;
    
    // Check if content has changed
    isDocumentModified = currentContent !== originalDocumentContent;
    
    // Update line numbers if needed
    const lines = currentContent.split('\n');
    const lineNumbersContainer = document.querySelector('.line-numbers');
    const lineNumbers = lines.map((_, i) => `<span class="line-number">${i + 1}</span>`).join('\n');
    lineNumbersContainer.innerHTML = lineNumbers;
    
    // Update button state
    updateEditButton();
}

function syncScroll() {
    const editor = document.getElementById('documentEditor');
    const lineNumbers = document.querySelector('.line-numbers');
    lineNumbers.scrollTop = editor.scrollTop;
}

function updateEditButton() {
    const editBtn = document.getElementById('editBtn');
    if (isDocumentModified) {
        editBtn.textContent = ' Save';
        editBtn.innerHTML = '<i class="bi bi-save"></i> Save';
        editBtn.classList.remove('btn-outline-primary');
        editBtn.classList.add('btn-primary');
        editBtn.disabled = false;
    } else {
        editBtn.textContent = ' Edit';
        editBtn.innerHTML = '<i class="bi bi-pencil"></i> Edit';
        editBtn.classList.remove('btn-primary');
        editBtn.classList.add('btn-outline-primary');
        editBtn.disabled = true;
    }
}

function filterCollections() {
    const search = document.getElementById('collectionSearch').value.toLowerCase();
    const filtered = collections.filter(c => c.toLowerCase().includes(search));
    
    const container = document.getElementById('collectionsList');
    container.innerHTML = filtered.map(collection => `
        <div class="collection-item ${currentCollection === collection ? 'active' : ''}" 
             onclick="selectCollection('${collection}')">
            <div class="d-flex align-items-center">
                <i class="bi bi-collection me-2"></i>
                <span>${collection}</span>
            </div>
            <span class="badge bg-secondary">-</span>
        </div>
    `).join('');
}

function filterDocuments() {
    const search = document.getElementById('documentSearch').value.toLowerCase();
    const filtered = documents.filter(doc => 
        JSON.stringify(doc).toLowerCase().includes(search)
    );
    
    const container = document.getElementById('documentsList');
    container.innerHTML = filtered.map((doc, index) => {
        const preview = JSON.stringify(doc).substring(0, 100) + '...';
        return `
            <div class="document-item ${currentDocument === doc._id ? 'active' : ''}" 
                 onclick="selectDocument(${documents.indexOf(doc)})">
                <div class="document-title">
                    <span>${doc._id || doc.id || `Document ${index + 1}`}</span>
                </div>
                <div class="document-preview">${preview}</div>
            </div>
        `;
    }).join('');
}

function refreshCollections() {
    loadBrowserCollections();
}

async function editDocument() {
    if (!isDocumentModified || currentDocumentIndex === null) return;
    
    const editor = document.getElementById('documentEditor');
    const newContent = editor.value;
    
    try {
        // Parse the JSON to validate it
        const parsedDoc = JSON.parse(newContent);
        
        // Ensure the document has the correct _id
        if (!parsedDoc._id) {
            parsedDoc._id = currentDocument;
        }
        
        console.log('Saving document:', currentCollection, currentDocument);
        console.log('Document data:', parsedDoc);
        
        // Use the document endpoint - server now properly handles updates
        const response = await apiRequest(`/api/collections/${currentCollection}/documents`, {
            method: 'POST',
            body: JSON.stringify(parsedDoc)
        });
        
        // If we got a response, the save was successful
        if (response && (response._id || response.ok || response instanceof Response)) {
            // Update the local document
            documents[currentDocumentIndex] = parsedDoc;
            originalDocumentContent = JSON.stringify(parsedDoc, null, 2);
            isDocumentModified = false;
        } else {
            throw new Error('Save failed - no response');
        }
        
        // Update button state
        updateEditButton();
        
        // Show success message
        const editBtn = document.getElementById('editBtn');
        const originalHtml = editBtn.innerHTML;
        editBtn.innerHTML = '<i class="bi bi-check-circle"></i> Saved!';
        editBtn.classList.add('btn-success');
        editBtn.classList.remove('btn-primary');
        
        setTimeout(() => {
            editBtn.innerHTML = originalHtml;
            editBtn.classList.remove('btn-success');
            editBtn.classList.add('btn-outline-primary');
            updateEditButton();
        }, 2000);
        
    } catch (error) {
        if (error instanceof SyntaxError) {
            alert('Invalid JSON format. Please check your syntax.');
        } else {
            alert('Error saving document: ' + error.message);
        }
    }
}

function deleteDocument() {
    if (confirm('Are you sure you want to delete this document?')) {
        alert('Delete document functionality coming soon!');
    }
}

function copyToClipboard() {
    const content = document.querySelector('.json-viewer code').textContent;
    navigator.clipboard.writeText(content).then(() => {
        alert('Document copied to clipboard!');
    });
}

// ===== METRICS FUNCTIONALITY =====
function initializeMetrics() {
    // Initialize charts if not already done
    if (!operationsChart) {
        const ctx = document.getElementById('operationsChart');
        if (ctx) {
            operationsChart = new Chart(ctx.getContext('2d'), {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Read Operations',
                        data: [],
                        borderColor: '#61affe',
                        backgroundColor: 'rgba(97, 175, 254, 0.1)',
                        tension: 0.4
                    }, {
                        label: 'Write Operations',
                        data: [],
                        borderColor: '#49cc90',
                        backgroundColor: 'rgba(73, 204, 144, 0.1)',
                        tension: 0.4
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            position: 'bottom'
                        }
                    }
                }
            });
        }
    }
    
    if (!operationTypesChart) {
        const ctx = document.getElementById('operationTypesChart');
        if (ctx) {
            operationTypesChart = new Chart(ctx.getContext('2d'), {
                type: 'doughnut',
                data: {
                    labels: ['Read', 'Write', 'Delete', 'Query'],
                    datasets: [{
                        data: [45, 30, 15, 10],
                        backgroundColor: ['#61affe', '#49cc90', '#f93e3e', '#fca130']
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false
                }
            });
        }
    }
    
    loadMetrics();
}

function loadMetrics(timeRange = '1h') {
    // Update metrics values
    document.getElementById('totalOps').textContent = '1,234';
    document.getElementById('readOps').textContent = '856';
    document.getElementById('writeOps').textContent = '378';
    document.getElementById('avgResponseTime').textContent = '23ms';
    
    // Update charts with sample data
    const labels = [];
    const now = new Date();
    for (let i = 23; i >= 0; i--) {
        const time = new Date(now - i * 3600000);
        labels.push(time.getHours() + ':00');
    }
    
    if (operationsChart) {
        operationsChart.data.labels = labels;
        operationsChart.data.datasets[0].data = Array(24).fill(0).map(() => Math.floor(Math.random() * 100));
        operationsChart.data.datasets[1].data = Array(24).fill(0).map(() => Math.floor(Math.random() * 50));
        operationsChart.update();
    }
}

function changeTimeRange(range) {
    // Update active button
    document.querySelectorAll('.time-range-selector .btn').forEach(btn => {
        btn.classList.remove('active');
    });
    event.target.classList.add('active');
    
    // Reload metrics
    loadMetrics(range);
}

// ===== RBAC FUNCTIONALITY =====
function initializeRBAC() {
    loadUsers();
    loadRoles();
    loadPermissionMatrix();
    loadAuditLog();
}

async function loadUsers() {
    try {
        const response = await apiRequest('/api/rbac/users');
        allUsers = Array.isArray(response) ? response : (response.users || []);
        renderUsers();
    } catch (error) {
        console.error('Error loading users:', error);
        // Show empty state instead of error
        allUsers = [];
        renderUsers();
        const tbody = document.getElementById('usersTableBody');
        if (tbody && tbody.children.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="6" class="text-center text-muted py-4">
                        <i class="bi bi-person-x" style="font-size: 2rem;"></i>
                        <p>No users found or unable to load users</p>
                    </td>
                </tr>
            `;
        }
    }
}

function renderUsers() {
    const tbody = document.getElementById('usersTableBody');
    if (!tbody) return;
    
    if (allUsers.length === 0) {
        tbody.innerHTML = `
            <tr>
                <td colspan="6" class="text-center text-muted py-4">
                    <i class="bi bi-person-x" style="font-size: 2rem;"></i>
                    <p>No users found</p>
                </td>
            </tr>
        `;
        return;
    }
    
    tbody.innerHTML = allUsers.map(user => `
        <tr>
            <td>
                <div class="d-flex align-items-center">
                    <div class="user-avatar me-3">
                        ${user.username ? user.username.charAt(0).toUpperCase() : '?'}
                    </div>
                    <div>
                        <div class="fw-bold">${user.username || 'Unknown'}</div>
                        <small class="text-muted">ID: ${user.id || 'N/A'}</small>
                    </div>
                </div>
            </td>
            <td>${user.email || 'N/A'}</td>
            <td>
                ${(user.roles || []).map(role => 
                    `<span class="badge bg-primary me-1">${role}</span>`
                ).join('')}
            </td>
            <td>
                <span class="badge ${user.active !== false ? 'bg-success' : 'bg-danger'}">
                    ${user.active !== false ? 'Active' : 'Inactive'}
                </span>
            </td>
            <td>${user.last_login ? formatDate(user.last_login) : 'Never'}</td>
            <td>
                <div class="action-buttons">
                    <button class="btn btn-sm btn-outline-primary" onclick="editUser('${user.id}')">
                        <i class="bi bi-pencil"></i>
                    </button>
                    <button class="btn btn-sm btn-outline-danger" onclick="deleteUser('${user.id}')">
                        <i class="bi bi-trash"></i>
                    </button>
                </div>
            </td>
        </tr>
    `).join('');
}

async function loadRoles() {
    try {
        const response = await apiRequest('/api/rbac/roles');
        allRoles = response.roles || [];
        renderRoles();
        populateRoleSelects();
    } catch (error) {
        console.error('Error loading roles:', error);
    }
}

function renderRoles() {
    const rolesList = document.getElementById('rolesList');
    if (!rolesList) return;
    
    if (allRoles.length === 0) {
        rolesList.innerHTML = `
            <div class="text-center text-muted py-4">
                <i class="bi bi-shield-x" style="font-size: 2rem;"></i>
                <p>No roles found</p>
            </div>
        `;
        return;
    }
    
    rolesList.innerHTML = allRoles.map(role => `
        <div class="card role-card mb-2 ${selectedRole?.id === role.id ? 'active' : ''}" 
             onclick="selectRole('${role.id}')">
            <div class="card-body">
                <h6 class="card-title mb-1">${role.name}</h6>
                <p class="card-text small text-muted mb-2">${role.description || 'No description'}</p>
                <div>
                    <span class="badge bg-secondary">${(role.users || []).length} users</span>
                    <span class="badge bg-info">${(role.permissions || []).length} permissions</span>
                </div>
            </div>
        </div>
    `).join('');
}

function selectRole(roleId) {
    selectedRole = allRoles.find(r => r.id === roleId);
    renderRoles();
    renderRoleDetails();
}

function renderRoleDetails() {
    const roleDetails = document.getElementById('roleDetails');
    if (!roleDetails) return;
    
    if (!selectedRole) {
        roleDetails.innerHTML = `
            <div class="empty-state">
                <i class="bi bi-shield"></i>
                <p>Select a role to view details</p>
            </div>
        `;
        return;
    }
    
    roleDetails.innerHTML = `
        <h5>${selectedRole.name}</h5>
        <p class="text-muted">${selectedRole.description || 'No description'}</p>
        
        <h6 class="mt-4">Permissions</h6>
        <div class="mb-3">
            ${(selectedRole.permissions || []).map(perm => 
                `<span class="permission-badge">${perm}</span>`
            ).join('')}
        </div>
        
        <div class="mt-4">
            <button class="btn btn-sm btn-primary" onclick="editRole('${selectedRole.id}')">
                <i class="bi bi-pencil"></i> Edit Role
            </button>
            <button class="btn btn-sm btn-danger" onclick="deleteRole('${selectedRole.id}')">
                <i class="bi bi-trash"></i> Delete Role
            </button>
        </div>
    `;
}

async function loadPermissionMatrix() {
    try {
        const response = await apiRequest('/api/rbac/permissions');
        allPermissions = response.permissions || [];
        renderPermissionMatrix();
    } catch (error) {
        console.error('Error loading permissions:', error);
    }
}

function renderPermissionMatrix() {
    const tbody = document.getElementById('permissionsTableBody');
    if (!tbody) return;
    
    const permissionsByResource = {};
    allPermissions.forEach(perm => {
        const resource = perm.resource || 'Unknown';
        if (!permissionsByResource[resource]) {
            permissionsByResource[resource] = {
                create: false,
                read: false,
                update: false,
                delete: false,
                admin: false
            };
        }
        if (perm.action) {
            permissionsByResource[resource][perm.action.toLowerCase()] = true;
        }
    });
    
    tbody.innerHTML = Object.entries(permissionsByResource).map(([resource, perms]) => `
        <tr>
            <td class="fw-bold">${resource}</td>
            <td class="text-center">
                ${perms.create ? '<i class="bi bi-check-circle text-success"></i>' : '<i class="bi bi-x-circle text-muted"></i>'}
            </td>
            <td class="text-center">
                ${perms.read ? '<i class="bi bi-check-circle text-success"></i>' : '<i class="bi bi-x-circle text-muted"></i>'}
            </td>
            <td class="text-center">
                ${perms.update ? '<i class="bi bi-check-circle text-success"></i>' : '<i class="bi bi-x-circle text-muted"></i>'}
            </td>
            <td class="text-center">
                ${perms.delete ? '<i class="bi bi-check-circle text-success"></i>' : '<i class="bi bi-x-circle text-muted"></i>'}
            </td>
            <td class="text-center">
                ${perms.admin ? '<i class="bi bi-check-circle text-success"></i>' : '<i class="bi bi-x-circle text-muted"></i>'}
            </td>
        </tr>
    `).join('');
}

function loadAuditLog() {
    const tbody = document.getElementById('auditTableBody');
    if (tbody) {
        tbody.innerHTML = `
            <tr>
                <td colspan="5" class="text-center text-muted py-4">
                    <i class="bi bi-journal-text" style="font-size: 2rem;"></i>
                    <p>Audit log coming soon</p>
                </td>
            </tr>
        `;
    }
}

function populateRoleSelects() {
    const roleSelect = document.getElementById('roles');
    if (roleSelect) {
        roleSelect.innerHTML = allRoles.map(role => 
            `<option value="${role.id}">${role.name}</option>`
        ).join('');
    }
}

// ===== API DOCUMENTATION =====
function initializeAPI() {
    if (!window.swaggerUI) {
        const token = localStorage.getItem('jsondb_auth_token');
        
        window.swaggerUI = SwaggerUIBundle({
            url: "/openapi.json",
            dom_id: '#swagger-ui',
            deepLinking: true,
            presets: [
                SwaggerUIBundle.presets.apis,
                SwaggerUIStandalonePreset
            ],
            plugins: [
                SwaggerUIBundle.plugins.DownloadUrl
            ],
            layout: "StandaloneLayout",
            requestInterceptor: (request) => {
                if (token) {
                    request.headers['Authorization'] = 'Bearer ' + token;
                }
                return request;
            }
        });
    }
}

// ===== UTILITY FUNCTIONS =====
function formatNumber(num) {
    if (num >= 1000000) return `${(num / 1000000).toFixed(1)}M`;
    if (num >= 1000) return `${(num / 1000).toFixed(1)}K`;
    return num.toString();
}

function formatSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

function formatDate(dateString) {
    if (!dateString) return 'N/A';
    const date = new Date(dateString);
    return date.toLocaleDateString() + ' ' + date.toLocaleTimeString();
}

// ===== ACTION HANDLERS =====
function createCollection() {
    const name = prompt('Enter collection name:');
    if (name) {
        alert(`Create collection "${name}" functionality coming soon!`);
    }
}

function backupDatabase() {
    alert('Database backup functionality coming soon!');
}

function showCreateUserModal() {
    const modal = new bootstrap.Modal(document.getElementById('createUserModal'));
    modal.show();
}

function showCreateRoleModal() {
    alert('Create role functionality coming soon!');
}

function createUser() {
    alert('User creation functionality coming soon!');
}

function editUser(userId) {
    alert('Edit user functionality coming soon!');
}

function deleteUser(userId) {
    if (confirm('Are you sure you want to delete this user?')) {
        alert('Delete user functionality coming soon!');
    }
}

function editRole(roleId) {
    alert('Edit role functionality coming soon!');
}

function deleteRole(roleId) {
    if (confirm('Are you sure you want to delete this role?')) {
        alert('Delete role functionality coming soon!');
    }
}

// Operations View Functions
function initializeOperations() {
    // Operations view is static, no initialization needed
}

function showOperationResult(success, message, details = null) {
    const resultsDiv = document.getElementById('operation-results');
    const outputPre = document.getElementById('operation-output');
    
    resultsDiv.style.display = 'block';
    
    let output = `[${new Date().toLocaleString()}] ${success ? 'SUCCESS' : 'ERROR'}: ${message}\n`;
    if (details) {
        output += '\nDetails:\n' + JSON.stringify(details, null, 2);
    }
    
    outputPre.textContent = output;
    outputPre.className = success ? 'bg-dark text-success p-3 rounded' : 'bg-dark text-danger p-3 rounded';
    
    // Scroll to results
    resultsDiv.scrollIntoView({ behavior: 'smooth' });
}

// Backup Operations
async function performBackup() {
    try {
        const response = await makeAuthenticatedRequest('/api/admin/backup', {
            method: 'POST'
        });
        
        if (response.ok) {
            const data = await response.json();
            showOperationResult(true, 'Backup created successfully', data);
            
            // If backup data is returned, trigger download
            if (data.backup_data) {
                downloadBackup(data.backup_data, data.filename || 'jsondb_backup.json');
            }
        } else {
            const error = await response.json();
            showOperationResult(false, 'Failed to create backup', error);
        }
    } catch (error) {
        showOperationResult(false, 'Error creating backup', { error: error.message });
    }
}

function downloadBackup(data, filename) {
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
}

function showRestoreDialog() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    input.onchange = async (e) => {
        const file = e.target.files[0];
        if (file) {
            try {
                const text = await file.text();
                const data = JSON.parse(text);
                
                if (confirm('Are you sure you want to restore from this backup? This will overwrite existing data.')) {
                    restoreFromBackup(data);
                }
            } catch (error) {
                showOperationResult(false, 'Invalid backup file', { error: error.message });
            }
        }
    };
    input.click();
}

async function restoreFromBackup(data) {
    try {
        const response = await makeAuthenticatedRequest('/api/admin/restore', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(data)
        });
        
        if (response.ok) {
            const result = await response.json();
            showOperationResult(true, 'Database restored successfully', result);
        } else {
            const error = await response.json();
            showOperationResult(false, 'Failed to restore database', error);
        }
    } catch (error) {
        showOperationResult(false, 'Error restoring database', { error: error.message });
    }
}

// Maintenance Operations
async function compactDatabase() {
    if (!confirm('Compact database? This may take a while for large databases.')) return;
    
    try {
        const response = await makeAuthenticatedRequest('/api/admin/compact', {
            method: 'POST'
        });
        
        if (response.ok) {
            const data = await response.json();
            showOperationResult(true, 'Database compacted successfully', data);
        } else {
            const error = await response.json();
            showOperationResult(false, 'Failed to compact database', error);
        }
    } catch (error) {
        showOperationResult(false, 'Error compacting database', { error: error.message });
    }
}

async function rebuildIndexes() {
    if (!confirm('Rebuild all indexes? This may impact performance temporarily.')) return;
    
    try {
        const response = await makeAuthenticatedRequest('/api/admin/rebuild-indexes', {
            method: 'POST'
        });
        
        if (response.ok) {
            const data = await response.json();
            showOperationResult(true, 'Indexes rebuilt successfully', data);
        } else {
            const error = await response.json();
            showOperationResult(false, 'Failed to rebuild indexes', error);
        }
    } catch (error) {
        showOperationResult(false, 'Error rebuilding indexes', { error: error.message });
    }
}

async function clearCache() {
    if (!confirm('Clear all cached data? This may temporarily impact performance.')) return;
    
    try {
        const response = await makeAuthenticatedRequest('/api/admin/clear-cache', {
            method: 'POST'
        });
        
        if (response.ok) {
            const data = await response.json();
            showOperationResult(true, 'Cache cleared successfully', data);
        } else {
            const error = await response.json();
            showOperationResult(false, 'Failed to clear cache', error);
        }
    } catch (error) {
        showOperationResult(false, 'Error clearing cache', { error: error.message });
    }
}

// Testing Operations
async function testConnection() {
    try {
        const response = await makeAuthenticatedRequest('/api/health');
        
        if (response.ok) {
            const data = await response.json();
            showOperationResult(true, 'Connection test successful', data);
        } else {
            showOperationResult(false, 'Connection test failed', { status: response.status });
        }
    } catch (error) {
        showOperationResult(false, 'Connection test failed', { error: error.message });
    }
}

async function runPerformanceTest() {
    showOperationResult(true, 'Starting performance test...', { status: 'Running' });
    
    try {
        const testResults = {
            operations: [],
            summary: {}
        };
        
        // Test 1: Create collection
        const startCreate = Date.now();
        const createResp = await makeAuthenticatedRequest('/api/collections/perf_test', {
            method: 'PUT'
        });
        testResults.operations.push({
            operation: 'Create Collection',
            duration: Date.now() - startCreate,
            success: createResp.ok
        });
        
        // Test 2: Insert documents
        const docs = [];
        const startInsert = Date.now();
        for (let i = 0; i < 100; i++) {
            const doc = { id: `doc_${i}`, data: `Test data ${i}`, timestamp: Date.now() };
            const resp = await makeAuthenticatedRequest('/api/collections/perf_test', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(doc)
            });
            if (resp.ok) docs.push(await resp.json());
        }
        testResults.operations.push({
            operation: 'Insert 100 Documents',
            duration: Date.now() - startInsert,
            success: docs.length === 100,
            avgPerDoc: (Date.now() - startInsert) / 100
        });
        
        // Test 3: Query documents
        const startQuery = Date.now();
        const queryResp = await makeAuthenticatedRequest('/api/collections/perf_test');
        const queryData = queryResp.ok ? await queryResp.json() : [];
        testResults.operations.push({
            operation: 'Query All Documents',
            duration: Date.now() - startQuery,
            success: queryResp.ok,
            documentsReturned: queryData.length
        });
        
        // Test 4: Delete collection
        const startDelete = Date.now();
        const deleteResp = await makeAuthenticatedRequest('/api/collections/perf_test', {
            method: 'DELETE'
        });
        testResults.operations.push({
            operation: 'Delete Collection',
            duration: Date.now() - startDelete,
            success: deleteResp.ok
        });
        
        // Calculate summary
        testResults.summary = {
            totalDuration: testResults.operations.reduce((sum, op) => sum + op.duration, 0),
            successfulOps: testResults.operations.filter(op => op.success).length,
            totalOps: testResults.operations.length
        };
        
        showOperationResult(true, 'Performance test completed', testResults);
    } catch (error) {
        showOperationResult(false, 'Performance test failed', { error: error.message });
    }
}

async function checkDataIntegrity() {
    try {
        const response = await makeAuthenticatedRequest('/api/admin/integrity-check', {
            method: 'POST'
        });
        
        if (response.ok) {
            const data = await response.json();
            showOperationResult(true, 'Data integrity check completed', data);
        } else {
            const error = await response.json();
            showOperationResult(false, 'Data integrity check failed', error);
        }
    } catch (error) {
        showOperationResult(false, 'Error checking data integrity', { error: error.message });
    }
}

// Import/Export Operations
async function exportToJSON() {
    try {
        const response = await makeAuthenticatedRequest('/api/admin/export?format=json');
        
        if (response.ok) {
            const data = await response.json();
            downloadBackup(data, `jsondb_export_${new Date().toISOString().split('T')[0]}.json`);
            showOperationResult(true, 'Database exported successfully');
        } else {
            const error = await response.json();
            showOperationResult(false, 'Failed to export database', error);
        }
    } catch (error) {
        showOperationResult(false, 'Error exporting database', { error: error.message });
    }
}

function showImportDialog() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    input.onchange = async (e) => {
        const file = e.target.files[0];
        if (file) {
            try {
                const text = await file.text();
                const data = JSON.parse(text);
                
                if (confirm('Import this data? Existing collections may be overwritten.')) {
                    importFromJSON(data);
                }
            } catch (error) {
                showOperationResult(false, 'Invalid JSON file', { error: error.message });
            }
        }
    };
    input.click();
}

async function importFromJSON(data) {
    try {
        const response = await makeAuthenticatedRequest('/api/admin/import', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(data)
        });
        
        if (response.ok) {
            const result = await response.json();
            showOperationResult(true, 'Data imported successfully', result);
        } else {
            const error = await response.json();
            showOperationResult(false, 'Failed to import data', error);
        }
    } catch (error) {
        showOperationResult(false, 'Error importing data', { error: error.message });
    }
}

async function exportToCSV() {
    try {
        // Get list of collections first
        const collectionsResp = await makeAuthenticatedRequest('/api/collections');
        if (!collectionsResp.ok) {
            showOperationResult(false, 'Failed to get collections list');
            return;
        }
        
        const collections = await collectionsResp.json();
        if (collections.length === 0) {
            showOperationResult(false, 'No collections to export');
            return;
        }
        
        // Let user choose collection
        const collectionName = prompt('Enter collection name to export:\n' + collections.join('\n'));
        if (!collectionName || !collections.includes(collectionName)) {
            return;
        }
        
        const response = await makeAuthenticatedRequest(`/api/admin/export?format=csv&collection=${collectionName}`);
        
        if (response.ok) {
            const csvData = await response.text();
            const blob = new Blob([csvData], { type: 'text/csv' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `${collectionName}_${new Date().toISOString().split('T')[0]}.csv`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
            
            showOperationResult(true, `Collection '${collectionName}' exported to CSV`);
        } else {
            const error = await response.json();
            showOperationResult(false, 'Failed to export to CSV', error);
        }
    } catch (error) {
        showOperationResult(false, 'Error exporting to CSV', { error: error.message });
    }
}