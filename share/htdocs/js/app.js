// JSONdb Single Page Application
const API_BASE_URL = '';
let authToken = localStorage.getItem('jsondb_auth_token');
let currentView = 'dashboard';
let refreshInterval = null;

// Polling configuration
const POLLING_INTERVALS = {
    dashboard: 30000,     // 30 seconds for dashboard
    browser: 60000,       // 60 seconds for browser
    metrics: 30000,       // 30 seconds for metrics
    rbac: 120000,         // 2 minutes for RBAC
    operations: 60000     // 60 seconds for operations
};

// Chart instances
let collectionsChart = null;
let operationsChart = null;
let operationTypesChart = null;
let cacheHitRateChart = null;

// Data storage
let allUsers = [];
let allRoles = [];
let allPermissions = [];
let selectedRole = null;
let currentCollection = null;
let currentDocument = null;
let collections = [];
let documents = [];
let schemas = [];

// Previous data for optimization
let previousData = {
    totalCollections: null,
    totalDocuments: null,
    databaseSize: null,
    collectionsData: {},
    systemHealth: null,
    lastUpdate: null
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
                // Set up polling for dashboard
                if (POLLING_INTERVALS.dashboard) {
                    refreshInterval = setInterval(() => loadDashboard(true), POLLING_INTERVALS.dashboard);
                }
                break;
            case 'browser':
                initializeBrowser();
                // Set up polling for browser if collection is selected
                if (POLLING_INTERVALS.browser && currentCollection) {
                    refreshInterval = setInterval(() => {
                        if (currentCollection) {
                            loadDocuments(currentCollection, true);
                        }
                    }, POLLING_INTERVALS.browser);
                }
                break;
            case 'metrics':
                initializeMetrics();
                // Set up polling for metrics
                if (POLLING_INTERVALS.metrics) {
                    const currentTimeRange = document.querySelector('.metrics-time-selector .btn-primary')?.dataset?.range || '1h';
                    refreshInterval = setInterval(() => loadMetrics(currentTimeRange, true), POLLING_INTERVALS.metrics);
                }
                break;
            case 'rbac':
                initializeRBAC();
                // Set up polling for RBAC
                if (POLLING_INTERVALS.rbac) {
                    refreshInterval = setInterval(() => loadRBACData(true), POLLING_INTERVALS.rbac);
                }
                break;
            case 'api':
                initializeAPI();
                // No polling for API docs
                break;
            case 'operations':
                initializeOperations();
                // Set up polling for operations
                if (POLLING_INTERVALS.operations) {
                    refreshInterval = setInterval(() => updateOperationsStatus(true), POLLING_INTERVALS.operations);
                }
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

// Notification system
function showNotification(message, type = 'info') {
    const notificationContainer = document.getElementById('notification-container') || createNotificationContainer();
    
    const notification = document.createElement('div');
    notification.className = `alert alert-${type} alert-dismissible fade show`;
    notification.innerHTML = `
        ${message}
        <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
    `;
    
    notificationContainer.appendChild(notification);
    
    // Auto-dismiss after 5 seconds
    setTimeout(() => {
        notification.remove();
    }, 5000);
}

function createNotificationContainer() {
    const container = document.createElement('div');
    container.id = 'notification-container';
    container.style.cssText = 'position: fixed; top: 70px; right: 20px; z-index: 1050; max-width: 350px;';
    document.body.appendChild(container);
    return container;
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
}

async function loadDashboard(isPolling = false) {
    try {
        // Add visual indicator for updates
        if (isPolling && previousData.lastUpdate) {
            const updateIndicator = document.querySelector('.last-update');
            if (updateIndicator) {
                updateIndicator.textContent = `Last updated: ${new Date().toLocaleTimeString()}`;
            }
        }
        
        // Load collections
        const collectionsData = await loadCollections();
        
        // Load system health
        await loadSystemHealth();
        
        // Update collections chart only if data changed
        if (hasDataChanged(collectionsData)) {
            updateCollectionsChart(collectionsData);
        }
        
        previousData.lastUpdate = Date.now();
        
    } catch (error) {
        console.error('Error loading dashboard:', error);
        // Don't stop polling on error
        if (!isPolling) {
            showNotification('Failed to load dashboard data', 'error');
        }
    }
}

// Helper function to check if data has changed
function hasDataChanged(newData) {
    if (!previousData.lastUpdate) return true;
    
    // Compare collection data
    for (const [collection, data] of Object.entries(newData)) {
        if (previousData.collectionsData[collection] !== data) {
            return true;
        }
    }
    
    return false;
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
            
            // Show real API latency from metrics
            let apiLatency = 'N/A';
            if (health.metrics && health.metrics.performance && health.metrics.performance.avg_response_time_ms) {
                apiLatency = `${health.metrics.performance.avg_response_time_ms.toFixed(2)}ms`;
            }
            document.getElementById('apiLatency').textContent = apiLatency;
            document.getElementById('uptime').textContent = health.uptime || 'N/A';
        } else {
            document.getElementById('cpuUsage').textContent = 'N/A';
            document.getElementById('memoryUsage').textContent = 'N/A';
            document.getElementById('apiLatency').textContent = 'N/A';
            document.getElementById('uptime').textContent = 'N/A';
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
        const [collectionsResponse, schemasResponse] = await Promise.all([
            apiRequest('/api/collections'),
            apiRequest('/api/schemas').catch(() => [])
        ]);
        
        // Handle both array response and object with collections property
        if (Array.isArray(collectionsResponse)) {
            collections = collectionsResponse;
        } else if (collectionsResponse.collections) {
            collections = collectionsResponse.collections;
        } else {
            collections = [];
        }
        
        // Store schemas for reference
        schemas = schemasResponse || [];
        
        await renderCollections();
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

async function renderCollections() {
    const container = document.getElementById('collectionsList');
    if (!container) return;
    
    // Fetch schemas if not already loaded
    if (!schemas || schemas.length === 0) {
        try {
            const response = await apiRequest('/api/schemas');
            schemas = response.schemas || [];
        } catch (error) {
            console.error('Failed to fetch schemas:', error);
            schemas = [];
        }
    }
    
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
                ${systemCollections.map(collection => {
                    const hasSchema = schemas && schemas.some(s => s.collection === collection);
                    return `
                        <div class="collection-item ${currentCollection === collection ? 'active' : ''}" 
                             data-collection="${collection}" 
                             onclick="selectCollection('${collection}')">
                            <i class="bi bi-gear-fill me-2" style="font-size: 0.875rem;"></i>
                            <span class="collection-name">${collection}</span>
                            ${hasSchema ? '<i class="bi bi-shield-check text-success ms-1" title="Schema defined"></i>' : ''}
                            <span class="badge bg-secondary ms-auto">0</span>
                        </div>
                    `;
                }).join('')}
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
                ${userCollections.map(collection => {
                    const hasSchema = schemas && schemas.some(s => s.collection === collection);
                    return `
                        <div class="collection-item ${currentCollection === collection ? 'active' : ''}" 
                             onclick="selectCollection('${collection}')">
                            <i class="bi bi-folder me-2" style="font-size: 0.875rem;"></i>
                            <span class="collection-name">${collection}</span>
                            ${hasSchema ? '<i class="bi bi-shield-check text-success ms-1" title="Schema defined"></i>' : ''}
                            <span class="badge bg-secondary ms-auto">0</span>
                        </div>
                    `;
                }).join('')}
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
    await renderCollections();
    
    // Load documents
    await loadDocuments(collection);
    
    // Setup polling for this collection
    if (currentView === 'browser' && POLLING_INTERVALS.browser) {
        if (refreshInterval) {
            clearInterval(refreshInterval);
        }
        refreshInterval = setInterval(() => {
            if (currentCollection === collection && currentView === 'browser') {
                loadDocuments(collection, true);
            }
        }, POLLING_INTERVALS.browser);
    }
}

async function loadDocuments(collection, isPolling = false) {
    try {
        const response = await apiRequest(`/api/collections/${collection}`);
        // Handle both array response and object with documents property
        const newDocuments = Array.isArray(response) ? response : (response.documents || []);
        
        // Check if documents have changed
        const hasChanged = JSON.stringify(documents) !== JSON.stringify(newDocuments);
        
        if (!isPolling || hasChanged) {
            documents = newDocuments;
            renderDocuments();
            
            // Update document count
            document.getElementById('documentCount').textContent = documents.length;
            
            // If no document is selected, show empty state
            if (!currentDocument) {
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
            }
            
            // Show update notification if polling
            if (isPolling && hasChanged) {
                showNotification('Documents updated', 'info');
            }
        }
        
    } catch (error) {
        console.error('Error loading documents:', error);
        if (!isPolling) {
            showNotification('Failed to load documents', 'error');
        }
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
    
    if (!cacheHitRateChart) {
        const ctx = document.getElementById('cacheHitRateChart');
        if (ctx) {
            cacheHitRateChart = new Chart(ctx.getContext('2d'), {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Hit Rate (%)',
                        data: [],
                        borderColor: '#49cc90',
                        backgroundColor: 'rgba(73, 204, 144, 0.1)',
                        tension: 0.4,
                        fill: true
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        y: {
                            beginAtZero: true,
                            max: 100,
                            ticks: {
                                callback: function(value) {
                                    return value + '%';
                                }
                            }
                        }
                    }
                }
            });
        }
    }
    
    loadMetrics();
}

async function loadMetrics(timeRange = '1h', isPolling = false) {
    try {
        // Calculate time range for historical data
        const now = Math.floor(Date.now() / 1000);
        let startTime, interval;
        
        switch(timeRange) {
            case '1h':
                startTime = now - 3600;
                interval = 300; // 5 minutes
                break;
            case '24h':
                startTime = now - 86400;
                interval = 3600; // 1 hour
                break;
            case '7d':
                startTime = now - 604800;
                interval = 86400; // 1 day
                break;
            case '30d':
                startTime = now - 2592000;
                interval = 86400; // 1 day
                break;
            default:
                startTime = now - 3600;
                interval = 300;
        }
        
        // Get current metrics and historical data in parallel
        const [health, collections, cacheStats, historyReadOps, historyWriteOps, historyResponseTime, historyCacheHits, historyCacheMisses] = await Promise.all([
            apiRequest('/api/health').catch(() => null),
            apiRequest('/api/collections').catch(() => ({ collections: [] })),
            apiRequest('/api/cache/stats').catch(() => null),
            apiRequest(`/api/metrics/aggregate?metric=db_read_operations_total&start=${startTime}&end=${now}&interval=${interval}`).catch(() => null),
            apiRequest(`/api/metrics/aggregate?metric=db_write_operations_total&start=${startTime}&end=${now}&interval=${interval}`).catch(() => null),
            apiRequest(`/api/metrics/aggregate?metric=db_operation_duration_seconds&start=${startTime}&end=${now}&interval=${interval}`).catch(() => null),
            apiRequest(`/api/metrics/aggregate?metric=cache_hits_total&start=${startTime}&end=${now}&interval=${interval}`).catch(() => null),
            apiRequest(`/api/metrics/aggregate?metric=cache_misses_total&start=${startTime}&end=${now}&interval=${interval}`).catch(() => null)
        ]);
        
        // Get real document counts from collections
        let totalDocs = 0;
        const collectionList = Array.isArray(collections) ? collections : (collections.collections || []);
        for (const collection of collectionList) {
            try {
                const docs = await apiRequest(`/api/collections/${collection}`);
                const docCount = Array.isArray(docs) ? docs.length : (docs.documents ? docs.documents.length : 0);
                totalDocs += docCount;
            } catch (e) {
                console.error(`Error loading collection ${collection}:`, e);
            }
        }
        
        // Get current metrics from health endpoint
        let totalOps = 0;
        let readOps = 0;
        let writeOps = 0;
        let avgResponseTime = null;
        
        if (health && health.metrics) {
            totalOps = health.metrics.operations.total || 0;
            readOps = health.metrics.operations.read || 0;
            writeOps = health.metrics.operations.write || 0;
            
            if (health.metrics.performance) {
                avgResponseTime = health.metrics.performance.avg_response_time_ms;
            }
        }
        
        // Add visual indicator for updates
        if (isPolling) {
            const metricsLastUpdate = document.querySelector('#metrics-view .last-update');
            if (metricsLastUpdate) {
                metricsLastUpdate.textContent = `Last updated: ${new Date().toLocaleTimeString()}`;
            }
        }
        
        // Update metrics display with real values
        document.getElementById('totalOps').textContent = totalOps > 0 ? formatNumber(totalOps) : '0';
        document.getElementById('readOps').textContent = readOps > 0 ? formatNumber(readOps) : '0';
        document.getElementById('writeOps').textContent = writeOps > 0 ? formatNumber(writeOps) : '0';
        
        // Show real response time if available
        document.getElementById('avgResponseTime').textContent = avgResponseTime !== null ? `${avgResponseTime.toFixed(2)}ms` : 'N/A';
        
        // Update cache metrics
        if (cacheStats) {
            const hitRate = cacheStats.hit_rate_percent || 0;
            const hits = cacheStats.hits || 0;
            const misses = cacheStats.misses || 0;
            const size = cacheStats.size || 0;
            const memoryMB = (cacheStats.memory_mb || 0).toFixed(2);
            
            document.getElementById('cacheHitRate').textContent = `${hitRate.toFixed(1)}%`;
            document.getElementById('cacheStats').textContent = `${formatNumber(hits)} hits / ${formatNumber(misses)} misses`;
            document.getElementById('cacheSize').textContent = formatNumber(size);
            document.getElementById('cacheMemory').textContent = `${memoryMB} MB`;
        } else {
            document.getElementById('cacheHitRate').textContent = '0%';
            document.getElementById('cacheStats').textContent = '0 hits / 0 misses';
            document.getElementById('cacheSize').textContent = '0';
            document.getElementById('cacheMemory').textContent = '0 MB';
        }
        
        // Process historical data for charts
        const labels = [];
        const readData = [];
        const writeData = [];
        const responseTimeData = [];
        
        // Extract data from historical metrics
        if (historyReadOps && historyReadOps.data) {
            historyReadOps.data.forEach(point => {
                const date = new Date(point.timestamp * 1000);
                let label;
                
                if (timeRange === '1h') {
                    label = date.getHours() + ':' + String(date.getMinutes()).padStart(2, '0');
                } else if (timeRange === '24h') {
                    label = date.getHours() + ':00';
                } else if (timeRange === '7d') {
                    label = date.toLocaleDateString('en', { weekday: 'short' });
                } else {
                    label = date.toLocaleDateString('en', { month: 'short', day: 'numeric' });
                }
                
                labels.push(label);
                readData.push(point.avg || 0);
            });
        }
        
        // Extract write operations data
        if (historyWriteOps && historyWriteOps.data) {
            historyWriteOps.data.forEach((point, index) => {
                writeData.push(point.avg || 0);
            });
        }
        
        // Extract response time data
        if (historyResponseTime && historyResponseTime.data) {
            historyResponseTime.data.forEach((point, index) => {
                // Convert seconds to milliseconds
                responseTimeData.push((point.avg || 0) * 1000);
            });
        }
        
        // If no historical data, create empty arrays
        if (labels.length === 0) {
            const now = new Date();
            const dataPoints = timeRange === '1h' ? 12 : timeRange === '24h' ? 24 : timeRange === '7d' ? 7 : 30;
            
            for (let i = dataPoints - 1; i >= 0; i--) {
                if (timeRange === '1h') {
                    const time = new Date(now - i * 5 * 60000);
                    labels.push(time.getHours() + ':' + String(time.getMinutes()).padStart(2, '0'));
                } else if (timeRange === '24h') {
                    const time = new Date(now - i * 3600000);
                    labels.push(time.getHours() + ':00');
                } else if (timeRange === '7d') {
                    const time = new Date(now - i * 86400000);
                    labels.push(time.toLocaleDateString('en', { weekday: 'short' }));
                } else {
                    const time = new Date(now - i * 86400000);
                    labels.push(time.toLocaleDateString('en', { month: 'short', day: 'numeric' }));
                }
                readData.push(0);
                writeData.push(0);
                responseTimeData.push(0);
            }
        }
        
        // Update operations chart with real data
        if (operationsChart) {
            operationsChart.data.labels = labels;
            operationsChart.data.datasets[0].data = readData;
            operationsChart.data.datasets[1].data = writeData;
            operationsChart.update();
        }
        
        // Calculate operation types for pie chart
        const currentReadOps = readData[readData.length - 1] || readOps;
        const currentWriteOps = writeData[writeData.length - 1] || writeOps;
        
        if (operationTypesChart) {
            operationTypesChart.data.datasets[0].data = [
                currentReadOps,
                currentWriteOps,
                0, // Updates (placeholder)
                0  // Deletes (placeholder)
            ];
            operationTypesChart.update();
        }
        
        // Process cache hit rate data
        const cacheHitRateData = [];
        if (historyCacheHits && historyCacheMisses && 
            historyCacheHits.data && historyCacheMisses.data) {
            
            // Calculate hit rate for each time point
            for (let i = 0; i < historyCacheHits.data.length; i++) {
                const hits = historyCacheHits.data[i].avg || 0;
                const misses = historyCacheMisses.data[i].avg || 0;
                const total = hits + misses;
                const hitRate = total > 0 ? (hits / total * 100) : 0;
                cacheHitRateData.push(hitRate);
            }
        } else {
            // Fill with zeros if no data
            for (let i = 0; i < labels.length; i++) {
                cacheHitRateData.push(0);
            }
        }
        
        // Update cache hit rate chart
        if (cacheHitRateChart) {
            cacheHitRateChart.data.labels = labels;
            cacheHitRateChart.data.datasets[0].data = cacheHitRateData;
            cacheHitRateChart.update();
        }
        
    } catch (error) {
        console.error('Error loading metrics:', error);
        // Show N/A on error
        document.getElementById('totalOps').textContent = 'N/A';
        document.getElementById('readOps').textContent = 'N/A';
        document.getElementById('writeOps').textContent = 'N/A';
        document.getElementById('avgResponseTime').textContent = 'N/A';
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
    loadRBACData();
}

async function loadRBACData(isPolling = false) {
    try {
        // Add visual indicator for updates
        if (isPolling) {
            const rbacLastUpdate = document.querySelector('#rbac-view .last-update');
            if (rbacLastUpdate) {
                rbacLastUpdate.textContent = `Last updated: ${new Date().toLocaleTimeString()}`;
            }
        }
        
        // Load all RBAC data
        await Promise.all([
            loadUsers(),
            loadRoles(),
            loadPermissionMatrix(),
            loadAuditLog()
        ]);
        
        if (isPolling) {
            showNotification('RBAC data refreshed', 'info');
        }
    } catch (error) {
        console.error('Error loading RBAC data:', error);
        if (!isPolling) {
            showNotification('Failed to load RBAC data', 'error');
        }
    }
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
    updateOperationsStatus();
}

async function updateOperationsStatus(isPolling = false) {
    try {
        // Get database status
        const health = await apiRequest('/api/health');
        
        // Update status indicators
        const statusIndicators = document.querySelectorAll('.operation-status');
        statusIndicators.forEach(indicator => {
            indicator.className = 'operation-status badge bg-success';
            indicator.textContent = 'Ready';
        });
        
        // Update last backup time if available
        const lastBackupElement = document.querySelector('.last-backup-time');
        if (lastBackupElement && health.last_backup) {
            lastBackupElement.textContent = `Last backup: ${formatDate(health.last_backup)}`;
        }
        
        // Add visual indicator for updates
        if (isPolling) {
            const opsLastUpdate = document.querySelector('#operations-view .last-update');
            if (opsLastUpdate) {
                opsLastUpdate.textContent = `Last updated: ${new Date().toLocaleTimeString()}`;
            }
        }
        
    } catch (error) {
        console.error('Error updating operations status:', error);
        // Update status to error
        const statusIndicators = document.querySelectorAll('.operation-status');
        statusIndicators.forEach(indicator => {
            indicator.className = 'operation-status badge bg-danger';
            indicator.textContent = 'Error';
        });
    }
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

// ===== SCHEMA MANAGEMENT FUNCTIONALITY =====
let schemas = [];
let currentSchema = null;

// Show schema manager modal
function showSchemaManager() {
    const modal = new bootstrap.Modal(document.getElementById('schemaManagerModal'));
    modal.show();
    loadSchemas();
    loadCollectionsForSchema();
}

// Load all schemas
async function loadSchemas() {
    try {
        const response = await apiRequest('/api/schemas');
        schemas = response || [];
        renderSchemaList();
    } catch (error) {
        console.error('Error loading schemas:', error);
        schemas = [];
        renderSchemaList();
    }
}

// Render schema list
function renderSchemaList() {
    const container = document.getElementById('schemaList');
    
    if (schemas.length === 0) {
        container.innerHTML = '<div class="text-muted text-center p-3">No schemas defined</div>';
        return;
    }
    
    container.innerHTML = schemas.map(schema => `
        <a href="#" class="list-group-item list-group-item-action ${currentSchema?.collection === schema.collection ? 'active' : ''}"
           onclick="selectSchema('${schema.collection}')">
            <div class="d-flex w-100 justify-content-between">
                <h6 class="mb-1">${schema.collection}</h6>
                ${schema.validation_count !== undefined ? `<small>${schema.validation_count} rules</small>` : ''}
            </div>
            ${schema.description ? `<p class="mb-1 small">${schema.description}</p>` : ''}
        </a>
    `).join('');
}

// Load collections for schema dropdown
async function loadCollectionsForSchema() {
    try {
        const response = await apiRequest('/api/collections');
        const collectionList = Array.isArray(response) ? response : (response.collections || []);
        
        const select = document.getElementById('schemaCollection');
        select.innerHTML = '<option value="">Select a collection...</option>' +
            collectionList.map(col => `<option value="${col}">${col}</option>`).join('');
    } catch (error) {
        console.error('Error loading collections:', error);
    }
}

// Select a schema for editing
async function selectSchema(collection) {
    try {
        const response = await apiRequest(`/api/schemas/${collection}`);
        currentSchema = response;
        
        // Show editor
        document.getElementById('schemaEditorContainer').style.display = 'block';
        document.getElementById('schemaWelcome').style.display = 'none';
        
        // Populate form
        document.getElementById('schemaCollection').value = collection;
        document.getElementById('schemaDescription').value = currentSchema.description || '';
        document.getElementById('schemaDefinition').value = JSON.stringify(currentSchema.schema || {}, null, 2);
        document.getElementById('schemaStrict').checked = currentSchema.strict || false;
        
        // Update title
        document.getElementById('schemaEditorTitle').textContent = `Edit Schema: ${collection}`;
        
        // Disable collection selector for existing schemas
        document.getElementById('schemaCollection').disabled = true;
        
        // Update active state in list
        renderSchemaList();
    } catch (error) {
        console.error('Error loading schema:', error);
        showNotification('Failed to load schema', 'error');
    }
}

// Create new schema
function createNewSchema() {
    currentSchema = null;
    
    // Show editor
    document.getElementById('schemaEditorContainer').style.display = 'block';
    document.getElementById('schemaWelcome').style.display = 'none';
    
    // Clear form
    document.getElementById('schemaCollection').value = '';
    document.getElementById('schemaDescription').value = '';
    document.getElementById('schemaDefinition').value = JSON.stringify({
        "type": "object",
        "properties": {
            "_id": {
                "type": "string",
                "description": "Document ID"
            }
        },
        "required": [],
        "additionalProperties": true
    }, null, 2);
    document.getElementById('schemaStrict').checked = false;
    
    // Update title
    document.getElementById('schemaEditorTitle').textContent = 'Create New Schema';
    
    // Enable collection selector for new schemas
    document.getElementById('schemaCollection').disabled = false;
    
    // Update active state in list
    renderSchemaList();
}

// Save schema
async function saveSchema() {
    const collection = document.getElementById('schemaCollection').value;
    const description = document.getElementById('schemaDescription').value;
    const definitionText = document.getElementById('schemaDefinition').value;
    const strict = document.getElementById('schemaStrict').checked;
    
    if (!collection) {
        showNotification('Please select a collection', 'error');
        return;
    }
    
    // Validate JSON
    let schema;
    try {
        schema = JSON.parse(definitionText);
    } catch (error) {
        showNotification('Invalid JSON schema: ' + error.message, 'error');
        return;
    }
    
    const schemaData = {
        collection: collection,
        description: description,
        schema: schema,
        strict: strict
    };
    
    try {
        const method = currentSchema ? 'PUT' : 'POST';
        const url = currentSchema ? `/api/schemas/${collection}` : '/api/schemas';
        
        const response = await apiRequest(url, method, schemaData);
        
        showNotification(`Schema ${currentSchema ? 'updated' : 'created'} successfully`, 'success');
        
        // Reload schemas
        await loadSchemas();
        
        // Select the saved schema
        selectSchema(collection);
    } catch (error) {
        console.error('Error saving schema:', error);
        showNotification('Failed to save schema', 'error');
    }
}

// Delete schema
async function deleteSchema() {
    if (!currentSchema) return;
    
    if (!confirm(`Are you sure you want to delete the schema for collection '${currentSchema.collection}'?`)) {
        return;
    }
    
    try {
        await apiRequest(`/api/schemas/${currentSchema.collection}`, 'DELETE');
        
        showNotification('Schema deleted successfully', 'success');
        
        // Clear editor
        document.getElementById('schemaEditorContainer').style.display = 'none';
        document.getElementById('schemaWelcome').style.display = 'block';
        currentSchema = null;
        
        // Reload schemas
        await loadSchemas();
    } catch (error) {
        console.error('Error deleting schema:', error);
        showNotification('Failed to delete schema', 'error');
    }
}