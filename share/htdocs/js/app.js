// JDBX Single Page Application
const API_BASE_URL = '';
let authToken = localStorage.getItem('jdbx_auth_token');
let currentView = 'dashboard';
let refreshInterval = null;

// Polling configuration - conservative intervals to prevent server overload
const POLLING_INTERVALS = {
    dashboard: 60000,     // 60 seconds for dashboard
    browser: 300000,      // 5 minutes for browser (very conservative)
    metrics: 60000,       // 60 seconds for metrics
    rbac: 60000,          // 60 seconds for RBAC
    operations: 120000    // 2 minutes for operations
};

// Chart instances
let collectionsChart = null;
let operationsChart = null;
let operationTypesChart = null;
let cacheHitRateChart = null;
let memoryUsageChart = null;
let databaseSizeChart = null;
let storageChart = null;
let connectionsChart = null;
let responseTimesChart = null;
let scriptPerformanceChart = null;

// Data storage - UNIFIED DOCUMENTS ARCHITECTURE
let allUsers = [];
let allRoles = [];
let allPermissions = [];
let selectedRole = null;
let currentLibrary = 'default';  // Current selected library
let currentCollection = null;
let currentDocument = null;
let libraries = [];  // List of available libraries
let collections = [];  // Virtual collections (document types)
let documents = [];   // All documents from unified API
let schemas = [];
let queryBuilderVisible = false;  // Track query builder state

// Unified Documents API mappings
const UNIFIED_API = {
    documents: '/api/documents',        // Main unified documents endpoint
    collections: '/api/collections',    // Virtual collections (document types)
    libraries: '/api/libraries'         // Library management
};

// Previous data for optimization
let previousData = {
    totalCollections: null,
    totalDocuments: null,
    databaseSize: null,
    collectionsData: {},
    systemHealth: null,
    lastUpdate: null
};

// Global variable to store consistent database size across all displays
window.currentDatabaseSizeBytes = 0;

// Unified function to get actual database size from unified documents API
async function getActualDatabaseSize() {
    try {
        // Get size from unified documents API with stats query
        const response = await apiRequest('/api/documents?stats=true');
        let totalSize = 0;
        
        // Handle unified documents response format
        if (response && response.stats) {
            totalSize = response.stats.total_size || 0;
        } else if (response && response.documents) {
            // Fallback: estimate size from document count
            totalSize = response.documents.length * 1024; // Rough estimate
        }
        
        // Apply minimum size for system documents (20KB)
        const actualSize = Math.max(totalSize, 20480);
        window.currentDatabaseSizeBytes = actualSize;
        
        return actualSize;
    } catch (error) {
        console.warn('Failed to get actual database size from unified API, using cached value:', error);
        // Fallback to old collections API
        try {
            const fallbackResponse = await apiRequest('/api/collections');
            const collections = Array.isArray(fallbackResponse) ? fallbackResponse : (fallbackResponse?.collections || []);
            const totalSize = collections.reduce((sum, collection) => sum + (collection.total_size || 0), 0);
            return Math.max(totalSize, 20480);
        } catch (fallbackError) {
            return window.currentDatabaseSizeBytes || 20480;
        }
    }
}

// Check authentication
if (!authToken) {
    window.location.href = '/login.html';
}

// Session validation check
let sessionCheckInterval = null;

async function validateSession() {
    // Always get fresh token from localStorage
    const currentToken = localStorage.getItem('jdbx_auth_token');
    authToken = currentToken; // Update global variable
    
    console.log('Session validation starting, token present:', !!currentToken);
    
    if (!currentToken) {
        console.log('No auth token, redirecting to login');
        window.location.href = '/login.html';
        return false;
    }
    
    try {
        // Make a lightweight request to check if session is valid
        // Using /api/libraries endpoint which requires auth but is lightweight
        const response = await fetch(`${API_BASE_URL}/api/libraries`, {
            method: 'GET',  // Use GET since server doesn't support HEAD
            headers: {
                'Authorization': `Bearer ${currentToken}`
            }
        });
        
        if (response.status === 401) {
            console.log('Session invalid (401), redirecting to login');
            console.log('Token was:', currentToken ? currentToken.substring(0, 20) + '...' : 'null');
            // Clear tokens
            localStorage.removeItem('jdbx_auth_token');
            localStorage.removeItem('jdbx_refresh_token');
            // Clear session check interval
            if (sessionCheckInterval) {
                clearInterval(sessionCheckInterval);
            }
            // Redirect to login
            window.location.href = '/login.html';
            return false;
        }
        
        // If HEAD method not allowed or not found, it's still a valid session (just not optimal)
        if (response.status === 405 || response.status === 404) {
            return true;
        }
        
        return response.ok;
    } catch (error) {
        console.error('Session validation error:', error);
        // On network error, don't log out immediately
        return true;
    }
}

// Start session validation check - every 30 seconds
function startSessionValidation() {
    // Initial check after 15 seconds to avoid interfering with login flow
    setTimeout(validateSession, 15000);
    
    // Then check every 30 seconds
    sessionCheckInterval = setInterval(validateSession, 30000);
}

// Start session validation when page loads
startSessionValidation();

// Debug: Monitor RBAC view for unexpected changes
window.addEventListener('DOMContentLoaded', function() {
    const rbacView = document.getElementById('rbac-view');
    if (rbacView) {
        console.log('Initial RBAC view content length:', rbacView.innerHTML.length);
        console.log('Initial RBAC view text:', rbacView.textContent.substring(0, 50));
        
        // Monitor for the admin text issue
        setInterval(() => {
            if (rbacView.textContent.trim() === 'admin') {
                console.error('DETECTED: RBAC view contains only "admin"!');
                console.trace('Stack trace for admin text:');
            }
        }, 1000);
    }
});

// ID Conflict Detector - Prevents issues like the "admin" bug
function detectIDConflicts() {
    const allElements = document.querySelectorAll('[id]');
    const idMap = {};
    let conflictsFound = false;
    
    allElements.forEach(el => {
        const id = el.id;
        if (!idMap[id]) {
            idMap[id] = [];
        }
        idMap[id].push(el);
    });
    
    // Report conflicts
    Object.entries(idMap).forEach(([id, elements]) => {
        if (elements.length > 1) {
            conflictsFound = true;
            console.error(`🚨 ID CONFLICT DETECTED: "${id}" used ${elements.length} times:`);
            elements.forEach(el => {
                console.error(`   - <${el.tagName}> ${el.className ? `class="${el.className}"` : ''}`, el);
            });
        }
    });
    
    if (conflictsFound) {
        console.error('⚠️  ID conflicts can cause serious bugs! See FRONTEND_BEST_PRACTICES.md');
    } else {
        console.log('✅ No ID conflicts detected');
    }
    
    return !conflictsFound;
}

// Initialize on DOM ready
// Expose functions to global scope for onclick handlers
window.switchView = switchView;
window.logout = logout;
window.switchLibrary = switchLibrary;
window.showLibraryManager = showLibraryManager;
window.createNewLibrary = createNewLibrary;
window.deleteLibrary = deleteLibrary;
window.deleteSpecificLibrary = deleteSpecificLibrary;
window.createNewCollection = createNewCollection;
window.createNewDocument = createNewDocument;
window.showSchemaManager = showSchemaManager;
window.refreshCollections = refreshCollections;
window.selectCollection = selectCollection;
window.toggleQueryBuilder = toggleQueryBuilder;
window.copyToClipboard = copyToClipboard;
window.deleteDocument = deleteDocument;
window.saveDocument = saveDocument;
window.toggleEditMode = toggleEditMode;
window.showNotification = showNotification;
window.apiRequest = apiRequest;

document.addEventListener('DOMContentLoaded', async function() {
    // Run ID conflict detection
    detectIDConflicts();
    
    // Update base URL
    const baseUrlElement = document.getElementById('baseUrl');
    if (baseUrlElement) {
        baseUrlElement.textContent = window.location.origin + '/api';
    }
    
    // Verify RBAC view structure
    const rbacView = document.getElementById('rbac-view');
    if (rbacView && rbacView.children.length === 0) {
        console.error('RBAC view is empty! Reloading page...');
        window.location.reload();
        return;
    }
    
    // Load libraries first for global selector
    await loadLibraries();
    
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
    
    // Handle body classes for different views
    document.body.classList.remove('page-dashboard', 'page-browser', 
                                    'page-metrics', 'page-rbac', 'page-operations', 'page-scripts', 'page-api');
    
    // Add page-specific class
    document.body.classList.add(`page-${view}`);
    
    // Control library selector visibility based on view
    const librarySelector = document.querySelector('.library-selector-nav');
    if (librarySelector) {
        // Show library selector only on browser, collections, and scripts views
        const showLibrarySelector = ['browser', 'collections', 'scripts'].includes(view);
        librarySelector.style.display = showLibrarySelector ? 'flex' : 'none';
    }
    
    // Show selected view with minimal delay to prevent snap
    const viewElement = document.getElementById(`${view}-view`);
    if (viewElement) {
        console.log(`Switching to view: ${view}`);
        console.log(`View element found:`, viewElement);
        console.log(`View element content length:`, viewElement.innerHTML.length);
        console.log(`View element text preview:`, viewElement.textContent.substring(0, 100));
        
        // Special check for RBAC view
        if (view === 'rbac' && viewElement.textContent.trim() === 'admin') {
            console.error('ERROR: RBAC view contains only "admin" when switching!');
            console.error('This should not happen. View HTML length:', viewElement.innerHTML.length);
        }
        
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
                // Set up polling for browser - refresh both libraries/collections list and documents
                if (POLLING_INTERVALS.browser) {
                    refreshInterval = setInterval(async () => {
                        try {
                            // Only poll if we're still in browser view to avoid unnecessary requests
                            if (currentView !== 'browser') {
                                return;
                            }
                            
                            // Refresh libraries and collections list with timeout
                            await Promise.race([
                                loadLibraries(),
                                new Promise((_, reject) => setTimeout(() => reject(new Error('Polling timeout')), 10000))
                            ]);
                            
                            // If a collection is selected, also refresh its documents
                            if (currentCollection) {
                                loadDocuments(currentCollection, true);
                            }
                        } catch (error) {
                            console.warn('Browser polling error (non-critical):', error.message);
                            // Don't stop polling on error, just log it
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
                // Add safeguard to ensure RBAC view isn't just text
                const rbacView = document.getElementById('rbac-view');
                if (rbacView && rbacView.textContent.trim() === 'admin') {
                    console.error('RBAC view contains only "admin" text! Reloading page...');
                    window.location.reload();
                    return;
                }
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
            case 'scripts':
                initializeScripts();
                // Set up polling for scripts (no regular polling needed)
                break;
        }
        
        // Update URL hash
        window.location.hash = view;
    }
}

// Logout
function logout() {
    localStorage.removeItem('jdbx_auth_token');
    localStorage.removeItem('jdbx_refresh_token');
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
    // Always get fresh token from localStorage to handle token refresh/updates
    const currentToken = localStorage.getItem('jdbx_auth_token');
    const defaultOptions = {
        headers: {
            'Authorization': `Bearer ${currentToken}`,
            'Content-Type': 'application/json',
            'Connection': 'close'  // Prevent keep-alive connection issues
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
        
        // Log request details for debugging
        if (options.method === 'PUT' || options.method === 'POST') {
            console.log('API Request:', url, {
                method: fetchOptions.method,
                headers: fetchOptions.headers,
                body: fetchOptions.body ? fetchOptions.body.substring(0, 200) + '...' : 'No body'
            });
        } else {
            console.log('API Request:', url, fetchOptions);
        }
        
        const response = await fetch(url, fetchOptions);
        
        if (!response.ok) {
            if (response.status === 401) {
                // For RBAC, admin, and metrics endpoints, don't kick out, just throw error
                if (endpoint.includes('/rbac/') || endpoint.includes('/admin/') || endpoint.includes('/metrics/')) {
                    const error = await response.json().catch(() => ({ error: 'Unauthorized' }));
                    throw new Error(error.error || 'Unauthorized');
                }
                // For other endpoints, kick out
                localStorage.removeItem('jdbx_auth_token');
                localStorage.removeItem('jdbx_refresh_token');
                // Clear session check interval
                if (sessionCheckInterval) {
                    clearInterval(sessionCheckInterval);
                }
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
            try {
                // Destroy existing chart if canvas is already in use
                const existingChart = Chart.getChart(ctx);
                if (existingChart) {
                    existingChart.destroy();
                }
                collectionsChart = new Chart(ctx.getContext('2d'), {
                type: 'pie',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Documents',
                        data: [],
                        backgroundColor: [
                            'rgba(255, 99, 132, 0.5)',
                            'rgba(54, 162, 235, 0.5)',
                            'rgba(255, 205, 86, 0.5)',
                            'rgba(75, 192, 192, 0.5)',
                            'rgba(153, 102, 255, 0.5)',
                            'rgba(255, 159, 64, 0.5)',
                            'rgba(199, 199, 199, 0.5)',
                            'rgba(83, 102, 255, 0.5)',
                            'rgba(255, 99, 255, 0.5)',
                            'rgba(99, 255, 132, 0.5)'
                        ],
                        borderColor: [
                            'rgba(255, 99, 132, 1)',
                            'rgba(54, 162, 235, 1)',
                            'rgba(255, 205, 86, 1)',
                            'rgba(75, 192, 192, 1)',
                            'rgba(153, 102, 255, 1)',
                            'rgba(255, 159, 64, 1)',
                            'rgba(199, 199, 199, 1)',
                            'rgba(83, 102, 255, 1)',
                            'rgba(255, 99, 255, 1)',
                            'rgba(99, 255, 132, 1)'
                        ],
                        borderWidth: 1
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            position: 'right'
                        },
                        tooltip: {
                            callbacks: {
                                label: function(context) {
                                    const label = context.label || '';
                                    const value = context.parsed || 0;
                                    const total = context.dataset.data.reduce((a, b) => a + b, 0);
                                    const percentage = ((value / total) * 100).toFixed(1);
                                    return `${label}: ${formatNumber(value)} (${percentage}%)`;
                                }
                            }
                        }
                    }
                }
            });
            } catch (error) {
                console.error('Error initializing collections chart:', error);
                collectionsChart = null;
            }
        }
    }
    
    // Initialize connections chart
    if (!connectionsChart) {
        const ctx = document.getElementById('connectionsChart');
        if (ctx) {
            try {
                // Destroy existing chart if canvas is already in use
                const existingChart = Chart.getChart(ctx);
                if (existingChart) {
                    existingChart.destroy();
                }
                connectionsChart = new Chart(ctx.getContext('2d'), {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Active Connections',
                        data: [],
                        borderColor: 'rgba(75, 192, 192, 1)',
                        backgroundColor: 'rgba(75, 192, 192, 0.1)',
                        borderWidth: 2,
                        tension: 0.4,
                        fill: true
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            display: true,
                            position: 'top'
                        }
                    },
                    scales: {
                        y: {
                            beginAtZero: true,
                            ticks: {
                                stepSize: 1
                            }
                        }
                    }
                }
            });
            } catch (error) {
                console.error('Error initializing connections chart:', error);
                connectionsChart = null;
            }
        }
    }
    
    // Initialize response times chart
    if (!responseTimesChart) {
        const ctx = document.getElementById('responseTimesChart');
        if (ctx) {
            try {
                // Destroy existing chart if canvas is already in use
                const existingChart = Chart.getChart(ctx);
                if (existingChart) {
                    existingChart.destroy();
                }
                responseTimesChart = new Chart(ctx.getContext('2d'), {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [
                        {
                            label: 'Average Response Time',
                            data: [],
                            borderColor: 'rgba(255, 99, 132, 1)',
                            backgroundColor: 'rgba(255, 99, 132, 0.1)',
                            borderWidth: 2,
                            fill: true,
                            tension: 0.4,
                            pointRadius: 3
                        }
                    ]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            display: true,
                            position: 'top'
                        },
                        tooltip: {
                            callbacks: {
                                label: function(context) {
                                    return `${context.dataset.label}: ${context.parsed.y.toFixed(2)} ms`;
                                }
                            }
                        }
                    },
                    scales: {
                        y: {
                            beginAtZero: true,
                            title: {
                                display: true,
                                text: 'Response Time (ms)'
                            }
                        }
                    }
                }
            });
            } catch (error) {
                console.error('Error initializing response times chart:', error);
                responseTimesChart = null;
            }
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
        
        // Load connections and response times
        await loadDashboardMetrics();
        
        // Load library statistics
        await loadLibraryStatistics();
        
        // Load welcome panel content
        if (!isPolling) {
            await loadWelcomePanel();
        }
        
        // Update collections chart only if data changed
        if (hasDataChanged(collectionsData)) {
            updateCollectionsChart(collectionsData.collections);
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
        console.log('Loading collections from unified documents API...');
        
        // Get virtual collections (document types) from unified API
        const collectionsResponse = await apiRequest('/api/collections');
        let collectionsData = Array.isArray(collectionsResponse) ? collectionsResponse : (collectionsResponse?.collections || []);
        
        console.log('Collections data from unified API:', collectionsData);
        
        // Also load libraries for library information
        if (libraries.length === 0) {
            try {
                const librariesResponse = await apiRequest('/api/libraries');
                let librariesData = Array.isArray(librariesResponse) ? librariesResponse : (librariesResponse?.libraries || []);
                libraries = librariesData;
                console.log('Libraries data:', librariesData);
            } catch (error) {
                console.warn('Failed to load libraries:', error);
                libraries = [{name: 'default'}]; // Fallback to default library
            }
        }
        
        console.log('Loaded collections from unified API:', collectionsData);
        
        // Normalize the collections data format for unified API
        collectionsData = collectionsData.map(item => {
            // Handle different response formats from unified API
            const collection = {
                name: item.name || item.collection || item.type || item,
                library: item.library || 'default',
                documentCount: item.document_count || item.documentCount || item.total_documents || item.count || 0,
                total_size: item.total_size || item.size || 0,
                isSystem: false
            };
            
            // Determine if it's a system collection
            if (typeof collection.name === 'string') {
                collection.isSystem = collection.name.startsWith('_') || 
                                    collection.name.startsWith('system_') || 
                                    collection.library === 'system';
            }
            
            console.log(`Collection ${collection.library}/${collection.name}: documentCount=${collection.documentCount}`);
            
            return collection;
        });
        
        // Update the global collections array
        collections = collectionsData;
        
        // Count system and user collections separately
        const systemCollections = collections.filter(c => {
            const name = typeof c === 'string' ? c : (c.name || '');
            return name.startsWith('_');
        });
        const userCollections = collections.filter(c => {
            const name = typeof c === 'string' ? c : (c.name || '');
            return !name.startsWith('_');
        });
        
        const totalCollections = collections.length;
        const userCollectionCount = userCollections.length;
        const systemCollectionCount = systemCollections.length;
        
        if (previousData.totalCollections !== totalCollections) {
            document.getElementById('statTotalCollections').textContent = totalCollections;
            document.getElementById('statUserCollections').textContent = userCollectionCount;
            document.getElementById('statSystemCollections').textContent = systemCollectionCount;
            previousData.totalCollections = totalCollections;
        }
        
        let totalDocuments = 0;
        let userDocuments = 0;
        let systemDocuments = 0;
        let totalSize = 0;
        
        for (const collectionInfo of collections) {
            // Handle both old format (string) and new format (object)
            const collectionName = typeof collectionInfo === 'string' ? collectionInfo : (collectionInfo?.name || 'unknown');
            
            // Check if it's a system collection
            const isSystemCollection = collectionName.startsWith('_');
            
            try {
                const docCountFromInfo = typeof collectionInfo === 'object' ? collectionInfo.documentCount : null;
                
                console.log(`Collection ${collectionName}: docCountFromInfo=${docCountFromInfo}, collectionInfo=`, collectionInfo);
                
                // If we already have the count from the API, use it for efficiency
                let docCount = docCountFromInfo !== null && docCountFromInfo !== undefined ? docCountFromInfo : 0;
                let size = typeof collectionInfo === 'object' ? (collectionInfo.total_size || 0) : 0;
                
                // Only fetch documents if we specifically need to calculate size and don't have it
                if (docCount > 0 && size === 0) {
                    // For now, estimate size based on document count
                    // This avoids fetching all documents just for size calculation
                    size = docCount * 100; // Rough estimate: 100 bytes per document
                }
                
                // Count documents for both system and user collections
                totalDocuments += docCount;
                if (isSystemCollection) {
                    systemDocuments += docCount;
                } else {
                    userDocuments += docCount;
                }
                totalSize += size;
                
                const collectionKey = `${collectionName}_${docCount}_${size}`;
                previousData.collectionsData[collectionName] = collectionKey;
                
            } catch (error) {
                console.error(`Error loading collection ${collectionName}:`, error);
            }
        }
        
        // Update document count display
        if (previousData.totalDocuments !== totalDocuments || 
            document.getElementById('totalDocuments')?.textContent === '0') {
            const totalDocsEl = document.getElementById('totalDocuments');
            const userDocsEl = document.getElementById('userDocuments');
            const systemDocsEl = document.getElementById('systemDocuments');
            
            if (totalDocsEl) totalDocsEl.textContent = formatNumber(totalDocuments);
            if (userDocsEl) userDocsEl.textContent = formatNumber(userDocuments);
            if (systemDocsEl) systemDocsEl.textContent = formatNumber(systemDocuments);
            
            previousData.totalDocuments = totalDocuments;
        }
        
        // Use unified database size calculation
        const actualSize = await getActualDatabaseSize();
        if (previousData.databaseSize !== actualSize) {
            document.getElementById('statDatabaseSize').textContent = formatSize(actualSize);
            previousData.databaseSize = actualSize;
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
            // Handle both old and new formats
            const memory = health.memory || health.mem || {};
            const memoryUsagePercent = memory.used_kb && memory.total_kb ? 
                ((memory.used_kb / memory.total_kb) * 100).toFixed(1) : '0';
            const loadPercent = health.load_average ? health.load_average.toFixed(1) : '0';
            
            document.getElementById('cpuUsage').textContent = `${loadPercent}%`;
            document.getElementById('memoryUsage').textContent = `${memoryUsagePercent}%`;
            
            // Show latency
            let apiLatency = 'N/A';
            if (health.metrics && health.metrics.performance && health.metrics.performance.avg_response_time_ms) {
                apiLatency = `${health.metrics.performance.avg_response_time_ms.toFixed(2)}ms`;
            } else if (health.latency_us) {
                apiLatency = `${(health.latency_us / 1000).toFixed(2)}ms`;
            }
            document.getElementById('apiLatency').textContent = apiLatency;
            
            // Calculate uptime from seconds
            if (health.uptime_seconds) {
                const days = Math.floor(health.uptime_seconds / 86400);
                const hours = Math.floor((health.uptime_seconds % 86400) / 3600);
                const minutes = Math.floor((health.uptime_seconds % 3600) / 60);
                document.getElementById('uptime').textContent = `${days}d ${hours}h ${minutes}m`;
            } else if (health.up) {
                const days = Math.floor(health.up / 86400);
                const hours = Math.floor((health.up % 86400) / 3600);
                const minutes = Math.floor((health.up % 3600) / 60);
                document.getElementById('uptime').textContent = `${days}d ${hours}h ${minutes}m`;
            } else {
                document.getElementById('uptime').textContent = 'N/A';
            }
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

function updateCollectionsChart(collections) {
    if (!collectionsChart || !collections) return;
    
    const labels = [];
    const data = [];
    const backgroundColors = [];
    
    // Sort collections by document count (descending) and take top 10
    const sortedCollections = [];
    
    // Process the collections array
    for (const collection of collections) {
        const collectionName = collection.name;
        const docCount = collection.documentCount || 0;
        
        // Include all collections with documents
        if (docCount > 0) {
            sortedCollections.push({ name: collectionName, count: docCount });
        }
    }
    
    // Sort by count descending and take top 10
    sortedCollections.sort((a, b) => b.count - a.count);
    const topCollections = sortedCollections.slice(0, 10);
    
    // Create chart data
    topCollections.forEach((col, index) => {
        labels.push(col.name);
        data.push(col.count);
    });
    
    // Update chart
    try {
        collectionsChart.data.labels = labels;
        if (collectionsChart.data.datasets && collectionsChart.data.datasets[0]) {
            collectionsChart.data.datasets[0].data = data;
        }
        collectionsChart.update();
    } catch (error) {
        console.error('Error updating collections chart:', error);
    }
}

async function loadMetricsData() {
    try {
        const response = await apiRequest('/api/metrics/history');
        if (response && response.metrics) {
            updateConnectionsChart(response.metrics.connections);
            updateResponseTimesChart(response.metrics.performance);
        }
    } catch (error) {
        console.error('Error loading metrics data:', error);
    }
}

function updateConnectionsChart(connectionsData) {
    if (!connectionsChart || !connectionsData || !connectionsData.data || connectionsData.data.length === 0) return;
    
    const labels = [];
    const data = [];
    
    // Get the last 10 data points
    const recentData = connectionsData.data.slice(-10);
    
    recentData.forEach(point => {
        const date = new Date(point.timestamp);
        labels.push(date.toLocaleTimeString());
        data.push(point.active || point.active_connections || 0);
    });
    
    try {
        connectionsChart.data.labels = labels;
        if (connectionsChart.data.datasets && connectionsChart.data.datasets[0]) {
            connectionsChart.data.datasets[0].data = data;
        }
        connectionsChart.update();
    } catch (error) {
        console.error('Error updating connections chart:', error);
    }
}

function updateResponseTimesChart(performanceData) {
    if (!responseTimesChart || !performanceData || !performanceData.data || performanceData.data.length === 0) return;
    
    const labels = [];
    const avgData = [];
    
    // Get the last 10 data points
    const recentData = performanceData.data.slice(-10);
    
    recentData.forEach(point => {
        const date = new Date(point.timestamp);
        labels.push(date.toLocaleTimeString());
        avgData.push(point.avg_response_time_ms || 0);
    });
    
    try {
        responseTimesChart.data.labels = labels;
        if (responseTimesChart.data.datasets && responseTimesChart.data.datasets[0]) {
            responseTimesChart.data.datasets[0].data = avgData; // Average response time
        }
        responseTimesChart.update();
    } catch (error) {
        console.error('Error updating response times chart:', error);
    }
}

async function loadLibraryStatistics() {
    try {
        // Get all libraries
        const librariesResponse = await apiRequest('/api/libraries');
        const libraries = librariesResponse.libraries || [];
        
        const statsPanel = document.getElementById('libraryStatsPanel');
        if (!statsPanel) return;
        
        let statsHtml = '';
        
        // Process each library
        for (const library of libraries) {
            try {
                // Get library stats
                const statsResponse = await apiRequest(`/api/libraries/${library.name}/stats`);
                const stats = statsResponse.stats || {};
                
                // Create library stat card
                statsHtml += `
                    <div class="col-md-4 col-lg-3 mb-3">
                        <div class="card h-100 ${library.name === currentLibrary ? 'border-primary' : ''}">
                            <div class="card-body">
                                <h6 class="card-title d-flex justify-content-between align-items-center">
                                    <span>${library.name}</span>
                                    ${library.name === 'system' ? '<span class="badge bg-secondary">System</span>' : ''}
                                    ${library.name === currentLibrary ? '<span class="badge bg-primary">Current</span>' : ''}
                                </h6>
                                <div class="small">
                                    <div class="d-flex justify-content-between mb-1">
                                        <span class="text-muted">Collections:</span>
                                        <span class="fw-bold">${stats.total_collections || 0}</span>
                                    </div>
                                    <div class="d-flex justify-content-between mb-1">
                                        <span class="text-muted">Documents:</span>
                                        <span class="fw-bold">${stats.total_documents || 0}</span>
                                    </div>
                                    <div class="d-flex justify-content-between mb-1">
                                        <span class="text-muted">Size:</span>
                                        <span class="fw-bold">${formatBytes(stats.total_size || 0)}</span>
                                    </div>
                                    <div class="d-flex justify-content-between">
                                        <span class="text-muted">Created:</span>
                                        <span class="fw-bold">${formatDate(library.created_at)}</span>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                `;
            } catch (error) {
                console.error(`Failed to load stats for library ${library.name}:`, error);
                // Add placeholder card for libraries with errors
                statsHtml += `
                    <div class="col-md-4 col-lg-3 mb-3">
                        <div class="card h-100 ${library.name === currentLibrary ? 'border-primary' : ''}">
                            <div class="card-body">
                                <h6 class="card-title">${library.name}</h6>
                                <div class="text-muted small">Unable to load statistics</div>
                            </div>
                        </div>
                    </div>
                `;
            }
        }
        
        // Update the panel
        statsPanel.innerHTML = statsHtml || '<div class="col-12 text-center text-muted">No libraries found</div>';
        
    } catch (error) {
        console.error('Error loading library statistics:', error);
        const statsPanel = document.getElementById('libraryStatsPanel');
        if (statsPanel) {
            statsPanel.innerHTML = '<div class="col-12 text-center text-danger">Failed to load library statistics</div>';
        }
    }
}

// Helper function to format bytes
function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// Helper function to format date
function formatDate(dateString) {
    if (!dateString) return 'N/A';
    const date = new Date(dateString);
    return date.toLocaleDateString();
}

async function loadDashboardMetrics() {
    try {
        // Fetch metrics data from _metrics collection
        const metricsData = await apiRequest('/api/collections/_metrics/documents').catch(err => {
            console.error('Failed to fetch metrics data:', err);
            return { documents: [] };
        });
        
        console.log('Dashboard metrics data:', metricsData);
        
        // Extract metrics documents by type
        // Deduplicate documents by ID (temporary fix for server bug)
        const seenIds = new Set();
        const uniqueDocuments = [];
        for (const doc of (metricsData.documents || [])) {
            if (!seenIds.has(doc.uuid || doc._id)) {
                seenIds.add(doc.uuid || doc._id);
                uniqueDocuments.push(doc);
            }
        }
        
        const metricsDocuments = uniqueDocuments;
        const performanceDoc = metricsDocuments.find(doc => doc.type === 'performance');
        const connectionsDoc = metricsDocuments.find(doc => doc.type === 'connections');
        
        // Update charts with the data
        if (connectionsDoc && connectionsDoc.data && connectionsDoc.data.length > 0) {
            updateConnectionsChart(connectionsDoc);
        }
        
        if (performanceDoc && performanceDoc.data && performanceDoc.data.length > 0) {
            updateResponseTimesChart(performanceDoc);
        }
    } catch (error) {
        console.error('Error loading dashboard metrics:', error);
    }
}

// ===== JAVASCRIPT SCRIPT INTEGRATION =====

// ===== BROWSER FUNCTIONALITY =====
async function initializeBrowser() {
    try {
        await loadLibraries();  // Load libraries first
        // Collections are loaded within loadLibraries, no need to call again
    } catch (error) {
        console.error('Error initializing browser:', error);
        showNotification('Failed to initialize browser', 'error');
    }
}

// Load libraries from unified documents
async function loadLibraries() {
    try {
        // Use the correct libraries API endpoint that the server provides
        const response = await apiRequest('/api/libraries');
        console.log('Libraries API response:', response);
        
        if (response && response.libraries) {
            libraries = response.libraries.map(lib => ({
                name: lib.name,
                description: lib.description || `${lib.name} library`,
                quotas: lib.quotas || {},
                settings: lib.settings || {},
                created_at: lib.created_at,
                template: lib.template || 'standard'
            }));
            
            console.log('Processed libraries:', libraries);
            
            // Load collections first, then render library selector with counts
            await loadBrowserCollections();
            renderLibrarySelector();
        } else {
            console.warn('Invalid libraries response format:', response);
            throw new Error('Invalid response format');
        }
    } catch (error) {
        console.error('Error loading libraries:', error);
        // Fallback to default libraries
        libraries = [
            { name: 'default', description: 'Default library for general use' },
            { name: 'system', description: 'System library for internal operations' }
        ];
        console.log('Using fallback libraries:', libraries);
        renderLibrarySelector();
    }
}

// Render library selector in the navigation bar
function renderLibrarySelector() {
    const selector = document.getElementById('globalLibrarySelector');
    if (!selector) return;
    
    // Update the selector options with collection counts
    selector.innerHTML = libraries.map(lib => {
        const libCollections = allCollections.filter(c => c.library === lib.name);
        const collectionCount = libCollections.length;
        console.log(`Library ${lib.name} has ${collectionCount} collections:`, libCollections);
        return `<option value="${lib.name}" ${lib.name === currentLibrary ? 'selected' : ''}>
            ${lib.name} (${collectionCount} collections)
        </option>`;
    }).join('');
    
    // Add onchange handler
    selector.onchange = function() {
        switchLibrary(this.value);
    };
}

// Show library management modal
async function showLibraryManager() {
    // Show library management modal
    const modal = document.createElement('div');
    modal.className = 'modal fade show';
    modal.style.display = 'block';
    modal.style.backgroundColor = 'rgba(0,0,0,0.5)';
    
    modal.innerHTML = `
        <div class="modal-dialog">
            <div class="modal-content">
                <div class="modal-header">
                    <h5 class="modal-title">Library Management</h5>
                    <button type="button" class="btn-close" onclick="this.closest('.modal').remove()"></button>
                </div>
                <div class="modal-body">
                    <div class="d-grid gap-2">
                        <button class="btn btn-primary" onclick="createNewLibrary(); this.closest('.modal').remove();">
                            <i class="bi bi-plus-circle me-2"></i>Create New Library
                        </button>
                        <button class="btn btn-danger" onclick="deleteLibrary(); this.closest('.modal').remove();">
                            <i class="bi bi-trash me-2"></i>Delete Current Library
                        </button>
                    </div>
                    <hr>
                    <h6>Existing Libraries:</h6>
                    <ul class="list-group">
                        ${libraries.map(lib => `
                            <li class="list-group-item d-flex justify-content-between align-items-center">
                                <span>${lib.name} ${lib.name === currentLibrary ? '<span class="badge bg-primary ms-2">Current</span>' : ''}</span>
                                ${lib.name !== 'system' && lib.name !== 'default' ? 
                                    `<button class="btn btn-sm btn-outline-danger" onclick="deleteSpecificLibrary('${lib.name}'); this.closest('.modal').remove();">
                                        <i class="bi bi-trash"></i>
                                    </button>` : 
                                    '<span class="text-muted small">Protected</span>'
                                }
                            </li>
                        `).join('')}
                    </ul>
                </div>
            </div>
        </div>
    `;
    
    document.body.appendChild(modal);
}

async function createNewLibrary() {
    
    const libraryName = prompt('Enter library name:');
    if (!libraryName) return;
    
    // Validate library name
    if (!/^[a-zA-Z_][a-zA-Z0-9_]*$/.test(libraryName)) {
        showNotification('Invalid library name. Use only letters, numbers, and underscores.', 'error');
        return;
    }
    
    // Check if library already exists
    if (libraries.find(lib => lib.name === libraryName)) {
        showNotification('Library already exists', 'warning');
        return;
    }
    
    try {
        // Create library metadata in unified documents
        const libraryMetadata = {
            type: 'library',
            name: libraryName,
            description: prompt('Enter library description (optional):') || `Library ${libraryName}`,
            quotas: {
                max_collections: 100,
                max_documents: 100000,
                max_size_mb: 1024
            },
            settings: {
                default_permissions: {
                    owner_perms: 'rwxda',
                    world_perms: 'r'
                }
            },
            created_at: new Date().toISOString()
        };
        
        const response = await apiRequest('/api/collections/documents/documents', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(libraryMetadata)
        });
        
        if (response.success || response.id) {
            showNotification(`Library "${libraryName}" created successfully`, 'success');
            
            // Reload libraries and switch to the new one
            await loadLibraries();
            switchLibrary(libraryName);
        }
    } catch (error) {
        console.error('Error creating library:', error);
        showNotification('Failed to create library', 'error');
    }
}

// Delete current library
async function deleteLibrary() {
    if (currentLibrary === 'system' || currentLibrary === 'default') {
        showNotification('Cannot delete system or default library', 'error');
        return;
    }
    
    if (!confirm(`Are you sure you want to delete the library "${currentLibrary}"? This action cannot be undone.`)) {
        return;
    }
    
    try {
        const response = await apiRequest(`/api/libraries/${currentLibrary}`, {
            method: 'DELETE'
        });
        
        if (response.success) {
            showNotification(`Library "${currentLibrary}" deleted successfully`, 'success');
            
            // Switch to default library
            await loadLibraries();
            switchLibrary('default');
        }
    } catch (error) {
        console.error('Error deleting library:', error);
        showNotification('Failed to delete library', 'error');
    }
}

// Delete specific library
async function deleteSpecificLibrary(libraryName) {
    if (libraryName === 'system' || libraryName === 'default') {
        showNotification('Cannot delete system or default library', 'error');
        return;
    }
    
    if (!confirm(`Are you sure you want to delete the library "${libraryName}"? This action cannot be undone.`)) {
        return;
    }
    
    try {
        const response = await apiRequest(`/api/libraries/${libraryName}`, {
            method: 'DELETE'
        });
        
        if (response.success) {
            showNotification(`Library "${libraryName}" deleted successfully`, 'success');
            
            // Reload libraries
            await loadLibraries();
            
            // If we deleted the current library, switch to default
            if (libraryName === currentLibrary) {
                switchLibrary('default');
            }
        }
    } catch (error) {
        console.error('Error deleting library:', error);
        showNotification('Failed to delete library', 'error');
    }
}

// Switch to a different library
async function switchLibrary(libraryName) {
    currentLibrary = libraryName;
    currentCollection = null;
    currentDocument = null;
    
    // Update global selector
    const globalSelector = document.getElementById('globalLibrarySelector');
    if (globalSelector && globalSelector.value !== libraryName) {
        globalSelector.value = libraryName;
    }
    
    // Clear panels if in browser view
    if (currentView === 'browser') {
        document.getElementById('documentsList').innerHTML = `
            <div class="empty-state">
                <i class="bi bi-folder2-open"></i>
                <p>Select a collection to view documents</p>
            </div>
        `;
        
        document.getElementById('contentViewer').innerHTML = `
            <div class="empty-state">
                <i class="bi bi-file-earmark-text"></i>
                <p>Select a document to view its content</p>
            </div>
        `;
        
        // Filter and render collections for the selected library
        filterCollectionsByLibrary();
        renderCollections();
    }
    
    // Reload current view data with library context
    switch(currentView) {
        case 'dashboard':
            loadDashboard();
            break;
        case 'metrics':
            if (window.loadMetrics) {
                loadMetrics();
            }
            break;
        case 'rbac':
            if (window.loadRBAC) {
                loadRBAC();
            }
            break;
    }
}

// Store all collections globally for library filtering
let allCollections = [];

async function loadBrowserCollections() {
    try {
        // In unified architecture, we get all collections from /api/collections
        // and they already include library information
        const collectionsResponse = await apiRequest('/api/collections');
        console.log('Collections API response:', collectionsResponse);
        
        // Handle the response format
        let rawCollections = [];
        if (Array.isArray(collectionsResponse)) {
            rawCollections = collectionsResponse;
        } else if (collectionsResponse && collectionsResponse.collections) {
            rawCollections = collectionsResponse.collections;
        } else {
            console.warn('No collections found in response:', collectionsResponse);
            rawCollections = [];
        }
        
        // Map to our format - collections already include library info from server
        allCollections = rawCollections.map(item => {
            if (typeof item === 'string') {
                // Legacy string format: "library/collection"
                const parts = item.split('/');
                const library = parts.length > 1 ? parts[0] : 'default';
                const name = parts.length > 1 ? parts[1] : parts[0];
                return { 
                    name: name, 
                    library: library,
                    fullPath: item,
                    documentCount: 0, 
                    isSystem: name.startsWith('_') || name.startsWith('system_')
                };
            } else {
                // Object format with properties - server provides library field
                const name = item.name || '';
                const library = item.library || 'default';
                const fullPath = `${library}/${name}`;
                return {
                    name: name,
                    library: library,
                    fullPath: fullPath,
                    documentCount: item.document_count || 0,
                    isSystem: item.is_system || name.startsWith('_') || name.startsWith('system_')
                };
            }
        });
        
        console.log('Processed collections:', allCollections);
        
        // Load schemas separately
        try {
            const schemasResponse = await apiRequest('/api/schemas');
            if (schemasResponse && schemasResponse.schemas) {
                schemas = schemasResponse.schemas;
            } else if (Array.isArray(schemasResponse)) {
                schemas = schemasResponse;
            } else {
                schemas = [];
            }
        } catch (error) {
            console.warn('Failed to load schemas:', error);
            schemas = [];
        }
        
        // Filter collections for current library
        filterCollectionsByLibrary();
        
        // Schemas already loaded above, no need to reference undefined schemasResponse
        
        await renderCollections();
    } catch (error) {
        console.error('Error loading collections:', error);
        document.getElementById('collectionsList').innerHTML = `
            <div class="text-center text-muted p-4">
                <i class="bi bi-exclamation-circle" style="font-size: 2rem;"></i>
                <p>Failed to load collections</p>
                <small class="text-muted">${error.message}</small>
            </div>
        `;
    }
}

// Filter collections by current library
function filterCollectionsByLibrary() {
    console.log('Filtering collections for library:', currentLibrary);
    console.log('All collections before filtering:', allCollections);
    collections = allCollections.filter(item => item.library === currentLibrary);
    console.log('Filtered collections:', collections);
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
                <p>No collections found in "${currentLibrary}" library</p>
                <small class="text-muted">
                    Try switching to another library or creating a new collection
                </small>
            </div>
        `;
        return;
    }
    
    // Separate system collections (starting with _) from user collections
    // Handle both old format (strings) and new format (objects)
    const systemCollections = collections.filter(c => {
        const name = typeof c === 'string' ? c : c.name;
        return name.startsWith('_');
    });
    const userCollections = collections.filter(c => {
        const name = typeof c === 'string' ? c : c.name;
        return !name.startsWith('_');
    });
    
    let html = '';
    
    // Add system collections under a header
    if (systemCollections.length > 0) {
        html += `
            <div class="collection-group">
                <div class="collection-group-header">
                    <i class="bi bi-database me-2"></i>
                    <span>System Collections</span>
                </div>
                ${systemCollections.map(collectionInfo => {
                    const name = typeof collectionInfo === 'string' ? collectionInfo : collectionInfo.name;
                    const count = typeof collectionInfo === 'object' ? collectionInfo.documentCount : 0;
                    const hasSchema = schemas && schemas.some(s => s.collection === name);
                    const hasJavaScript = name === '_validators' || name === '_transformers' || name === '_functions';
                    return `
                        <div class="collection-item ${currentCollection === name ? 'active' : ''}" 
                             data-collection="${name}" 
                             onclick="selectCollection('${name}')">
                            <i class="bi bi-gear-fill me-2" style="font-size: 0.875rem;"></i>
                            <span class="collection-name">${name}</span>
                            ${hasSchema ? '<i class="bi bi-shield-check text-success ms-1" title="Schema defined"></i>' : ''}
                            ${hasJavaScript ? '<i class="bi bi-code-slash text-warning ms-1" title="Contains JavaScript Scripts"></i>' : ''}
                            <span class="badge bg-secondary ms-auto">${count}</span>
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
                ${userCollections.map(collectionInfo => {
                    const name = typeof collectionInfo === 'string' ? collectionInfo : collectionInfo.name;
                    const count = typeof collectionInfo === 'object' ? collectionInfo.documentCount : 0;
                    const hasSchema = schemas && schemas.some(s => s.collection === name);
                    return `
                        <div class="collection-item ${currentCollection === name ? 'active' : ''}" 
                             onclick="selectCollection('${name}')">
                            <i class="bi bi-folder me-2" style="font-size: 0.875rem;"></i>
                            <span class="collection-name">${name}</span>
                            ${hasSchema ? '<i class="bi bi-shield-check text-success ms-1" title="Schema defined"></i>' : ''}
                            <span class="badge bg-secondary ms-auto">${count}</span>
                        </div>
                    `;
                }).join('')}
            </div>
        `;
    }
    
    container.innerHTML = html;
    
    // Update document counts only if we have collections without counts
    const needsCountUpdate = collections.some(c => {
        const count = typeof c === 'object' ? c.documentCount : null;
        return count === undefined || count === null;
    });
    
    if (needsCountUpdate) {
        updateCollectionCounts();
    }
}

async function updateCollectionCounts() {
    // Update counts for each collection with rate limiting to prevent connection overload
    for (let i = 0; i < collections.length; i++) {
        const collection = collections[i];
        try {
            const collectionName = typeof collection === 'string' ? collection : collection.name;
            // Use the full path which includes library prefix
            const collectionPath = collection.fullPath || `${currentLibrary}/${collectionName}`;
            console.log(`Requesting count for collection: ${collectionPath} (${i+1}/${collections.length})`);
            const response = await apiRequest(`/api/collections/${collectionPath}/documents`);
            console.log(`Received response for ${collectionPath}:`, response ? 'success' : 'null');
            const count = response && response.documents ? response.documents.length : 0;
            
            // Check if count has changed before updating DOM (prevent flicker)
            const countKey = `${collectionName}_count`;
            if (previousData.collectionsData[countKey] !== count) {
                // Update the collections array with the real count
                if (typeof collection === 'object') {
                    collection.documentCount = count;
                } else {
                    // If it's a string, find and update in the collections array
                    for (let j = 0; j < collections.length; j++) {
                        if (typeof collections[j] === 'string' && collections[j] === collectionName) {
                            collections[j] = { name: collectionName, documentCount: count, isSystem: collectionName.startsWith('_') };
                            break;
                        } else if (typeof collections[j] === 'object' && collections[j].name === collectionName) {
                            collections[j].documentCount = count;
                            break;
                        }
                    }
                }
                
                // Find the badge for this collection and update it only if count changed
                const collectionItems = document.querySelectorAll('.collection-item');
                collectionItems.forEach(item => {
                    if (item.querySelector('.collection-name')?.textContent === collectionName) {
                        const badge = item.querySelector('.badge');
                        if (badge) {
                            badge.textContent = count.toString();
                            console.log(`Updated count for ${collectionName}: ${previousData.collectionsData[countKey]} → ${count}`);
                        }
                    }
                });
                
                // Store the new count for future comparison
                previousData.collectionsData[countKey] = count;
            } else {
                console.log(`No change in count for ${collectionName}: ${count}`);
            }
            
            // Add small delay between requests to prevent connection overload
            if (i < collections.length - 1) {
                await new Promise(resolve => setTimeout(resolve, 50));
            }
        } catch (error) {
            const collectionName = typeof collection === 'string' ? collection : collection.name;
            console.error(`Error getting count for ${collectionName}:`, error);
            // Continue with other collections even if one fails
        }
    }
}

function updateCollectionActiveState() {
    // Update active state for collections without full re-render
    const collectionItems = document.querySelectorAll('.collection-item');
    collectionItems.forEach(item => {
        const collectionName = item.querySelector('.collection-name')?.textContent;
        if (collectionName === currentCollection) {
            item.classList.add('active');
        } else {
            item.classList.remove('active');
        }
    });
}

async function selectCollection(collectionName) {
    currentCollection = collectionName;
    currentDocument = null;
    
    // Update active state without full re-render to prevent count flicker
    updateCollectionActiveState();
    
    // Find the full path for this collection
    const collection = collections.find(c => c.name === collectionName);
    const collectionPath = collection?.fullPath || `${currentLibrary}/${collectionName}`;
    
    // Load documents
    await loadDocuments(collectionPath);
    
    // Setup polling for this collection
    if (currentView === 'browser' && POLLING_INTERVALS.browser) {
        if (refreshInterval) {
            clearInterval(refreshInterval);
        }
        refreshInterval = setInterval(() => {
            if (currentCollection === collectionName && currentView === 'browser') {
                loadDocuments(collectionPath, true);
            }
        }, POLLING_INTERVALS.browser);
    }
}

async function loadDocuments(collectionPath, isPolling = false) {
    try {
        // Parse library/collection path
        const parts = collectionPath.split('/');
        const library = parts[0] || 'default';
        const collection = parts[1] || parts[0];
        
        // Use the unified documents API with query filters for library and type
        // In unified architecture, collection name corresponds to document type
        const query = {
            library: library,
            type: collection
        };
        const response = await apiRequest(`/api/documents?query=${encodeURIComponent(JSON.stringify(query))}`);
        // Handle both array response and object with documents property
        const newDocuments = Array.isArray(response) ? response : (response.documents || []);
        
        // Check if documents have changed
        const hasChanged = JSON.stringify(documents) !== JSON.stringify(newDocuments);
        
        if (!isPolling || hasChanged) {
            documents = newDocuments;
            renderDocuments();
            
            // Update document count
            document.getElementById('documentCount').textContent = documents.length;
            
            // Update panel title to show library/collection
            const panelTitle = document.getElementById('panelTitle');
            if (panelTitle && !queryBuilderVisible) {
                panelTitle.textContent = `${currentLibrary}/${currentCollection}`;
            }
            
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

// Helper function to truncate UUIDs
function truncateUuid(uuid, maxLength = 10) {
    if (!uuid || uuid.length <= maxLength) return uuid;
    // Check if it's a UUID format (with dashes)
    if (uuid.match(/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i)) {
        return uuid.substring(0, maxLength) + '...';
    }
    return uuid;
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
        const docId = doc.uuid || doc._id || doc.id || `Document ${index + 1}`;
        const docName = doc.name || docId; // Use name if available, otherwise fallback to ID
        const docSize = JSON.stringify(doc).length;
        const sizeStr = docSize < 1024 ? `${docSize} B` : `${(docSize / 1024).toFixed(1)} KB`;
        const createdAt = doc.created_at || '';
        const updatedAt = doc.updated_at || '';
        
        // Format the display based on whether we have a custom name
        const hasCustomName = doc.name && doc.name !== docId;
        
        // Truncate UUID for display
        const displayId = truncateUuid(docId);
        
        return `
            <div class="document-item ${currentDocument === (doc.uuid || doc._id || doc.id) ? 'active' : ''}" 
                 onclick="selectDocument(${index})" title="${docId}">
                <div class="d-flex align-items-center w-100">
                    <i class="bi bi-file-text me-2" style="font-size: 0.875rem;"></i>
                    <div class="flex-grow-1">
                        ${hasCustomName ? `
                            <div class="document-name fw-bold">${docName}</div>
                            <div class="document-id text-muted small">${displayId}</div>
                        ` : `
                            <div class="document-id text-muted small">${displayId}</div>
                        `}
                        ${createdAt || updatedAt ? `
                            <div class="document-dates text-muted small" style="font-size: 0.75rem;">
                                ${createdAt ? `<span title="Created"><i class="bi bi-plus-circle" style="font-size: 0.7rem;"></i> ${new Date(createdAt).toLocaleDateString()}</span>` : ''}
                                ${updatedAt && updatedAt !== createdAt ? `<span title="Updated" class="ms-2"><i class="bi bi-pencil" style="font-size: 0.7rem;"></i> ${new Date(updatedAt).toLocaleDateString()}</span>` : ''}
                            </div>
                        ` : ''}
                    </div>
                    <span class="badge bg-secondary ms-2">${sizeStr}</span>
                </div>
            </div>
        `;
    }).join('');
}

let currentDocumentIndex = null;
let originalDocumentContent = null;
let isDocumentModified = false;
let transformPreviewMode = false;
let currentTransformers = [];
let transformedContent = null;

function selectDocument(index) {
    const doc = documents[index];
    if (!doc) return;
    
    currentDocument = doc.uuid || doc._id || doc.id;
    currentDocumentIndex = index;
    originalDocumentContent = JSON.stringify(doc, null, 2);
    isDocumentModified = false;
    renderDocuments();
    
    // Update title with truncated UUID
    const fullId = currentDocument || `Document ${index + 1}`;
    const displayTitle = doc.name || truncateUuid(fullId, 15);
    document.getElementById('documentTitle').textContent = displayTitle;
    document.getElementById('documentTitle').title = fullId; // Show full ID on hover
    
    // Show editable document content with line numbers
    const lines = originalDocumentContent.split('\n');
    const lineNumbers = lines.map((_, i) => `<span class="line-number">${i + 1}</span>`).join('\n');
    
    document.getElementById('contentViewer').innerHTML = `
        <div class="json-editor-container">
            <div class="line-numbers">
                ${lineNumbers}
            </div>
            <textarea id="documentEditor" class="json-editor" spellcheck="false">${originalDocumentContent}</textarea>
            <div id="validationFeedback" class="validation-feedback">
                <div class="validation-status">
                    <span class="validation-icon"><i class="bi bi-check-circle-fill text-success"></i></span>
                    <span class="validation-message">Document is valid</span>
                </div>
                <div class="validation-details" id="validationDetails"></div>
            </div>
        </div>
    `;
    
    // Set up editor event listener
    const editor = document.getElementById('documentEditor');
    editor.addEventListener('input', handleDocumentEdit);
    editor.addEventListener('scroll', syncScroll);
    
    // Initialize validation
    setTimeout(() => validateDocumentRealtime(originalDocumentContent), 100);
    
    // Check for transformers and update buttons
    checkTransformersAvailable().then(() => {
        updateDocumentButtons();
        
        // Update version controls for JavaScript scripts
        updateVersionControls();
    });
}

// ===== QUERY BUILDER FUNCTIONALITY =====
// queryBuilderVisible already declared at top of file

function toggleQueryBuilder() {
    queryBuilderVisible = !queryBuilderVisible;
    const queryBuilder = document.getElementById('queryBuilder');
    const searchBox = document.getElementById('documentsSearchBox');
    const panelTitle = document.getElementById('panelTitle');
    
    if (queryBuilderVisible) {
        queryBuilder.style.display = 'block';
        searchBox.style.display = 'none';
        panelTitle.textContent = 'Query Builder';
        updateQueryPreview();
    } else {
        queryBuilder.style.display = 'none';
        searchBox.style.display = 'block';
        panelTitle.textContent = 'Documents';
    }
}

function updateQueryBuilder() {
    const queryType = document.getElementById('queryType').value;
    
    // Hide all sections
    document.querySelectorAll('.query-section').forEach(section => {
        section.style.display = 'none';
    });
    
    // Show selected section
    document.getElementById(queryType + 'Query').style.display = 'block';
    
    // Update preview
    updateQueryPreview();
}

function updateQueryPreview() {
    const queryType = document.getElementById('queryType').value;
    let query = {};
    
    switch (queryType) {
        case 'simple':
            const field = document.getElementById('simpleField').value;
            const value = document.getElementById('simpleValue').value;
            if (field) {
                query[field] = value;
            }
            break;
            
        case 'comparison':
            const compField = document.getElementById('compField').value;
            const compOp = document.getElementById('compOperator').value;
            const compValue = document.getElementById('compValue').value;
            if (compField) {
                query[compField] = { [compOp]: compValue };
            }
            break;
            
        case 'logical':
            const logOp = document.getElementById('logicalOperator').value;
            const conditions = [];
            document.querySelectorAll('#logicalConditions .condition').forEach(cond => {
                const inputs = cond.querySelectorAll('input');
                if (inputs[0].value) {
                    conditions.push({ [inputs[0].value]: inputs[1].value });
                }
            });
            if (conditions.length > 0) {
                query[logOp] = conditions;
            }
            break;
            
        case 'custom':
            const customJSON = document.getElementById('customJSON').value;
            const parseResult = parseJSONSafely(customJSON, 'query JSON');
            query = parseResult.success ? parseResult.data : {};
            break;
    }
    
    document.getElementById('queryPreview').textContent = JSON.stringify(query, null, 2);
}

function addCondition() {
    const container = document.getElementById('logicalConditions');
    const newCondition = document.createElement('div');
    newCondition.className = 'condition mb-2';
    newCondition.innerHTML = `
        <div class="input-group input-group-sm">
            <input type="text" class="form-control" placeholder="Field" onkeyup="updateQueryPreview()">
            <input type="text" class="form-control" placeholder="Value" onkeyup="updateQueryPreview()">
            <button class="btn btn-outline-danger" type="button" onclick="removeCondition(this)">
                <i class="bi bi-x"></i>
            </button>
        </div>
    `;
    container.appendChild(newCondition);
    updateQueryPreview();
}

function removeCondition(button) {
    button.closest('.condition').remove();
    updateQueryPreview();
}

async function executeQuery() {
    if (!currentCollection) {
        showNotification('Please select a collection first', 'warning');
        return;
    }
    
    const queryType = document.getElementById('queryType').value;
    let query = {};
    
    // Build query based on type (same as updateQueryPreview)
    switch (queryType) {
        case 'simple':
            const field = document.getElementById('simpleField').value;
            const value = document.getElementById('simpleValue').value;
            if (field) {
                query[field] = value;
            }
            break;
            
        case 'comparison':
            const compField = document.getElementById('compField').value;
            const compOp = document.getElementById('compOperator').value;
            const compValue = document.getElementById('compValue').value;
            if (compField) {
                query[compField] = { [compOp]: compValue };
            }
            break;
            
        case 'logical':
            const logOp = document.getElementById('logicalOperator').value;
            const conditions = [];
            document.querySelectorAll('#logicalConditions .condition').forEach(cond => {
                const inputs = cond.querySelectorAll('input');
                if (inputs[0].value) {
                    conditions.push({ [inputs[0].value]: inputs[1].value });
                }
            });
            if (conditions.length > 0) {
                query[logOp] = conditions;
            }
            break;
            
        case 'custom':
            const customJSON = document.getElementById('customJSON').value;
            const parseResult = parseJSONSafely(customJSON, 'query JSON');
            if (!parseResult.success) {
                return;
            }
            query = parseResult.data;
            break;
    }
    
    try {
        // Use the query endpoint with the query parameter
        const response = await apiRequest(`/api/collections/${currentCollection}/query`, {
            method: 'POST',
            body: JSON.stringify({ query: query })
        });
        
        // Update documents with query results
        if (response.documents) {
            documents = response.documents;
        } else if (Array.isArray(response)) {
            documents = response;
        } else {
            documents = [];
        }
        
        // Update UI
        document.getElementById('documentCount').textContent = documents.length;
        renderDocuments();
        
        showNotification(`Found ${documents.length} documents`, 'success');
        
        // Optionally close query builder after execution
        // toggleQueryBuilder();
    } catch (error) {
        console.error('Query error:', error);
        showNotification('Query failed: ' + (error.message || 'Unknown error'), 'error');
    }
}

function clearQuery() {
    // Clear all inputs
    document.getElementById('simpleField').value = '';
    document.getElementById('simpleValue').value = '';
    document.getElementById('compField').value = '';
    document.getElementById('compValue').value = '';
    document.getElementById('customJSON').value = '';
    
    // Reset logical conditions
    const container = document.getElementById('logicalConditions');
    container.innerHTML = `
        <div class="condition mb-2">
            <div class="input-group input-group-sm">
                <input type="text" class="form-control" placeholder="Field" onkeyup="updateQueryPreview()">
                <input type="text" class="form-control" placeholder="Value" onkeyup="updateQueryPreview()">
                <button class="btn btn-outline-danger" type="button" onclick="removeCondition(this)">
                    <i class="bi bi-x"></i>
                </button>
            </div>
        </div>
    `;
    
    // Update preview
    updateQueryPreview();
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
    
    // Perform real-time validation
    clearTimeout(window.validationTimeout);
    window.validationTimeout = setTimeout(() => {
        validateDocumentRealtime(currentContent);
    }, 500); // Debounce validation by 500ms
    
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

// ===== REAL-TIME VALIDATION SYSTEM =====

async function validateDocumentRealtime(content) {
    if (!currentCollection) {
        return;
    }
    
    // First validate JSON syntax
    let jsonValid = true;
    let parsedDoc = null;
    
    // Use parseJSONSafely but don't show notifications for real-time validation
    const parseResult = parseJSONSafely(content, 'document', false);
    if (!parseResult.success) {
        jsonValid = false;
        updateValidationDisplay({
            valid: false,
            error: 'JSON Syntax Error',
            message: parseResult.error.replace('Invalid document: ', ''),
            type: 'syntax'
        });
        return;
    }
    parsedDoc = parseResult.data;
    
    // If JSON is valid, check for JavaScript validators
    try {
        const validators = await getCollectionValidators(currentCollection);
        if (validators.length === 0) {
            // No validators, just show JSON is valid
            updateValidationDisplay({
                valid: true,
                message: 'Document is valid JSON'
            });
            return;
        }
        
        // Run validators
        const validationResults = await runValidators(parsedDoc, validators);
        updateValidationDisplay(validationResults);
        
    } catch (error) {
        console.error('Validation error:', error);
        updateValidationDisplay({
            valid: false,
            error: 'Validation Error',
            message: 'Failed to run validation scripts',
            type: 'system'
        });
    }
}

async function getCollectionValidators(collection) {
    try {
        // Use the current library for validators collection
        const validatorsPath = `${currentLibrary}/_validators`;
        const response = await apiRequest(`/api/collections/${validatorsPath}/documents`);
        if (response && response.documents) {
            // Filter validators for this collection or global validators
            return response.documents.filter(script => {
                const tags = script.tags || [];
                return tags.includes(collection) || tags.includes('*') || tags.includes('global');
            });
        }
        return [];
    } catch (error) {
        // Validators collection might not exist, which is fine
        if (error.message && !error.message.includes('404') && !error.message.includes('Not found')) {
            console.error('Error loading validators:', error);
        }
        return [];
    }
}

async function runValidators(document, validators) {
    const results = {
        valid: true,
        errors: [],
        warnings: [],
        validatorResults: [],
        executionMetrics: {
            totalTime: 0,
            validatorCount: validators.length,
            successCount: 0,
            errorCount: 0
        }
    };
    
    const startTime = performance.now();
    
    for (const validator of validators) {
        const validatorStartTime = performance.now();
        
        try {
            // Validate the validator document structure first
            const validationResult = validateScriptDocument(validator, 'validator');
            if (!validationResult.valid) {
                results.valid = false;
                results.errors.push({
                    validator: validator.name || 'Unknown Validator',
                    message: `Script validation failed: ${validationResult.errors.join(', ')}`,
                    type: 'script_validation_error'
                });
                results.executionMetrics.errorCount++;
                continue;
            }
            
            const response = await apiRequest(`/api/js/functions/${validator.name || validator.id}`, {
                method: 'POST',
                body: JSON.stringify({
                    input_data: document,
                    context: {
                        collection: currentCollection,
                        operation: 'validate',
                        realtime: true
                    }
                })
            });
            
            const validatorTime = performance.now() - validatorStartTime;
            
            results.validatorResults.push({
                name: validator.name || 'Unknown Validator',
                result: response.result,
                success: response.success,
                executionTime: Math.round(validatorTime),
                scriptVersion: validator.version || '1.0.0'
            });
            
            if (!response.success) {
                results.valid = false;
                const errorMessage = parseExecutionError(response.error) || 'Validation failed';
                results.errors.push({
                    validator: validator.name || 'Unknown Validator',
                    message: errorMessage,
                    type: 'execution_error',
                    details: response.error
                });
                results.executionMetrics.errorCount++;
            } else {
                results.executionMetrics.successCount++;
                
                // Handle validation result
                if (response.result) {
                    if (response.result.valid === false) {
                        results.valid = false;
                        const errors = response.result.errors || [];
                        errors.forEach(error => {
                            results.errors.push({
                                validator: validator.name || 'Unknown Validator',
                                message: error,
                                type: 'validation_rule_error'
                            });
                        });
                    }
                    
                    if (response.result.warnings) {
                        response.result.warnings.forEach(warning => {
                            results.warnings.push({
                                validator: validator.name || 'Unknown Validator',
                                message: warning,
                                type: 'validation_warning'
                            });
                        });
                    }
                }
            }
            
        } catch (error) {
            results.valid = false;
            results.executionMetrics.errorCount++;
            
            let errorMessage = 'Unknown execution error';
            let errorType = 'network_error';
            
            if (error.message) {
                if (error.message.includes('Failed to fetch')) {
                    errorMessage = 'Network error: Unable to connect to server';
                    errorType = 'network_error';
                } else if (error.message.includes('Unauthorized')) {
                    errorMessage = 'Authorization error: Insufficient permissions to execute validator';
                    errorType = 'auth_error';
                } else if (error.message.includes('timeout')) {
                    errorMessage = 'Timeout error: Validator execution took too long';
                    errorType = 'timeout_error';
                } else {
                    errorMessage = `Execution error: ${error.message}`;
                    errorType = 'execution_error';
                }
            }
            
            results.errors.push({
                validator: validator.name || 'Unknown Validator',
                message: errorMessage,
                type: errorType,
                details: error.stack || error.message
            });
        }
    }
    
    results.executionMetrics.totalTime = Math.round(performance.now() - startTime);
    
    // Add performance analysis
    if (results.executionMetrics.totalTime > 1000) {
        results.warnings.push({
            message: `Slow validation detected: ${results.executionMetrics.totalTime}ms total execution time`,
            type: 'performance',
            suggestion: 'Consider optimizing validators or reducing validation complexity'
        });
    }
    
    if (results.validatorResults.length > 10) {
        results.warnings.push({
            message: `High validator count: ${results.validatorResults.length} validators executed`,
            type: 'performance', 
            suggestion: 'Consider consolidating validators or using more specific tags'
        });
    }
    
    // Record performance metrics for analysis
    recordScriptPerformanceMetrics({
        operation: 'validation',
        totalTime: results.executionMetrics.totalTime,
        scriptCount: results.executionMetrics.validatorCount,
        successCount: results.executionMetrics.successCount,
        errorCount: results.executionMetrics.errorCount,
        collection: currentCollection || 'unknown',
        timestamp: new Date().toISOString()
    });
    
    return results;
}

// Helper function to validate script document structure
function validateScriptDocument(script, expectedType) {
    const errors = [];
    
    // Required fields validation
    if (!script.name || typeof script.name !== 'string') {
        errors.push('Script name is required and must be a string');
    }
    
    if (!script.type || script.type !== expectedType) {
        errors.push(`Script type must be '${expectedType}'`);
    }
    
    if (!script.code || typeof script.code !== 'string') {
        errors.push('Script code is required and must be a string');
    }
    
    if (!Array.isArray(script.tags)) {
        errors.push('Script tags must be an array');
    }
    
    if (script.enabled !== undefined && typeof script.enabled !== 'boolean') {
        errors.push('Script enabled flag must be a boolean');
    }
    
    // JavaScript syntax validation
    if (script.code) {
        try {
            // Basic syntax check using Function constructor
            new Function(script.code);
            
            // Check for required function patterns based on script type
            if (expectedType === 'validator') {
                if (!script.code.includes('function validate') && 
                    !script.code.includes('function(document, context)') &&
                    !script.code.includes('(document, context) =>')) {
                    errors.push('Validator scripts should contain a validate function with (document, context) parameters');
                }
            } else if (expectedType === 'transformer') {
                if (!script.code.includes('function transform') && 
                    !script.code.includes('function(document, context)') &&
                    !script.code.includes('(document, context) =>')) {
                    errors.push('Transformer scripts should contain a transform function with (document, context) parameters');
                }
            } else if (expectedType === 'function') {
                if (!script.code.includes('function execute') && 
                    !script.code.includes('function(input, context)') &&
                    !script.code.includes('(input, context) =>')) {
                    errors.push('Function scripts should contain an execute function with (input, context) parameters');
                }
            }
        } catch (syntaxError) {
            errors.push(`JavaScript syntax error: ${syntaxError.message}`);
        }
    }
    
    return {
        valid: errors.length === 0,
        errors: errors
    };
}

// Helper function to parse execution errors and provide better error messages
function parseExecutionError(error) {
    if (!error) return null;
    
    const errorString = typeof error === 'string' ? error : JSON.stringify(error);
    
    // Common JavaScript errors
    if (errorString.includes('ReferenceError')) {
        const match = errorString.match(/ReferenceError: (\w+) is not defined/);
        if (match) {
            return `Variable '${match[1]}' is not defined. Check your script for typos or missing variable declarations.`;
        }
        return 'Reference error: Variable or function not found';
    }
    
    if (errorString.includes('TypeError')) {
        if (errorString.includes('undefined')) {
            return 'Type error: Attempting to access property of undefined value';
        }
        if (errorString.includes('null')) {
            return 'Type error: Attempting to access property of null value';  
        }
        return 'Type error: Invalid operation on value type';
    }
    
    if (errorString.includes('SyntaxError')) {
        return 'Syntax error: Invalid JavaScript syntax in script code';
    }
    
    if (errorString.includes('timeout')) {
        return 'Execution timeout: Script took too long to execute (max 5 seconds)';
    }
    
    if (errorString.includes('memory')) {
        return 'Memory error: Script exceeded memory limit (max 64MB)';
    }
    
    if (errorString.includes('permission')) {
        return 'Permission error: Script attempted unauthorized operation';
    }
    
    return errorString;
}

function updateValidationDisplay(validation) {
    const feedbackContainer = document.getElementById('validationFeedback');
    const detailsContainer = document.getElementById('validationDetails');
    
    if (!feedbackContainer) return;
    
    // Update main status
    const statusElement = feedbackContainer.querySelector('.validation-status');
    const iconElement = statusElement.querySelector('.validation-icon');
    const messageElement = statusElement.querySelector('.validation-message');
    
    if (validation.valid) {
        iconElement.innerHTML = '<i class="bi bi-check-circle-fill text-success"></i>';
        messageElement.textContent = validation.message || 'Document is valid';
        messageElement.className = 'validation-message text-success';
        feedbackContainer.className = 'validation-feedback valid';
    } else {
        iconElement.innerHTML = '<i class="bi bi-exclamation-triangle-fill text-danger"></i>';
        messageElement.textContent = validation.error || validation.message || 'Document validation failed';
        messageElement.className = 'validation-message text-danger';
        feedbackContainer.className = 'validation-feedback invalid';
    }
    
    // Clear previous details
    detailsContainer.innerHTML = '';
    
    // Show execution metrics if available
    if (validation.executionMetrics) {
        const metricsDiv = document.createElement('div');
        metricsDiv.className = 'validation-metrics mt-2';
        metricsDiv.innerHTML = `
            <small class="text-info fw-bold">Execution Metrics:</small>
            <div class="validation-metrics-grid">
                <div class="metric-item">
                    <i class="bi bi-clock"></i> 
                    <span>Time: ${validation.executionMetrics.totalTime || 0}ms</span>
                </div>
                <div class="metric-item">
                    <i class="bi bi-gear"></i> 
                    <span>Validators: ${validation.executionMetrics.validatorCount || 0}</span>
                </div>
                <div class="metric-item">
                    <i class="bi bi-check-circle"></i> 
                    <span>Success: ${validation.executionMetrics.successCount || 0}</span>
                </div>
                <div class="metric-item">
                    <i class="bi bi-x-circle"></i> 
                    <span>Errors: ${validation.executionMetrics.errorCount || 0}</span>
                </div>
            </div>
        `;
        detailsContainer.appendChild(metricsDiv);
    }
    
    // Show validation details with enhanced error information
    if (validation.errors && validation.errors.length > 0) {
        const errorsDiv = document.createElement('div');
        errorsDiv.className = 'validation-errors mt-2';
        
        // Group errors by type for better organization
        const groupedErrors = {};
        validation.errors.forEach(error => {
            const errorType = error.type || 'validation';
            if (!groupedErrors[errorType]) {
                groupedErrors[errorType] = [];
            }
            groupedErrors[errorType].push(error);
        });
        
        let errorsHtml = '<small class="text-danger fw-bold">Errors:</small>';
        
        Object.keys(groupedErrors).forEach(errorType => {
            const typeIcon = {
                'syntax': 'bi-code-slash',
                'execution': 'bi-exclamation-octagon',
                'timeout': 'bi-clock',
                'permission': 'bi-shield-exclamation',
                'validation': 'bi-x-circle'
            }[errorType] || 'bi-x-circle';
            
            const typeLabel = {
                'syntax': 'Syntax Error',
                'execution': 'Execution Error', 
                'timeout': 'Timeout Error',
                'permission': 'Permission Error',
                'validation': 'Validation Error'
            }[errorType] || 'Error';
            
            errorsHtml += `<div class="error-group mt-1">`;
            if (Object.keys(groupedErrors).length > 1) {
                errorsHtml += `<div class="error-type-header"><i class="bi ${typeIcon}"></i> ${typeLabel}</div>`;
            }
            
            groupedErrors[errorType].forEach(error => {
                const errorMessage = typeof error === 'string' ? error : (error.message || error.toString());
                const validatorName = error.validator || error.script_name;
                const suggestion = error.suggestion || '';
                
                errorsHtml += `
                    <div class="validation-item text-danger">
                        <i class="bi ${typeIcon}"></i> 
                        ${validatorName ? `<strong>[${validatorName}]</strong> ` : ''}
                        ${errorMessage}
                        ${suggestion ? `<div class="error-suggestion text-muted"><i class="bi bi-lightbulb"></i> ${suggestion}</div>` : ''}
                    </div>
                `;
            });
            errorsHtml += `</div>`;
        });
        
        errorsDiv.innerHTML = errorsHtml;
        detailsContainer.appendChild(errorsDiv);
    }
    
    if (validation.warnings && validation.warnings.length > 0) {
        const warningsDiv = document.createElement('div');
        warningsDiv.className = 'validation-warnings mt-2';
        warningsDiv.innerHTML = `
            <small class="text-warning fw-bold">Warnings:</small>
            ${validation.warnings.map(warning => {
                const warningMessage = typeof warning === 'string' ? warning : (warning.message || warning.toString());
                const validatorName = warning.validator || warning.script_name;
                return `<div class="validation-item text-warning">
                    <i class="bi bi-exclamation-triangle"></i> 
                    ${validatorName ? `<strong>[${validatorName}]</strong> ` : ''}
                    ${warningMessage}
                </div>`;
            }).join('')}
        `;
        detailsContainer.appendChild(warningsDiv);
    }
    
    if (validation.validatorResults && validation.validatorResults.length > 0) {
        const resultsDiv = document.createElement('div');
        resultsDiv.className = 'validation-results mt-2';
        
        const successfulValidators = validation.validatorResults.filter(r => r.success);
        const failedValidators = validation.validatorResults.filter(r => !r.success);
        
        if (successfulValidators.length > 0) {
            resultsDiv.innerHTML += `
                <small class="text-success fw-bold">Validators passed (${successfulValidators.length}):</small>
                ${successfulValidators.map(result => `
                    <div class="validation-item text-success">
                        <i class="bi bi-check-circle"></i> 
                        <strong>${result.name}</strong>
                        ${result.executionTime ? `<span class="text-muted ms-2">(${result.executionTime}ms)</span>` : ''}
                    </div>`
                ).join('')}
            `;
        }
        
        if (failedValidators.length > 0) {
            resultsDiv.innerHTML += `
                <small class="text-danger fw-bold mt-2 d-block">Validators failed (${failedValidators.length}):</small>
                ${failedValidators.map(result => `
                    <div class="validation-item text-danger">
                        <i class="bi bi-x-circle"></i> 
                        <strong>${result.name}</strong>
                        ${result.error ? `<div class="ms-3 text-muted">${result.error}</div>` : ''}
                        ${result.executionTime ? `<span class="text-muted ms-2">(${result.executionTime}ms)</span>` : ''}
                    </div>`
                ).join('')}
            `;
        }
        
        detailsContainer.appendChild(resultsDiv);
    }
}

// ===== PERFORMANCE MONITORING SYSTEM =====

// Performance metrics storage
let performanceMetrics = {
    validation: [],
    transformation: [],
    function_execution: []
};

// Performance thresholds (configurable)
const PERFORMANCE_THRESHOLDS = {
    validation: {
        slow: 500,     // ms
        verySlow: 1000 // ms
    },
    transformation: {
        slow: 300,
        verySlow: 800
    },
    function_execution: {
        slow: 1000,
        verySlow: 3000
    }
};

// Record performance metrics
function recordScriptPerformanceMetrics(metrics) {
    const operationType = metrics.operation;
    
    // Add to local storage
    if (!performanceMetrics[operationType]) {
        performanceMetrics[operationType] = [];
    }
    
    performanceMetrics[operationType].push({
        ...metrics,
        id: Date.now() + Math.random(),
        timestamp: new Date().toISOString()
    });
    
    // Keep only last 100 entries per operation type
    if (performanceMetrics[operationType].length > 100) {
        performanceMetrics[operationType] = performanceMetrics[operationType].slice(-100);
    }
    
    // Store in localStorage for persistence
    try {
        localStorage.setItem('jdbx_performance_metrics', JSON.stringify(performanceMetrics));
    } catch (e) {
        console.warn('Failed to store performance metrics:', e);
    }
    
    // Send to server for permanent storage (async, don't block UI)
    sendPerformanceMetricsToServer(metrics).catch(err => {
        console.warn('Failed to send performance metrics to server:', err);
    });
}

// Send performance metrics to server
async function sendPerformanceMetricsToServer(metrics) {
    try {
        const response = await fetch('/api/js/performance/metrics', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${getAuthToken()}`
            },
            body: JSON.stringify(metrics)
        });
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
    } catch (error) {
        // Silently fail - performance metrics are nice-to-have
        console.debug('Performance metrics not sent to server:', error.message);
    }
}

// Load performance metrics from localStorage
function loadPerformanceMetrics() {
    try {
        const stored = localStorage.getItem('jdbx_performance_metrics');
        if (stored) {
            performanceMetrics = JSON.parse(stored);
        }
    } catch (e) {
        console.warn('Failed to load performance metrics:', e);
        performanceMetrics = {
            validation: [],
            transformation: [],
            function_execution: []
        };
    }
}

// Analyze performance and generate optimization suggestions
function analyzePerformance(operationType, recentCount = 10) {
    const metrics = performanceMetrics[operationType] || [];
    const recent = metrics.slice(-recentCount);
    
    if (recent.length === 0) {
        return { suggestions: [], stats: null };
    }
    
    // Calculate statistics
    const times = recent.map(m => m.totalTime);
    const avgTime = times.reduce((a, b) => a + b, 0) / times.length;
    const maxTime = Math.max(...times);
    const minTime = Math.min(...times);
    
    const scriptCounts = recent.map(m => m.scriptCount || 0);
    const avgScriptCount = scriptCounts.reduce((a, b) => a + b, 0) / scriptCounts.length;
    
    const errorRates = recent.map(m => {
        const total = (m.successCount || 0) + (m.errorCount || 0);
        return total > 0 ? (m.errorCount || 0) / total : 0;
    });
    const avgErrorRate = errorRates.reduce((a, b) => a + b, 0) / errorRates.length;
    
    const stats = {
        avgTime: Math.round(avgTime),
        maxTime,
        minTime,
        avgScriptCount: Math.round(avgScriptCount * 10) / 10,
        avgErrorRate: Math.round(avgErrorRate * 100),
        sampleSize: recent.length
    };
    
    // Generate suggestions
    const suggestions = [];
    const thresholds = PERFORMANCE_THRESHOLDS[operationType];
    
    if (avgTime > thresholds.verySlow) {
        suggestions.push({
            type: 'critical',
            category: 'performance',
            message: `Very slow ${operationType} performance (${stats.avgTime}ms average)`,
            suggestion: 'Consider optimizing script logic, reducing complexity, or using more specific tags',
            priority: 'high'
        });
    } else if (avgTime > thresholds.slow) {
        suggestions.push({
            type: 'warning',
            category: 'performance', 
            message: `Slow ${operationType} performance (${stats.avgTime}ms average)`,
            suggestion: 'Review script efficiency and consider optimizations',
            priority: 'medium'
        });
    }
    
    if (avgScriptCount > 8) {
        suggestions.push({
            type: 'warning',
            category: 'efficiency',
            message: `High script count (${stats.avgScriptCount} scripts average)`,
            suggestion: 'Consider consolidating scripts or using more specific collection tags',
            priority: 'medium'
        });
    }
    
    if (avgErrorRate > 0.1) {
        suggestions.push({
            type: 'warning',
            category: 'reliability',
            message: `High error rate (${stats.avgErrorRate}%)`,
            suggestion: 'Review script error handling and input validation',
            priority: 'high'
        });
    }
    
    // Collection-specific suggestions
    const collections = [...new Set(recent.map(m => m.collection))];
    if (collections.length > 5) {
        suggestions.push({
            type: 'info',
            category: 'organization',
            message: `Scripts running on many collections (${collections.length})`,
            suggestion: 'Consider using collection-specific scripts instead of global ones',
            priority: 'low'
        });
    }
    
    return { suggestions, stats };
}

// Get performance dashboard data
function getPerformanceDashboardData() {
    const data = {
        validation: analyzePerformance('validation'),
        transformation: analyzePerformance('transformation'), 
        function_execution: analyzePerformance('function_execution')
    };
    
    // Overall health score
    const allSuggestions = [
        ...(data.validation?.suggestions || []),
        ...(data.transformation?.suggestions || []),
        ...(data.function_execution?.suggestions || [])
    ];
    
    const criticalCount = allSuggestions.filter(s => s.type === 'critical').length;
    const warningCount = allSuggestions.filter(s => s.type === 'warning').length;
    
    let healthScore = 100;
    healthScore -= criticalCount * 25;
    healthScore -= warningCount * 10;
    healthScore = Math.max(0, healthScore);
    
    data.overall = {
        healthScore,
        criticalIssues: criticalCount,
        warnings: warningCount,
        totalSuggestions: allSuggestions.length
    };
    
    return data;
}

// Initialize performance monitoring
function initializePerformanceMonitoring() {
    loadPerformanceMetrics();
    
    // Clean up old metrics (older than 24 hours)
    const cutoff = new Date(Date.now() - 24 * 60 * 60 * 1000).toISOString();
    
    Object.keys(performanceMetrics).forEach(type => {
        performanceMetrics[type] = performanceMetrics[type].filter(
            metric => metric.timestamp > cutoff
        );
    });
    
    console.log('Performance monitoring initialized');
}

// ===== SCRIPT VERSIONING SYSTEM =====

// Script version tracking
let scriptVersions = {};

// Version comparison cache
let versionComparisonCache = {};

// Generate version number for a script
function generateVersionNumber(existingVersions = []) {
    if (existingVersions.length === 0) {
        return '1.0.0';
    }
    
    // Find the latest version and increment patch number
    const latest = existingVersions
        .map(v => v.split('.').map(n => parseInt(n)))
        .sort((a, b) => {
            for (let i = 0; i < 3; i++) {
                if (a[i] !== b[i]) return b[i] - a[i];
            }
            return 0;
        })[0];
    
    return `${latest[0]}.${latest[1]}.${latest[2] + 1}`;
}

// Create a new version of a script
async function createScriptVersion(scriptDocument, changeType = 'patch', changeDescription = '') {
    try {
        const scriptId = scriptDocument.uuid || scriptDocument._id;
        const scriptType = scriptDocument.type;
        
        // Load existing versions
        const versions = await loadScriptVersions(scriptId);
        
        // Generate new version number based on change type
        let newVersion;
        if (versions.length === 0) {
            newVersion = '1.0.0';
        } else {
            const latest = versions[0].version.split('.').map(n => parseInt(n));
            switch (changeType) {
                case 'major':
                    newVersion = `${latest[0] + 1}.0.0`;
                    break;
                case 'minor':
                    newVersion = `${latest[0]}.${latest[1] + 1}.0`;
                    break;
                case 'patch':
                default:
                    newVersion = `${latest[0]}.${latest[1]}.${latest[2] + 1}`;
                    break;
            }
        }
        
        // Create version document
        const versionDocument = {
            script_id: scriptId,
            version: newVersion,
            script_type: scriptType,
            change_type: changeType,
            change_description: changeDescription || `${changeType} update`,
            script_data: {
                name: scriptDocument.name,
                description: scriptDocument.description,
                code: scriptDocument.code,
                tags: scriptDocument.tags,
                enabled: scriptDocument.enabled,
                author: scriptDocument.author
            },
            metadata: {
                created_at: new Date().toISOString(),
                created_by: getCurrentUser(),
                file_size: new Blob([scriptDocument.code]).size,
                code_lines: scriptDocument.code.split('\n').length,
                dependencies: extractDependencies(scriptDocument.code)
            },
            performance_data: getScriptPerformanceData(scriptId),
            previous_version: versions.length > 0 ? versions[0].version : null
        };
        
        // Store version in _script_versions collection
        const response = await apiRequest('/api/collections/_script_versions', {
            method: 'POST',
            body: JSON.stringify(versionDocument)
        });
        
        if (response.success) {
            // Update the main script document with latest version info
            const updatedScript = {
                ...scriptDocument,
                version: newVersion,
                version_history: {
                    current_version: newVersion,
                    version_count: versions.length + 1,
                    last_updated: new Date().toISOString(),
                    last_updated_by: getCurrentUser()
                }
            };
            
            // Cache the version locally
            if (!scriptVersions[scriptId]) {
                scriptVersions[scriptId] = [];
            }
            scriptVersions[scriptId].unshift(versionDocument);
            
            return {
                success: true,
                version: newVersion,
                versionId: response.id,
                updatedScript
            };
        }
        
        throw new Error(response.error || 'Failed to create version');
        
    } catch (error) {
        console.error('Error creating script version:', error);
        return {
            success: false,
            error: error.message
        };
    }
}

// Load all versions for a script
async function loadScriptVersions(scriptId) {
    try {
        // Check cache first
        if (scriptVersions[scriptId]) {
            return scriptVersions[scriptId];
        }
        
        // Query _script_versions collection
        const response = await apiRequest(`/api/collections/_script_versions?script_id=${scriptId}`);
        
        if (response.success && response.documents) {
            // Sort by version number (latest first)
            const versions = response.documents.sort((a, b) => {
                const aVer = a.version.split('.').map(n => parseInt(n));
                const bVer = b.version.split('.').map(n => parseInt(n));
                
                for (let i = 0; i < 3; i++) {
                    if (aVer[i] !== bVer[i]) return bVer[i] - aVer[i];
                }
                return 0;
            });
            
            // Cache the versions
            scriptVersions[scriptId] = versions;
            return versions;
        }
        
        return [];
        
    } catch (error) {
        console.error('Error loading script versions:', error);
        return [];
    }
}

// Rollback script to a specific version
async function rollbackToVersion(scriptId, targetVersion, rollbackReason = '') {
    try {
        const versions = await loadScriptVersions(scriptId);
        const targetVersionDoc = versions.find(v => v.version === targetVersion);
        
        if (!targetVersionDoc) {
            throw new Error(`Version ${targetVersion} not found`);
        }
        
        // Get current script document
        const scriptType = targetVersionDoc.script_type;
        const collectionName = getCollectionNameForScriptType(scriptType);
        const currentScript = await apiRequest(`/api/collections/${collectionName}/${scriptId}`);
        
        if (!currentScript.success) {
            throw new Error('Failed to load current script');
        }
        
        // Create a new version with current state before rollback
        await createScriptVersion(
            currentScript.document, 
            'patch', 
            `Pre-rollback backup to ${targetVersion}`
        );
        
        // Update script with target version data
        const rolledBackScript = {
            ...currentScript.document,
            ...targetVersionDoc.script_data,
            version: generateVersionNumber(versions.map(v => v.version)),
            rollback_info: {
                rolled_back_from: currentScript.document.version,
                rolled_back_to: targetVersion,
                rollback_reason: rollbackReason,
                rollback_date: new Date().toISOString(),
                rollback_by: getCurrentUser()
            },
            updated_at: new Date().toISOString()
        };
        
        // Save the rolled back script
        const updateResponse = await apiRequest(`/api/collections/${collectionName}/${scriptId}`, {
            method: 'PUT',
            body: JSON.stringify(rolledBackScript)
        });
        
        if (updateResponse.success) {
            // Create version entry for the rollback
            await createScriptVersion(
                rolledBackScript,
                'patch',
                `Rollback to version ${targetVersion}: ${rollbackReason}`
            );
            
            // Clear cache to force reload
            delete scriptVersions[scriptId];
            
            return {
                success: true,
                message: `Successfully rolled back to version ${targetVersion}`,
                newVersion: rolledBackScript.version
            };
        }
        
        throw new Error(updateResponse.error || 'Failed to save rolled back script');
        
    } catch (error) {
        console.error('Error during rollback:', error);
        return {
            success: false,
            error: error.message
        };
    }
}

// Compare two versions of a script
function compareVersions(version1Data, version2Data) {
    const comparison = {
        metadata: {
            version1: version1Data.version,
            version2: version2Data.version,
            compared_at: new Date().toISOString()
        },
        differences: {
            name: version1Data.script_data.name !== version2Data.script_data.name,
            description: version1Data.script_data.description !== version2Data.script_data.description,
            code: version1Data.script_data.code !== version2Data.script_data.code,
            tags: JSON.stringify(version1Data.script_data.tags) !== JSON.stringify(version2Data.script_data.tags),
            enabled: version1Data.script_data.enabled !== version2Data.script_data.enabled
        },
        code_diff: generateCodeDiff(version1Data.script_data.code, version2Data.script_data.code),
        size_change: version1Data.metadata.file_size - version2Data.metadata.file_size,
        lines_change: version1Data.metadata.code_lines - version2Data.metadata.code_lines
    };
    
    comparison.has_changes = Object.values(comparison.differences).some(diff => diff);
    
    return comparison;
}

// Generate simple code diff
function generateCodeDiff(code1, code2) {
    const lines1 = code1.split('\n');
    const lines2 = code2.split('\n');
    
    const diff = [];
    const maxLines = Math.max(lines1.length, lines2.length);
    
    for (let i = 0; i < maxLines; i++) {
        const line1 = lines1[i] || '';
        const line2 = lines2[i] || '';
        
        if (line1 !== line2) {
            if (line1 && !line2) {
                diff.push({ type: 'removed', line: i + 1, content: line1 });
            } else if (!line1 && line2) {
                diff.push({ type: 'added', line: i + 1, content: line2 });
            } else {
                diff.push({ type: 'modified', line: i + 1, old: line1, new: line2 });
            }
        }
    }
    
    return diff;
}

// Extract dependencies from code (simple regex-based)
function extractDependencies(code) {
    const dependencies = new Set();
    
    // Look for common patterns
    const patterns = [
        /require\(['"`]([^'"`]+)['"`]\)/g,
        /import\s+.*\s+from\s+['"`]([^'"`]+)['"`]/g,
        /import\(['"`]([^'"`]+)['"`]\)/g
    ];
    
    patterns.forEach(pattern => {
        let match;
        while ((match = pattern.exec(code)) !== null) {
            dependencies.add(match[1]);
        }
    });
    
    return Array.from(dependencies);
}

// Get collection name for script type
function getCollectionNameForScriptType(scriptType) {
    switch (scriptType) {
        case 'validator': return '_validators';
        case 'transformer': return '_transformers';
        case 'function': return '_functions';
        default: return '_validators';
    }
}

// Get current user (placeholder - would integrate with actual auth)
function getCurrentUser() {
    return 'current_user'; // This would be replaced with actual user from session
}

// Get performance data for a script
function getScriptPerformanceData(scriptId) {
    // This would integrate with the performance monitoring system
    // to get recent performance metrics for the script
    return {
        avg_execution_time: 0,
        success_rate: 100,
        last_executed: null,
        execution_count: 0
    };
}

// Initialize versioning system
function initializeVersioningSystem() {
    console.log('Script versioning system initialized');
    
    // Load any cached version data
    try {
        const cached = localStorage.getItem('jdbx_script_versions');
        if (cached) {
            scriptVersions = JSON.parse(cached);
        }
    } catch (e) {
        console.warn('Failed to load cached version data:', e);
        scriptVersions = {};
    }
}

// UI Functions for Version Management

// Show version history for a script
async function showVersionHistory(scriptId) {
    try {
        const versions = await loadScriptVersions(scriptId);
        
        if (versions.length === 0) {
            showOperationResult(false, 'No version history found for this script');
            return;
        }
        
        // Create version history modal
        const modal = document.createElement('div');
        modal.className = 'modal fade';
        modal.innerHTML = `
            <div class="modal-dialog modal-xl">
                <div class="modal-content">
                    <div class="modal-header">
                        <h5 class="modal-title"><i class="bi bi-clock-history"></i> Version History</h5>
                        <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                    </div>
                    <div class="modal-body">
                        <div class="version-history-container">
                            ${renderVersionHistory(versions)}
                        </div>
                    </div>
                    <div class="modal-footer">
                        <button type="button" class="btn btn-unified" onclick="exportVersionHistory('${scriptId}')">
                            <i class="bi bi-download"></i> Export History
                        </button>
                        <button type="button" class="btn btn-unified-primary" data-bs-dismiss="modal">Close</button>
                    </div>
                </div>
            </div>
        `;
        
        document.body.appendChild(modal);
        const bootstrapModal = new bootstrap.Modal(modal);
        bootstrapModal.show();
        
        // Clean up modal when closed
        modal.addEventListener('hidden.bs.modal', () => {
            document.body.removeChild(modal);
        });
        
    } catch (error) {
        showOperationResult(false, 'Failed to load version history', { error: error.message });
    }
}

// Render version history HTML
function renderVersionHistory(versions) {
    return `
        <div class="version-timeline">
            ${versions.map((version, index) => `
                <div class="version-item ${index === 0 ? 'current' : ''}">
                    <div class="version-marker">
                        <div class="version-number">${version.version}</div>
                        ${index === 0 ? '<div class="current-badge">Current</div>' : ''}
                    </div>
                    <div class="version-content">
                        <div class="version-header">
                            <div class="version-info">
                                <span class="version-title">${version.change_description}</span>
                                <span class="version-type badge bg-${getChangeTypeBadgeColor(version.change_type)}">${version.change_type}</span>
                            </div>
                            <div class="version-meta">
                                <small class="text-muted">
                                    ${formatDateTime(version.metadata.created_at)} by ${version.metadata.created_by}
                                </small>
                            </div>
                        </div>
                        <div class="version-details">
                            <div class="version-stats">
                                <span class="stat-item">
                                    <i class="bi bi-file-earmark-code"></i> ${version.metadata.code_lines} lines
                                </span>
                                <span class="stat-item">
                                    <i class="bi bi-hdd"></i> ${formatFileSize(version.metadata.file_size)}
                                </span>
                                ${version.metadata.dependencies.length > 0 ? 
                                    `<span class="stat-item">
                                        <i class="bi bi-link-45deg"></i> ${version.metadata.dependencies.length} deps
                                    </span>` : ''
                                }
                            </div>
                            <div class="version-actions">
                                <button class="btn btn-sm btn-unified" onclick="viewVersionCode('${version.uuid || version._id}')">
                                    <i class="bi bi-eye"></i> View Code
                                </button>
                                ${index > 0 ? `
                                    <button class="btn btn-sm btn-unified" onclick="compareWithCurrent('${version.script_id}', '${version.version}')">
                                        <i class="bi bi-arrow-left-right"></i> Compare
                                    </button>
                                    <button class="btn btn-sm btn-unified-warning" onclick="rollbackToVersionWithConfirm('${version.script_id}', '${version.version}')">
                                        <i class="bi bi-arrow-counterclockwise"></i> Rollback
                                    </button>
                                ` : ''}
                            </div>
                        </div>
                        ${version.rollback_info ? `
                            <div class="rollback-info">
                                <i class="bi bi-info-circle"></i>
                                Rollback from ${version.rollback_info.rolled_back_from} to ${version.rollback_info.rolled_back_to}
                                ${version.rollback_info.rollback_reason ? `: ${version.rollback_info.rollback_reason}` : ''}
                            </div>
                        ` : ''}
                    </div>
                </div>
            `).join('')}
        </div>
    `;
}

// Show version comparison
async function compareWithCurrent(scriptId, targetVersion) {
    try {
        const versions = await loadScriptVersions(scriptId);
        const currentVersion = versions[0];
        const targetVersionDoc = versions.find(v => v.version === targetVersion);
        
        if (!targetVersionDoc) {
            showOperationResult(false, `Version ${targetVersion} not found`);
            return;
        }
        
        const comparison = compareVersions(currentVersion, targetVersionDoc);
        showVersionComparison(comparison, currentVersion, targetVersionDoc);
        
    } catch (error) {
        showOperationResult(false, 'Failed to compare versions', { error: error.message });
    }
}

// Show version comparison modal
function showVersionComparison(comparison, version1, version2) {
    const modal = document.createElement('div');
    modal.className = 'modal fade';
    modal.innerHTML = `
        <div class="modal-dialog modal-xl">
            <div class="modal-content">
                <div class="modal-header">
                    <h5 class="modal-title">
                        <i class="bi bi-arrow-left-right"></i> 
                        Compare Versions: ${version1.version} ↔ ${version2.version}
                    </h5>
                    <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                </div>
                <div class="modal-body">
                    <div class="version-comparison">
                        ${renderVersionComparison(comparison, version1, version2)}
                    </div>
                </div>
                <div class="modal-footer">
                    <button type="button" class="btn btn-unified" onclick="exportComparison()">
                        <i class="bi bi-download"></i> Export Comparison
                    </button>
                    <button type="button" class="btn btn-unified-primary" data-bs-dismiss="modal">Close</button>
                </div>
            </div>
        </div>
    `;
    
    document.body.appendChild(modal);
    const bootstrapModal = new bootstrap.Modal(modal);
    bootstrapModal.show();
    
    // Clean up modal when closed
    modal.addEventListener('hidden.bs.modal', () => {
        document.body.removeChild(modal);
    });
}

// Render version comparison
function renderVersionComparison(comparison, version1, version2) {
    return `
        <div class="comparison-header">
            <div class="comparison-summary">
                ${comparison.has_changes ? 
                    `<div class="alert alert-info">
                        <i class="bi bi-info-circle"></i> 
                        Found differences between versions
                    </div>` :
                    `<div class="alert alert-success">
                        <i class="bi bi-check-circle"></i> 
                        No differences found between versions
                    </div>`
                }
            </div>
            <div class="comparison-stats">
                <div class="stat-card">
                    <div class="stat-value">${comparison.size_change > 0 ? '+' : ''}${comparison.size_change}</div>
                    <div class="stat-label">Size Change (bytes)</div>
                </div>
                <div class="stat-card">
                    <div class="stat-value">${comparison.lines_change > 0 ? '+' : ''}${comparison.lines_change}</div>
                    <div class="stat-label">Lines Change</div>
                </div>
                <div class="stat-card">
                    <div class="stat-value">${comparison.code_diff.length}</div>
                    <div class="stat-label">Code Changes</div>
                </div>
            </div>
        </div>
        
        <div class="comparison-details">
            <div class="row">
                <div class="col-md-6">
                    <div class="version-panel">
                        <h6><i class="bi bi-tag"></i> Version ${version1.version} (Current)</h6>
                        <div class="version-metadata">
                            <div><strong>Created:</strong> ${formatDateTime(version1.metadata.created_at)}</div>
                            <div><strong>Author:</strong> ${version1.metadata.created_by}</div>
                            <div><strong>Size:</strong> ${formatFileSize(version1.metadata.file_size)}</div>
                            <div><strong>Lines:</strong> ${version1.metadata.code_lines}</div>
                        </div>
                    </div>
                </div>
                <div class="col-md-6">
                    <div class="version-panel">
                        <h6><i class="bi bi-tag"></i> Version ${version2.version}</h6>
                        <div class="version-metadata">
                            <div><strong>Created:</strong> ${formatDateTime(version2.metadata.created_at)}</div>
                            <div><strong>Author:</strong> ${version2.metadata.created_by}</div>
                            <div><strong>Size:</strong> ${formatFileSize(version2.metadata.file_size)}</div>
                            <div><strong>Lines:</strong> ${version2.metadata.code_lines}</div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        
        ${comparison.code_diff.length > 0 ? `
            <div class="code-diff-section">
                <h6><i class="bi bi-code-slash"></i> Code Differences</h6>
                <div class="code-diff">
                    ${renderCodeDiff(comparison.code_diff)}
                </div>
            </div>
        ` : ''}
    `;
}

// Render code diff
function renderCodeDiff(diff) {
    return diff.map(change => {
        const typeClass = change.type === 'added' ? 'diff-added' : 
                         change.type === 'removed' ? 'diff-removed' : 'diff-modified';
        
        if (change.type === 'modified') {
            return `
                <div class="diff-line ${typeClass}">
                    <div class="line-number">${change.line}</div>
                    <div class="diff-content">
                        <div class="diff-old">- ${escapeHtml(change.old)}</div>
                        <div class="diff-new">+ ${escapeHtml(change.new)}</div>
                    </div>
                </div>
            `;
        } else {
            return `
                <div class="diff-line ${typeClass}">
                    <div class="line-number">${change.line}</div>
                    <div class="diff-content">
                        <div class="diff-${change.type}">
                            ${change.type === 'added' ? '+' : '-'} ${escapeHtml(change.content)}
                        </div>
                    </div>
                </div>
            `;
        }
    }).join('');
}

// Rollback with confirmation
function rollbackToVersionWithConfirm(scriptId, targetVersion) {
    const modal = document.createElement('div');
    modal.className = 'modal fade';
    modal.innerHTML = `
        <div class="modal-dialog">
            <div class="modal-content">
                <div class="modal-header">
                    <h5 class="modal-title">
                        <i class="bi bi-exclamation-triangle text-warning"></i> 
                        Confirm Rollback
                    </h5>
                    <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                </div>
                <div class="modal-body">
                    <p>Are you sure you want to rollback to version <strong>${targetVersion}</strong>?</p>
                    <p class="text-muted">This action will:</p>
                    <ul class="text-muted">
                        <li>Create a backup of the current version</li>
                        <li>Restore the script to version ${targetVersion}</li>
                        <li>Create a new version entry for the rollback</li>
                    </ul>
                    <div class="form-group mt-3">
                        <label for="rollbackReason" class="form-label">Rollback Reason (optional):</label>
                        <input type="text" class="form-control" id="rollbackReason" 
                               placeholder="Reason for rollback...">
                    </div>
                </div>
                <div class="modal-footer">
                    <button type="button" class="btn btn-unified" data-bs-dismiss="modal">Cancel</button>
                    <button type="button" class="btn btn-unified-warning" onclick="performRollback('${scriptId}', '${targetVersion}')">
                        <i class="bi bi-arrow-counterclockwise"></i> Rollback
                    </button>
                </div>
            </div>
        </div>
    `;
    
    document.body.appendChild(modal);
    const bootstrapModal = new bootstrap.Modal(modal);
    bootstrapModal.show();
    
    // Clean up modal when closed
    modal.addEventListener('hidden.bs.modal', () => {
        document.body.removeChild(modal);
    });
}

// Perform the actual rollback
async function performRollback(scriptId, targetVersion) {
    const reasonInput = document.getElementById('rollbackReason');
    const reason = reasonInput ? reasonInput.value : '';
    
    // Close the confirmation modal
    const modal = document.querySelector('.modal.show');
    if (modal) {
        const bootstrapModal = bootstrap.Modal.getInstance(modal);
        bootstrapModal.hide();
    }
    
    try {
        showOperationResult(null, 'Rolling back...', null, true);
        
        const result = await rollbackToVersion(scriptId, targetVersion, reason);
        
        if (result.success) {
            showOperationResult(true, result.message);
            
            // Refresh the current view if we're looking at this script
            if (currentDocumentId === scriptId) {
                loadDocument(scriptId);
            }
        } else {
            showOperationResult(false, 'Rollback failed', { error: result.error });
        }
        
    } catch (error) {
        showOperationResult(false, 'Rollback failed', { error: error.message });
    }
}

// Helper functions
function getChangeTypeBadgeColor(changeType) {
    switch (changeType) {
        case 'major': return 'danger';
        case 'minor': return 'warning';
        case 'patch': return 'info';
        default: return 'secondary';
    }
}

function formatDateTime(dateString) {
    return new Date(dateString).toLocaleString();
}

function formatFileSize(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// ===== VERSION CONTROL INTEGRATION =====

function updateVersionControls() {
    const versionControls = document.getElementById('versionControls');
    const currentVersionLabel = document.getElementById('currentVersionLabel');
    
    // Check if current document is a JavaScript script
    if (currentDocument && isJavaScriptScript()) {
        versionControls.style.display = 'flex';
        
        // Load and display current version
        loadCurrentVersion();
    } else {
        versionControls.style.display = 'none';
        currentVersionLabel.style.display = 'none';
    }
}

function isJavaScriptScript() {
    return currentCollection && ['_validators', '_transformers', '_functions'].includes(currentCollection);
}

async function loadCurrentVersion() {
    try {
        const versions = await loadScriptVersions(currentDocument);
        const currentVersionLabel = document.getElementById('currentVersionLabel');
        
        if (versions.length > 0) {
            const latestVersion = versions[0];
            currentVersionLabel.textContent = `v${latestVersion.version}`;
            currentVersionLabel.style.display = 'inline';
        } else {
            currentVersionLabel.textContent = 'No versions';
            currentVersionLabel.style.display = 'inline';
        }
    } catch (error) {
        console.error('Error loading current version:', error);
        const currentVersionLabel = document.getElementById('currentVersionLabel');
        currentVersionLabel.style.display = 'none';
    }
}

function showCreateVersionModal() {
    if (!currentDocument || !isJavaScriptScript()) {
        showMessage('Version management is only available for JavaScript scripts.', 'warning');
        return;
    }
    
    // Reset form
    document.getElementById('createVersionForm').reset();
    document.getElementById('changeType').value = 'patch';
    
    // Show modal
    const modal = new bootstrap.Modal(document.getElementById('createVersionModal'));
    modal.show();
}

async function createVersionFromModal() {
    try {
        const changeType = document.getElementById('changeType').value;
        const changeDescription = document.getElementById('changeDescription').value;
        const versionTags = document.getElementById('versionTags').value;
        
        if (!changeDescription.trim()) {
            showMessage('Please provide a change description.', 'warning');
            return;
        }
        
        // Get current document data
        const currentDoc = documents[currentDocumentIndex];
        if (!currentDoc) {
            showMessage('No document selected.', 'error');
            return;
        }
        
        // Parse tags
        const tags = versionTags ? versionTags.split(',').map(t => t.trim()).filter(t => t) : [];
        
        // Create version
        const versionDoc = await createScriptVersion(currentDoc, changeType, changeDescription);
        
        // Add tags if provided
        if (tags.length > 0) {
            versionDoc.tags = [...(versionDoc.tags || []), ...tags];
            await apiRequest(`/api/collections/_script_versions/${versionDoc.uuid || versionDoc._id}`, 'PUT', versionDoc);
        }
        
        showMessage(`Version ${versionDoc.version} created successfully!`, 'success');
        
        // Update version display
        loadCurrentVersion();
        
        // Close modal
        bootstrap.Modal.getInstance(document.getElementById('createVersionModal')).hide();
        
    } catch (error) {
        console.error('Error creating version:', error);
        showMessage('Failed to create version: ' + error.message, 'error');
    }
}

async function showVersionHistory() {
    if (!currentDocument || !isJavaScriptScript()) {
        showMessage('Version management is only available for JavaScript scripts.', 'warning');
        return;
    }
    
    const modal = new bootstrap.Modal(document.getElementById('versionHistoryModal'));
    const content = document.getElementById('versionHistoryContent');
    
    // Show loading
    content.innerHTML = `
        <div class="text-center p-4">
            <div class="spinner-border" role="status">
                <span class="visually-hidden">Loading...</span>
            </div>
        </div>
    `;
    
    modal.show();
    
    try {
        const versions = await loadScriptVersions(currentDocument);
        content.innerHTML = renderVersionHistory(versions);
    } catch (error) {
        console.error('Error loading version history:', error);
        content.innerHTML = `
            <div class="alert alert-danger">
                <i class="bi bi-exclamation-triangle"></i>
                Failed to load version history: ${error.message}
            </div>
        `;
    }
}

// Global variable to store rollback target for confirmation modal
let rollbackTarget = null;

function rollbackToVersionWithConfirm(scriptId, version) {
    rollbackTarget = { scriptId, version };
    
    const modal = new bootstrap.Modal(document.getElementById('rollbackConfirmModal'));
    const details = document.getElementById('rollbackDetails');
    const reasonField = document.getElementById('rollbackReason');
    const confirmBtn = document.getElementById('confirmRollbackBtn');
    
    details.textContent = `This will rollback the script to version ${version}. The current version will be backed up automatically.`;
    reasonField.value = '';
    
    // Set up confirm button handler
    confirmBtn.onclick = async () => {
        const reason = reasonField.value.trim();
        if (!reason) {
            showMessage('Please provide a reason for the rollback.', 'warning');
            return;
        }
        
        try {
            await rollbackToVersion(rollbackTarget.scriptId, rollbackTarget.version, reason);
            showMessage(`Successfully rolled back to version ${rollbackTarget.version}`, 'success');
            
            // Reload current document to show changes
            if (currentCollection) {
                loadDocuments(currentCollection);
            }
            
            // Update version display
            loadCurrentVersion();
            
            bootstrap.Modal.getInstance(document.getElementById('rollbackConfirmModal')).hide();
            bootstrap.Modal.getInstance(document.getElementById('versionHistoryModal')).hide();
            
        } catch (error) {
            console.error('Error during rollback:', error);
            showMessage('Rollback failed: ' + error.message, 'error');
        }
    };
    
    modal.show();
}

// ===== BATCH SCRIPT OPERATIONS SYSTEM =====

// Global state for batch operations
let batchMode = false;
let selectedScripts = new Set();

function toggleBatchMode() {
    batchMode = !batchMode;
    selectedScripts.clear();
    
    const batchControls = document.getElementById('batchControls');
    const batchToggleBtn = document.getElementById('batchToggleBtn');
    
    if (batchMode) {
        batchControls.style.display = 'flex';
        batchToggleBtn.innerHTML = '<i class="bi bi-x-circle"></i> Exit Batch';
        batchToggleBtn.classList.remove('btn-unified');
        batchToggleBtn.classList.add('btn-unified-danger');
        
        // Add checkboxes to documents
        renderDocumentsWithBatchSelection();
    } else {
        batchControls.style.display = 'none';
        batchToggleBtn.innerHTML = '<i class="bi bi-check2-square"></i> Batch Mode';
        batchToggleBtn.classList.remove('btn-unified-danger');
        batchToggleBtn.classList.add('btn-unified');
        
        // Remove checkboxes from documents
        renderDocuments();
    }
    
    updateBatchControls();
}

function renderDocumentsWithBatchSelection() {
    if (!batchMode) {
        renderDocuments();
        return;
    }
    
    const documentsContainer = document.getElementById('documentsList');
    
    if (!documents || documents.length === 0) {
        documentsContainer.innerHTML = '<div class="text-muted text-center p-3">No documents found</div>';
        return;
    }
    
    const documentsHTML = documents.map((doc, index) => {
        const docId = doc.uuid || doc._id || doc.id;
        const isSelected = selectedScripts.has(docId);
        const docName = doc.name || docId || `Document ${index + 1}`;
        const docType = getDocumentType(doc);
        
        return `
            <div class="list-group-item list-group-item-action d-flex align-items-center ${currentDocument === docId ? 'active' : ''}" 
                 onclick="selectDocument(${index})">
                <div class="form-check me-2" onclick="event.stopPropagation();">
                    <input class="form-check-input" type="checkbox" 
                           id="batch_${docId}" 
                           ${isSelected ? 'checked' : ''}
                           onchange="toggleScriptSelection('${docId}')">
                </div>
                <div class="flex-grow-1">
                    <div class="d-flex justify-content-between align-items-center">
                        <span class="fw-bold">${docName}</span>
                        <small class="text-muted">${docType}</small>
                    </div>
                    ${doc.description ? `<small class="text-muted">${doc.description}</small>` : ''}
                </div>
            </div>
        `;
    }).join('');
    
    documentsContainer.innerHTML = documentsHTML;
}

function toggleScriptSelection(scriptId) {
    if (selectedScripts.has(scriptId)) {
        selectedScripts.delete(scriptId);
    } else {
        selectedScripts.add(scriptId);
    }
    updateBatchControls();
}

function selectAllScripts() {
    documents.forEach(doc => {
        const docId = doc.uuid || doc._id || doc.id;
        selectedScripts.add(docId);
    });
    renderDocumentsWithBatchSelection();
    updateBatchControls();
}

function clearScriptSelection() {
    selectedScripts.clear();
    renderDocumentsWithBatchSelection();
    updateBatchControls();
}

function updateBatchControls() {
    const count = selectedScripts.size;
    const batchCount = document.getElementById('batchCount');
    const batchActions = document.getElementById('batchActions');
    
    if (batchCount) {
        batchCount.textContent = `${count} selected`;
    }
    
    if (batchActions) {
        batchActions.style.display = count > 0 ? 'block' : 'none';
    }
}

async function batchEnableScripts() {
    if (selectedScripts.size === 0) return;
    
    const confirmation = confirm(`Enable ${selectedScripts.size} selected scripts?`);
    if (!confirmation) return;
    
    try {
        const results = await processBatchOperation(selectedScripts, async (scriptId) => {
            const doc = documents.find(d => (d.uuid || d._id || d.id) === scriptId);
            if (doc) {
                doc.enabled = true;
                await apiRequest(`/api/collections/${currentCollection}/${scriptId}`, 'PUT', doc);
                return { success: true, scriptId };
            }
            return { success: false, scriptId, error: 'Document not found' };
        });
        
        showBatchResults('Enable Scripts', results);
        loadDocuments(currentCollection);
    } catch (error) {
        showMessage('Batch enable failed: ' + error.message, 'error');
    }
}

async function batchDisableScripts() {
    if (selectedScripts.size === 0) return;
    
    const confirmation = confirm(`Disable ${selectedScripts.size} selected scripts?`);
    if (!confirmation) return;
    
    try {
        const results = await processBatchOperation(selectedScripts, async (scriptId) => {
            const doc = documents.find(d => (d.uuid || d._id || d.id) === scriptId);
            if (doc) {
                doc.enabled = false;
                await apiRequest(`/api/collections/${currentCollection}/${scriptId}`, 'PUT', doc);
                return { success: true, scriptId };
            }
            return { success: false, scriptId, error: 'Document not found' };
        });
        
        showBatchResults('Disable Scripts', results);
        loadDocuments(currentCollection);
    } catch (error) {
        showMessage('Batch disable failed: ' + error.message, 'error');
    }
}

async function batchDeleteScripts() {
    if (selectedScripts.size === 0) return;
    
    const confirmation = confirm(`Delete ${selectedScripts.size} selected scripts? This action cannot be undone.`);
    if (!confirmation) return;
    
    try {
        const results = await processBatchOperation(selectedScripts, async (scriptId) => {
            await apiRequest(`/api/collections/${currentCollection}/${scriptId}`, 'DELETE');
            return { success: true, scriptId };
        });
        
        showBatchResults('Delete Scripts', results);
        selectedScripts.clear();
        loadDocuments(currentCollection);
    } catch (error) {
        showMessage('Batch delete failed: ' + error.message, 'error');
    }
}

async function batchCreateVersions() {
    if (selectedScripts.size === 0) return;
    
    const changeType = prompt('Enter change type (patch/minor/major):', 'patch');
    if (!changeType || !['patch', 'minor', 'major'].includes(changeType)) {
        showMessage('Please enter a valid change type: patch, minor, or major', 'warning');
        return;
    }
    
    const changeDescription = prompt('Enter change description:');
    if (!changeDescription || !changeDescription.trim()) {
        showMessage('Please provide a change description', 'warning');
        return;
    }
    
    try {
        const results = await processBatchOperation(selectedScripts, async (scriptId) => {
            const doc = documents.find(d => (d.uuid || d._id || d.id) === scriptId);
            if (doc) {
                const versionDoc = await createScriptVersion(doc, changeType, changeDescription.trim());
                return { success: true, scriptId, version: versionDoc.version };
            }
            return { success: false, scriptId, error: 'Document not found' };
        });
        
        showBatchResults('Create Versions', results);
    } catch (error) {
        showMessage('Batch version creation failed: ' + error.message, 'error');
    }
}

async function batchExportScripts() {
    if (selectedScripts.size === 0) return;
    
    try {
        const exportData = {
            export_type: 'javascript_scripts',
            export_date: new Date().toISOString(),
            collection: currentCollection,
            scripts: []
        };
        
        for (const scriptId of selectedScripts) {
            const doc = documents.find(d => (d.uuid || d._id || d.id) === scriptId);
            if (doc) {
                // Include version history if available
                try {
                    const versions = await loadScriptVersions(scriptId);
                    exportData.scripts.push({
                        document: doc,
                        versions: versions
                    });
                } catch (error) {
                    // Include document without versions if version loading fails
                    exportData.scripts.push({
                        document: doc,
                        versions: []
                    });
                }
            }
        }
        
        const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `${currentCollection}_scripts_${new Date().toISOString().split('T')[0]}.json`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        
        showMessage(`Exported ${selectedScripts.size} scripts successfully`, 'success');
    } catch (error) {
        showMessage('Export failed: ' + error.message, 'error');
    }
}

function showImportScriptsModal() {
    const modal = new bootstrap.Modal(document.getElementById('importScriptsModal'));
    
    // Reset form
    document.getElementById('importScriptsForm').reset();
    document.getElementById('importPreview').innerHTML = '';
    document.getElementById('importActions').style.display = 'none';
    document.getElementById('executeImportBtn').style.display = 'none';
    
    modal.show();
}

async function handleImportFileSelect(event) {
    const file = event.target.files[0];
    if (!file) return;
    
    try {
        const text = await file.text();
        const parseResult = parseJSONSafely(text, 'script import file');
        if (!parseResult.success) {
            return;
        }
        const importData = parseResult.data;
        
        // Validate import data structure
        if (!importData.export_type || importData.export_type !== 'javascript_scripts') {
            throw new Error('Invalid export file format');
        }
        
        if (!importData.scripts || !Array.isArray(importData.scripts)) {
            throw new Error('No scripts found in export file');
        }
        
        // Show preview
        renderImportPreview(importData);
        document.getElementById('importActions').style.display = 'block';
        document.getElementById('executeImportBtn').style.display = 'inline-block';
        
    } catch (error) {
        showMessage('Failed to parse import file: ' + error.message, 'error');
        event.target.value = '';
    }
}

function renderImportPreview(importData) {
    const preview = document.getElementById('importPreview');
    const scripts = importData.scripts;
    
    const html = `
        <div class="alert alert-info">
            <h6>Import Preview</h6>
            <p>Found ${scripts.length} scripts from collection: ${importData.collection}</p>
            <p>Export date: ${new Date(importData.export_date).toLocaleDateString()}</p>
        </div>
        
        <div class="table-responsive">
            <table class="table table-sm">
                <thead>
                    <tr>
                        <th>Name</th>
                        <th>Type</th>
                        <th>Versions</th>
                        <th>Status</th>
                    </tr>
                </thead>
                <tbody>
                    ${scripts.map(script => {
                        const doc = script.document;
                        const versions = script.versions || [];
                        return `
                            <tr>
                                <td>${doc.name || doc.uuid || doc._id || 'Unnamed'}</td>
                                <td>${getDocumentType(doc)}</td>
                                <td>${versions.length} versions</td>
                                <td>
                                    <span class="badge ${doc.enabled ? 'bg-success' : 'bg-secondary'}">
                                        ${doc.enabled ? 'Enabled' : 'Disabled'}
                                    </span>
                                </td>
                            </tr>
                        `;
                    }).join('')}
                </tbody>
            </table>
        </div>
    `;
    
    preview.innerHTML = html;
    
    // Store import data for processing
    window.pendingImportData = importData;
}

async function executeImportScripts() {
    if (!window.pendingImportData) {
        showMessage('No import data available', 'error');
        return;
    }
    
    const importVersions = document.getElementById('importVersions').checked;
    const overwriteExisting = document.getElementById('overwriteExisting').checked;
    const importData = window.pendingImportData;
    
    try {
        const results = [];
        const total = importData.scripts.length;
        
        // Show progress
        const progressModal = showProgressModal('Importing scripts...', total);
        
        for (let i = 0; i < importData.scripts.length; i++) {
            const scriptData = importData.scripts[i];
            const doc = scriptData.document;
            const versions = scriptData.versions || [];
            
            try {
                // Check if script already exists
                const existingDocs = await apiRequest(`/api/collections/${currentCollection}`);
                const existing = existingDocs.find(d => d.name === doc.name || d.uuid === doc.uuid || d._id === doc._id);
                
                if (existing && !overwriteExisting) {
                    results.push({ 
                        success: false, 
                        scriptName: doc.name || doc.uuid || doc._id, 
                        error: 'Script already exists (use overwrite option)' 
                    });
                    continue;
                }
                
                // Import the script document
                let importedDoc;
                if (existing && overwriteExisting) {
                    // Update existing document
                    const updateData = { ...doc };
                    delete updateData._id; // Remove _id to avoid conflicts
                    delete updateData.uuid; // Remove uuid to avoid conflicts
                    importedDoc = await apiRequest(`/api/collections/${currentCollection}/${existing.uuid || existing._id}`, 'PUT', updateData);
                } else {
                    // Create new document
                    const createData = { ...doc };
                    delete createData._id; // Let server assign new ID
                    delete createData.uuid; // Let server assign new UUID
                    importedDoc = await apiRequest(`/api/collections/${currentCollection}`, 'POST', createData);
                }
                
                // Import version history if requested
                if (importVersions && versions.length > 0) {
                    for (const version of versions) {
                        try {
                            const versionData = { ...version };
                            versionData.script_id = importedDoc.uuid || importedDoc._id || importedDoc.id;
                            delete versionData._id; // Let server assign new ID
                            delete versionData.uuid; // Let server assign new UUID
                            
                            await apiRequest('/api/collections/_script_versions', 'POST', versionData);
                        } catch (versionError) {
                            console.warn(`Failed to import version ${version.version}:`, versionError);
                        }
                    }
                }
                
                results.push({ 
                    success: true, 
                    scriptName: doc.name || doc.uuid || doc._id,
                    versionsImported: importVersions ? versions.length : 0
                });
                
            } catch (error) {
                results.push({ 
                    success: false, 
                    scriptName: doc.name || doc.uuid || doc._id, 
                    error: error.message 
                });
            }
            
            updateProgressModal(progressModal, i + 1, total);
        }
        
        hideProgressModal(progressModal);
        showImportResults(results);
        
        // Refresh the current collection
        if (currentCollection) {
            loadDocuments(currentCollection);
        }
        
        // Close modal
        bootstrap.Modal.getInstance(document.getElementById('importScriptsModal')).hide();
        
    } catch (error) {
        showMessage('Import failed: ' + error.message, 'error');
    }
}

function showImportResults(results) {
    const successful = results.filter(r => r.success).length;
    const failed = results.filter(r => !r.success).length;
    
    let message = `Import completed: ${successful} scripts imported`;
    if (failed > 0) {
        message += `, ${failed} failed`;
    }
    
    const type = failed > 0 ? 'warning' : 'success';
    showMessage(message, type);
    
    // Show detailed results
    console.group('Import Results');
    results.forEach(result => {
        if (result.success) {
            console.log(`✓ ${result.scriptName} (${result.versionsImported} versions)`);
        } else {
            console.error(`✗ ${result.scriptName}: ${result.error}`);
        }
    });
    console.groupEnd();
}

async function processBatchOperation(scriptIds, operation) {
    const results = [];
    const total = scriptIds.size;
    let completed = 0;
    
    // Show progress
    const progressModal = showProgressModal('Processing batch operation...', total);
    
    try {
        for (const scriptId of scriptIds) {
            try {
                const result = await operation(scriptId);
                results.push(result);
            } catch (error) {
                results.push({ success: false, scriptId, error: error.message });
            }
            
            completed++;
            updateProgressModal(progressModal, completed, total);
        }
    } finally {
        hideProgressModal(progressModal);
    }
    
    return results;
}

function showBatchResults(operation, results) {
    const successful = results.filter(r => r.success).length;
    const failed = results.filter(r => !r.success).length;
    
    let message = `${operation} completed: ${successful} successful`;
    if (failed > 0) {
        message += `, ${failed} failed`;
    }
    
    const type = failed > 0 ? 'warning' : 'success';
    showMessage(message, type);
    
    // Show detailed results if there were failures
    if (failed > 0) {
        console.group(`${operation} - Detailed Results`);
        results.forEach(result => {
            if (!result.success) {
                console.error(`Failed: ${result.scriptId} - ${result.error}`);
            }
        });
        console.groupEnd();
    }
}

function showProgressModal(title, total) {
    const modal = document.createElement('div');
    modal.className = 'modal fade';
    modal.innerHTML = `
        <div class="modal-dialog modal-sm">
            <div class="modal-content">
                <div class="modal-header">
                    <h5 class="modal-title">${title}</h5>
                </div>
                <div class="modal-body text-center">
                    <div class="progress mb-3">
                        <div class="progress-bar" role="progressbar" style="width: 0%"></div>
                    </div>
                    <div class="progress-text">0 / ${total}</div>
                </div>
            </div>
        </div>
    `;
    
    document.body.appendChild(modal);
    const bootstrapModal = new bootstrap.Modal(modal);
    bootstrapModal.show();
    
    return { element: modal, bootstrap: bootstrapModal };
}

function updateProgressModal(progressModal, completed, total) {
    const percent = (completed / total) * 100;
    const progressBar = progressModal.element.querySelector('.progress-bar');
    const progressText = progressModal.element.querySelector('.progress-text');
    
    progressBar.style.width = `${percent}%`;
    progressText.textContent = `${completed} / ${total}`;
}

function hideProgressModal(progressModal) {
    progressModal.bootstrap.hide();
    setTimeout(() => {
        document.body.removeChild(progressModal.element);
    }, 300);
}

function getDocumentType(doc) {
    if (doc.type) return doc.type;
    if (currentCollection === '_validators') return 'validator';
    if (currentCollection === '_transformers') return 'transformer';
    if (currentCollection === '_functions') return 'function';
    return 'document';
}

// ===== TRANSFORMATION PREVIEW SYSTEM =====

async function checkTransformersAvailable() {
    await loadTransformersForPreview();
    return currentTransformers.length > 0;
}

async function toggleTransformPreview() {
    transformPreviewMode = !transformPreviewMode;
    const previewBtn = document.getElementById('transformPreviewBtn');
    
    if (transformPreviewMode) {
        previewBtn.innerHTML = '<i class="bi bi-eye-slash"></i> Hide Preview';
        previewBtn.classList.remove('btn-unified');
        previewBtn.classList.add('btn-primary');
        
        // Load and run transformers
        await loadTransformersForPreview();
        showTransformationPreview();
    } else {
        previewBtn.innerHTML = '<i class="bi bi-arrow-left-right"></i> Preview';
        previewBtn.classList.remove('btn-primary');
        previewBtn.classList.add('btn-unified');
        
        // Return to normal view
        showNormalDocumentView();
    }
}

async function loadTransformersForPreview() {
    if (!currentCollection) {
        currentTransformers = [];
        return;
    }
    
    try {
        // Use the current library for transformers collection
        const transformersPath = `${currentLibrary}/_transformers`;
        const response = await apiRequest(`/api/collections/${transformersPath}/documents`);
        if (response && response.documents) {
            // Filter transformers for this collection or global transformers
            currentTransformers = response.documents.filter(script => {
                const tags = script.tags || [];
                return tags.includes(currentCollection) || tags.includes('*') || tags.includes('global');
            });
        } else {
            currentTransformers = [];
        }
    } catch (error) {
        // Transformers collection might not exist, which is fine
        if (error.message && !error.message.includes('404') && !error.message.includes('Not found')) {
            console.error('Error loading transformers:', error);
        }
        currentTransformers = [];
    }
}

async function runTransformers(document) {
    const startTime = performance.now();
    
    if (currentTransformers.length === 0) {
        return {
            success: true,
            result: document,
            transformations: [],
            executionMetrics: {
                totalTime: 0,
                transformerCount: 0,
                successCount: 0,
                errorCount: 0
            }
        };
    }
    
    let currentDoc = document;
    const transformations = [];
    let successCount = 0;
    let errorCount = 0;
    
    for (const transformer of currentTransformers) {
        const transformerStartTime = performance.now();
        
        try {
            const response = await apiRequest(`/api/js/functions/${transformer.name || transformer.id}`, {
                method: 'POST',
                body: JSON.stringify({
                    input_data: currentDoc,
                    context: {
                        collection: currentCollection,
                        operation: 'transform',
                        preview: true
                    }
                })
            });
            
            const executionTime = Math.round(performance.now() - transformerStartTime);
            
            if (response.success && response.result) {
                currentDoc = response.result;
                transformations.push({
                    name: transformer.name,
                    success: true,
                    result: response.result,
                    executionTime
                });
                successCount++;
            } else {
                transformations.push({
                    name: transformer.name,
                    success: false,
                    error: response.error || 'Transformation failed',
                    executionTime
                });
                errorCount++;
                // Continue with original document if transformation fails
            }
            
        } catch (error) {
            const executionTime = Math.round(performance.now() - transformerStartTime);
            transformations.push({
                name: transformer.name,
                success: false,
                error: `Execution error: ${error.message}`,
                executionTime
            });
            errorCount++;
        }
    }
    
    const totalTime = Math.round(performance.now() - startTime);
    
    const executionMetrics = {
        totalTime,
        transformerCount: currentTransformers.length,
        successCount,
        errorCount
    };
    
    // Record performance metrics
    recordScriptPerformanceMetrics({
        operation: 'transformation',
        totalTime,
        scriptCount: currentTransformers.length,
        successCount,
        errorCount,
        collection: currentCollection || 'unknown',
        timestamp: new Date().toISOString()
    });
    
    return {
        success: true,
        result: currentDoc,
        transformations: transformations,
        executionMetrics
    };
}

function showTransformationPreview() {
    if (!originalDocumentContent) return;
    
    // Parse the current document
    const editor = document.getElementById('documentEditor');
    const currentContent = editor ? editor.value : originalDocumentContent;
    const parseResult = parseJSONSafely(currentContent, 'document JSON');
    if (!parseResult.success) {
        return;
    }
    const parsedDoc = parseResult.data;
    
    // Run transformers and show preview
    runTransformers(parsedDoc).then(result => {
        transformedContent = JSON.stringify(result.result, null, 2);
        
        // Create split-view layout
        const lines1 = (originalDocumentContent || '').split('\n');
        const lineNumbers1 = lines1.map((_, i) => `<span class="line-number">${i + 1}</span>`).join('\n');
        
        const lines2 = transformedContent.split('\n');
        const lineNumbers2 = lines2.map((_, i) => `<span class="line-number">${i + 1}</span>`).join('\n');
        
        document.getElementById('contentViewer').innerHTML = `
            <div class="transformation-preview-container">
                <div class="preview-section">
                    <div class="preview-header">
                        <h6 class="mb-0">Original Document</h6>
                        <span class="badge bg-secondary">${currentCollection}</span>
                    </div>
                    <div class="json-editor-container">
                        <div class="line-numbers">
                            ${lineNumbers1}
                        </div>
                        <textarea id="documentEditor" class="json-editor" spellcheck="false">${originalDocumentContent}</textarea>
                    </div>
                </div>
                
                <div class="preview-divider">
                    <i class="bi bi-arrow-right"></i>
                </div>
                
                <div class="preview-section">
                    <div class="preview-header">
                        <h6 class="mb-0">Transformed Result</h6>
                        <span class="badge bg-primary">${currentTransformers.length} transformer(s)</span>
                    </div>
                    <div class="json-editor-container">
                        <div class="line-numbers">
                            ${lineNumbers2}
                        </div>
                        <div class="json-preview">${transformedContent}</div>
                    </div>
                </div>
                
                <div class="transformation-details">
                    <h6>Transformation Summary:</h6>
                    ${result.transformations.map(t => `
                        <div class="transformation-step ${t.success ? 'success' : 'error'}">
                            <i class="bi bi-${t.success ? 'check-circle' : 'x-circle'}"></i>
                            <span class="transformer-name">${t.name}</span>
                            ${t.error ? `<span class="error-message">${t.error}</span>` : ''}
                        </div>
                    `).join('')}
                </div>
            </div>
        `;
        
        // Set up editor event listener (only for the original document)
        const editor = document.getElementById('documentEditor');
        if (editor) {
            editor.addEventListener('input', handleDocumentEditWithPreview);
            editor.addEventListener('scroll', syncScrollPreview);
        }
        
    }).catch(error => {
        console.error('Failed to run transformers:', error);
        showNotification('Failed to generate transformation preview', 'error');
    });
}

function showNormalDocumentView() {
    // Restore normal document editor view
    const lines = originalDocumentContent.split('\n');
    const lineNumbers = lines.map((_, i) => `<span class="line-number">${i + 1}</span>`).join('\n');
    
    document.getElementById('contentViewer').innerHTML = `
        <div class="json-editor-container">
            <div class="line-numbers">
                ${lineNumbers}
            </div>
            <textarea id="documentEditor" class="json-editor" spellcheck="false">${originalDocumentContent}</textarea>
            <div id="validationFeedback" class="validation-feedback">
                <div class="validation-status">
                    <span class="validation-icon"><i class="bi bi-check-circle-fill text-success"></i></span>
                    <span class="validation-message">Document is valid</span>
                </div>
                <div class="validation-details" id="validationDetails"></div>
            </div>
        </div>
    `;
    
    // Set up editor event listener
    const editor = document.getElementById('documentEditor');
    editor.addEventListener('input', handleDocumentEdit);
    editor.addEventListener('scroll', syncScroll);
    
    // Initialize validation
    setTimeout(() => validateDocumentRealtime(originalDocumentContent), 100);
}

function handleDocumentEditWithPreview() {
    // Handle editing while in preview mode
    handleDocumentEdit();
    
    // Update transformation preview with debouncing
    clearTimeout(window.transformPreviewTimeout);
    window.transformPreviewTimeout = setTimeout(() => {
        showTransformationPreview();
    }, 1000); // Debounce preview updates by 1 second
}

function syncScrollPreview() {
    // Sync scroll between original and preview sections
    const editor = document.getElementById('documentEditor');
    const lineNumbers = document.querySelector('.line-numbers');
    if (lineNumbers) {
        lineNumbers.scrollTop = editor.scrollTop;
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
            <div class="document-item ${currentDocument === (doc.uuid || doc._id) ? 'active' : ''}" 
                 onclick="selectDocument(${documents.indexOf(doc)})">
                <div class="document-title">
                    <span>${doc.uuid || doc._id || doc.id || `Document ${index + 1}`}</span>
                </div>
                <div class="document-preview">${preview}</div>
            </div>
        `;
    }).join('');
}

function refreshCollections() {
    loadBrowserCollections();
}

// Create new collection function
async function createNewCollection() {
    // Prompt user for collection name
    const collectionName = prompt(`Enter collection name for library '${currentLibrary}':`);
    
    if (!collectionName) {
        return; // User cancelled
    }
    
    // Validate collection name
    if (!/^[a-zA-Z_][a-zA-Z0-9_]*$/.test(collectionName)) {
        showNotification('Invalid collection name. Use only letters, numbers, and underscores. Must start with letter or underscore.', 'error');
        return;
    }
    
    // Check if collection already exists
    if (collections.find(col => col.name === collectionName)) {
        showNotification('Collection already exists', 'warning');
        return;
    }
    
    try {
        // First create the collection metadata in unified documents
        const collectionMetadata = {
            type: 'collection',
            library: currentLibrary,
            collection_name: collectionName,
            name: collectionName,
            description: `Collection created via Admin UI`,
            permissions: {
                owner_perms: 'rwxda',
                world_perms: 'r'
            },
            versioning: {
                enabled: true,
                max_versions: 10
            },
            created_at: new Date().toISOString()
        };
        
        // Create collection metadata document
        const metaResponse = await apiRequest('/api/collections/documents/documents', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(collectionMetadata)
        });
        
        if (!metaResponse.success && !metaResponse.id) {
            throw new Error('Failed to create collection metadata');
        }
        
        // Now create the first document in the actual collection
        const collectionPath = `${currentLibrary}/${collectionName}`;
        const initialDocument = {
            uuid: 'welcome-doc',
            name: 'Welcome Document',
            message: `Welcome to the ${collectionName} collection in library ${currentLibrary}!`,
            created_at: new Date().toISOString(),
            collection_info: {
                created_by: 'Admin UI',
                description: `Initial document for ${collectionName} collection`
            }
        };
        
        const response = await apiRequest(`/api/collections/${collectionPath}/documents`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(initialDocument)
        });
        
        if (response.success || response.id) {
            showNotification(`Collection "${collectionName}" created successfully`, 'success');
            
            // Refresh collections to show the new one
            await loadBrowserCollections();
            renderLibrarySelector(); // Update library selector with new counts
            
            // Auto-select the new collection
            setTimeout(() => selectCollection(collectionName), 100);
        } else {
            showNotification(response.error || 'Failed to create collection', 'error');
        }
    } catch (error) {
        console.error('Error creating collection:', error);
        showNotification(`Failed to create collection: ${error.message}`, 'error');
    }
}

// Old editDocument function removed - now using toggleEditMode/saveDocument

// Document CRUD Operations
async function saveDocument() {
    try {
        const editor = document.getElementById('documentEditor');
        if (!editor || !currentDocument) {
            showNotification('No document to save', 'warning');
            return;
        }
        
        // Get the current content from editor
        const content = editor.value;
        let parsedDocument;
        
        try {
            parsedDocument = JSON.parse(content);
        } catch (e) {
            showNotification('Invalid JSON format', 'error');
            return;
        }
        
        // Parse library/collection from current path
        const parts = currentCollection.split('/');
        const library = parts[0] || 'default';
        const collection = parts[1] || parts[0];
        
        // Use PUT to update the document
        const response = await apiRequest(
            `/api/libraries/${library}/collections/${collection}/documents/${currentDocument.uuid}`,
            {
                method: 'PUT',
                body: JSON.stringify(parsedDocument)
            }
        );
        
        if (response) {
            showNotification('Document saved successfully', 'success');
            currentDocument = response;
            
            // Refresh the documents list
            await loadDocuments(currentCollection);
            
            // Update the view with the saved document
            selectDocument(currentDocument);
        }
    } catch (error) {
        console.error('Error saving document:', error);
        showNotification('Failed to save document: ' + error.message, 'error');
    }
}

async function deleteDocument() {
    if (!currentDocument) {
        showNotification('No document selected', 'warning');
        return;
    }
    
    const selectedDoc = documents[currentDocumentIndex];
    const docDisplayName = selectedDoc?.name || currentDocument;
    
    if (confirm(`Are you sure you want to delete the document "${docDisplayName}"?`)) {
        try {
            // Use unified documents API - DELETE by document ID
            await apiRequest(
                `/api/documents/${currentDocument}`,
                { method: 'DELETE' }
            );
            
            showNotification('Document deleted successfully', 'success');
            
            // Clear current document
            currentDocument = null;
            currentDocumentIndex = null;
            
            // Refresh the documents list
            await loadDocuments(currentCollection);
            
            // Clear the content viewer
            document.getElementById('documentTitle').textContent = 'No document selected';
            document.getElementById('contentViewer').innerHTML = `
                <div class="empty-state">
                    <i class="bi bi-file-earmark-text"></i>
                    <p>Select a document to view its content</p>
                </div>
            `;
            
            // Disable buttons
            const editBtn = document.getElementById('editBtn');
            const deleteBtn = document.getElementById('deleteBtn');
            if (editBtn) editBtn.disabled = true;
            if (deleteBtn) deleteBtn.disabled = true;
            
        } catch (error) {
            console.error('Error deleting document:', error);
            showNotification('Failed to delete document: ' + error.message, 'error');
        }
    }
}

async function createDocument() {
    if (!currentCollection) {
        showNotification('No collection selected', 'warning');
        return;
    }
    
    try {
        // Parse library/collection from current path
        const parts = currentCollection.split('/');
        const library = parts[0] || 'default';
        const collection = parts[1] || parts[0];
        
        // Create a basic document template with proper unified architecture fields
        const newDocument = {
            name: `new_document_${Date.now()}`,
            type: collection,
            library: library,
            content: "New document content"
        };
        
        // Use POST to create the document in unified documents API
        const response = await apiRequest(
            `/api/documents`,
            {
                method: 'POST',
                body: JSON.stringify(newDocument)
            }
        );
        
        if (response) {
            showNotification('Document created successfully', 'success');
            
            // Refresh the documents list
            await loadDocuments(currentCollection);
            
            // Select the new document
            currentDocument = response;
            selectDocument(currentDocument);
        }
    } catch (error) {
        console.error('Error creating document:', error);
        showNotification('Failed to create document: ' + error.message, 'error');
    }
}

function copyToClipboard() {
    const content = document.querySelector('.json-viewer code').textContent;
    navigator.clipboard.writeText(content).then(() => {
        showNotification('Document copied to clipboard!', 'info');
    });
}

// ===== METRICS FUNCTIONALITY =====
function initializeMetrics() {
    console.log('initializeMetrics called');
    
    // Initialize performance monitoring
    initializePerformanceMonitoring();
    
    // Initialize charts if not already done
    if (!operationsChart) {
        const ctx = document.getElementById('operationsChart');
        if (ctx) {
            // Use enhanced chart configuration from metrics-improvements.js
            const config = window.enhancedChartConfigs ? window.enhancedChartConfigs.operations : {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Read Operations',
                        data: [],
                        borderColor: '#61affe',
                        backgroundColor: 'rgba(97, 175, 254, 0.1)',
                        tension: 0.4,
                        pointRadius: 3,
                        pointHoverRadius: 5
                    }, {
                        label: 'Write Operations',
                        data: [],
                        borderColor: '#49cc90',
                        backgroundColor: 'rgba(73, 204, 144, 0.1)',
                        tension: 0.4,
                        pointRadius: 3,
                        pointHoverRadius: 5
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            display: true,
                            position: 'bottom',
                            labels: {
                                usePointStyle: true,
                                padding: 15,
                                font: {
                                    size: 12
                                }
                            }
                        },
                        tooltip: {
                            backgroundColor: 'rgba(0, 0, 0, 0.8)',
                            titleColor: '#fff',
                            bodyColor: '#fff',
                            callbacks: {
                                label: function(context) {
                                    let label = context.dataset.label || '';
                                    if (label) {
                                        label += ': ';
                                    }
                                    label += (window.formatNumber ? window.formatNumber(context.parsed.y) : context.parsed.y) + ' ops/min';
                                    return label;
                                }
                            }
                        }
                    },
                    scales: {
                        x: {
                            display: true,
                            title: {
                                display: true,
                                text: 'Time',
                                font: {
                                    size: 12
                                }
                            }
                        },
                        y: {
                            display: true,
                            title: {
                                display: true,
                                text: 'Operations per Minute',
                                font: {
                                    size: 12
                                }
                            },
                            beginAtZero: true,
                            ticks: {
                                callback: function(value) {
                                    return window.formatNumber ? window.formatNumber(value) : value;
                                }
                            }
                        }
                    }
                }
            };
            // Destroy existing chart if canvas is already in use
            Chart.getChart(ctx)?.destroy();
            operationsChart = new Chart(ctx.getContext('2d'), config);
        }
    }
    
    if (!operationTypesChart) {
        const ctx = document.getElementById('operationTypesChart');
        if (ctx) {
            // Use enhanced chart configuration from metrics-improvements.js
            const config = window.enhancedChartConfigs ? window.enhancedChartConfigs.operationTypes : {
                type: 'doughnut',
                data: {
                    labels: ['Read', 'Write', 'Other'],
                    datasets: [{
                        data: [45, 30, 25],
                        backgroundColor: ['#61affe', '#49cc90', '#fca130'],
                        borderWidth: 2,
                        borderColor: '#fff'
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            display: true,
                            position: 'bottom',
                            labels: {
                                padding: 15,
                                generateLabels: function(chart) {
                                    const data = chart.data;
                                    if (data.labels.length && data.datasets.length) {
                                        const dataset = data.datasets[0];
                                        const total = dataset.data.reduce((a, b) => a + b, 0);
                                        return data.labels.map((label, i) => {
                                            const value = dataset.data[i];
                                            const percentage = total > 0 ? (value / total * 100).toFixed(1) : 0;
                                            return {
                                                text: `${label}: ${window.formatNumber ? window.formatNumber(value) : value} (${percentage}%)`,
                                                fillStyle: dataset.backgroundColor[i],
                                                strokeStyle: dataset.borderColor,
                                                lineWidth: dataset.borderWidth,
                                                hidden: false,
                                                index: i
                                            };
                                        });
                                    }
                                    return [];
                                }
                            }
                        },
                        tooltip: {
                            backgroundColor: 'rgba(0, 0, 0, 0.8)',
                            callbacks: {
                                label: function(context) {
                                    const label = context.label || '';
                                    const value = context.parsed;
                                    const total = context.dataset.data.reduce((a, b) => a + b, 0);
                                    const percentage = total > 0 ? (value / total * 100).toFixed(1) : 0;
                                    return `${label}: ${window.formatNumber ? window.formatNumber(value) : value} ops (${percentage}%)`;
                                }
                            }
                        }
                    }
                }
            };
            // Destroy existing chart if canvas is already in use
            Chart.getChart(ctx)?.destroy();
            operationTypesChart = new Chart(ctx.getContext('2d'), config);
        }
    }
    
    if (!cacheHitRateChart) {
        const ctx = document.getElementById('cacheHitRateChart');
        if (ctx) {
            // Use enhanced chart configuration from metrics-improvements.js
            const config = window.enhancedChartConfigs ? window.enhancedChartConfigs.cacheHitRate : {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Cache Hit Rate',
                        data: [],
                        borderColor: '#17a2b8',
                        backgroundColor: 'rgba(23, 162, 184, 0.1)',
                        tension: 0.4,
                        pointRadius: 3,
                        pointHoverRadius: 5,
                        yAxisID: 'y-hitrate'
                    }, {
                        label: 'Cache Size',
                        data: [],
                        borderColor: '#6c757d',
                        backgroundColor: 'rgba(108, 117, 125, 0.1)',
                        tension: 0.4,
                        pointRadius: 3,
                        pointHoverRadius: 5,
                        yAxisID: 'y-size',
                        hidden: true
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    interaction: {
                        mode: 'index',
                        intersect: false
                    },
                    plugins: {
                        legend: {
                            display: true,
                            position: 'bottom',
                            labels: {
                                usePointStyle: true,
                                padding: 15
                            }
                        },
                        tooltip: {
                            backgroundColor: 'rgba(0, 0, 0, 0.8)',
                            callbacks: {
                                label: function(context) {
                                    let label = context.dataset.label || '';
                                    if (label) {
                                        label += ': ';
                                    }
                                    if (context.dataset.yAxisID === 'y-hitrate') {
                                        label += (context.parsed.y * 100).toFixed(1) + '%';
                                    } else {
                                        label += window.formatBytes ? window.formatBytes(context.parsed.y) : context.parsed.y + ' bytes';
                                    }
                                    return label;
                                }
                            }
                        }
                    },
                    scales: {
                        x: {
                            display: true,
                            title: {
                                display: true,
                                text: 'Time'
                            }
                        },
                        'y-hitrate': {
                            type: 'linear',
                            display: true,
                            position: 'left',
                            title: {
                                display: true,
                                text: 'Hit Rate (%)'
                            },
                            beginAtZero: true,
                            max: 1,
                            ticks: {
                                callback: function(value) {
                                    return (value * 100).toFixed(0) + '%';
                                }
                            }
                        },
                        'y-size': {
                            type: 'linear',
                            display: true,
                            position: 'right',
                            title: {
                                display: true,
                                text: 'Cache Size'
                            },
                            beginAtZero: true,
                            grid: {
                                drawOnChartArea: false
                            },
                            ticks: {
                                callback: function(value) {
                                    return window.formatBytes ? window.formatBytes(value) : value + ' bytes';
                                }
                            }
                        }
                    }
                }
            };
            // Destroy existing chart if canvas is already in use
            Chart.getChart(ctx)?.destroy();
            cacheHitRateChart = new Chart(ctx.getContext('2d'), config);
        }
    }
    
    // Initialize memory usage chart
    if (!memoryUsageChart) {
        const ctx = document.getElementById('memoryUsageChart');
        if (ctx) {
            // Use enhanced chart configuration from metrics-improvements.js
            const config = window.enhancedChartConfigs ? window.enhancedChartConfigs.memory : {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Process Memory',
                        data: [],
                        borderColor: '#6f42c1',
                        backgroundColor: 'rgba(111, 66, 193, 0.2)',
                        fill: true,
                        tension: 0.4
                    }, {
                        label: 'System Memory Used',
                        data: [],
                        borderColor: '#e83e8c',
                        backgroundColor: 'rgba(232, 62, 140, 0.1)',
                        tension: 0.4,
                        borderDash: [5, 5]
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    interaction: {
                        mode: 'index',
                        intersect: false
                    },
                    plugins: {
                        legend: {
                            display: true,
                            position: 'bottom',
                            labels: {
                                usePointStyle: true,
                                padding: 15
                            }
                        },
                        tooltip: {
                            backgroundColor: 'rgba(0, 0, 0, 0.8)',
                            callbacks: {
                                label: function(context) {
                                    let label = context.dataset.label || '';
                                    if (label) {
                                        label += ': ';
                                    }
                                    label += window.formatBytes ? window.formatBytes(context.parsed.y * 1024) : (context.parsed.y + ' KB'); // Convert KB to bytes for formatting
                                    
                                    // Add percentage for system memory
                                    if (context.dataset.label === 'System Memory Used') {
                                        const totalMemory = 32638500 * 1024; // From health endpoint
                                        const percentage = (context.parsed.y * 1024 / totalMemory * 100).toFixed(1);
                                        label += ' (' + percentage + '%)';
                                    }
                                    return label;
                                }
                            }
                        }
                    },
                    scales: {
                        x: {
                            display: true,
                            title: {
                                display: true,
                                text: 'Time'
                            }
                        },
                        y: {
                            display: true,
                            title: {
                                display: true,
                                text: 'Memory Usage'
                            },
                            beginAtZero: true,
                            ticks: {
                                callback: function(value) {
                                    return window.formatBytes ? window.formatBytes(value * 1024) : (value + ' KB');
                                }
                            }
                        }
                    }
                }
            };
            // Destroy existing chart if canvas is already in use
            Chart.getChart(ctx)?.destroy();
            memoryUsageChart = new Chart(ctx.getContext('2d'), config);
        }
    }
    
    // Initialize database size chart
    if (!databaseSizeChart) {
        const ctx = document.getElementById('processMemoryChart');
        if (ctx) {
            // Destroy existing chart if canvas is already in use
            Chart.getChart(ctx)?.destroy();
            databaseSizeChart = new Chart(ctx.getContext('2d'), {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Database Size (MB)',
                        data: [],
                        borderColor: '#61affe',
                        backgroundColor: 'rgba(97, 175, 254, 0.1)',
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
                            ticks: {
                                callback: function(value) {
                                    return value + ' MB';
                                }
                            }
                        }
                    },
                    plugins: {
                        legend: {
                            position: 'bottom'
                        }
                    }
                }
            });
        }
    }
    
    // Initialize storage chart
    if (!storageChart) {
        const ctx = document.getElementById('storageChart');
        if (ctx) {
            // Destroy existing chart if canvas is already in use
            Chart.getChart(ctx)?.destroy();
            storageChart = new Chart(ctx.getContext('2d'), {
                type: 'doughnut',
                data: {
                    labels: [],
                    datasets: [{
                        data: [],
                        backgroundColor: [
                            '#61affe', '#49cc90', '#fca130', '#f93e3e', '#ff6384',
                            '#36a2eb', '#cc65fe', '#ffce56', '#4bc0c0', '#9966ff'
                        ]
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            position: 'right',
                            labels: {
                                padding: 15,
                                usePointStyle: true,
                                font: {
                                    size: 11
                                }
                            }
                        },
                        tooltip: {
                            callbacks: {
                                label: function(context) {
                                    const label = context.label || '';
                                    const value = context.parsed || 0;
                                    const total = context.dataset.data.reduce((a, b) => a + b, 0);
                                    const percentage = ((value / total) * 100).toFixed(1);
                                    return `${label}: ${formatBytes(value * 1024)} (${percentage}%)`;
                                }
                            }
                        }
                    }
                }
            });
        }
    }
    
    // Initialize Script Performance Chart
    if (!scriptPerformanceChart) {
        const ctx = document.getElementById('scriptPerformanceChart');
        if (ctx) {
            const config = {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Execution Time (ms)',
                        data: [],
                        borderColor: '#9f7aea',
                        backgroundColor: 'rgba(159, 122, 234, 0.1)',
                        tension: 0.4,
                        pointRadius: 3,
                        pointHoverRadius: 5
                    }, {
                        label: 'Success Rate (%)',
                        data: [],
                        borderColor: '#49cc90',
                        backgroundColor: 'rgba(73, 204, 144, 0.1)',
                        tension: 0.4,
                        pointRadius: 3,
                        pointHoverRadius: 5,
                        yAxisID: 'y1'
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            display: true,
                            position: 'bottom',
                            labels: {
                                usePointStyle: true,
                                padding: 15,
                                font: {
                                    size: 12
                                }
                            }
                        },
                        tooltip: {
                            backgroundColor: 'rgba(0, 0, 0, 0.8)',
                            titleColor: '#fff',
                            bodyColor: '#fff'
                        }
                    },
                    scales: {
                        x: {
                            display: true,
                            title: {
                                display: true,
                                text: 'Time',
                                font: {
                                    size: 12
                                }
                            }
                        },
                        y: {
                            type: 'linear',
                            display: true,
                            position: 'left',
                            title: {
                                display: true,
                                text: 'Execution Time (ms)',
                                font: {
                                    size: 12
                                }
                            },
                            beginAtZero: true
                        },
                        y1: {
                            type: 'linear',
                            display: true,
                            position: 'right',
                            title: {
                                display: true,
                                text: 'Success Rate (%)',
                                font: {
                                    size: 12
                                }
                            },
                            min: 0,
                            max: 100,
                            grid: {
                                drawOnChartArea: false,
                            }
                        }
                    }
                }
            };
            Chart.getChart(ctx)?.destroy();
            scriptPerformanceChart = new Chart(ctx.getContext('2d'), config);
        }
    }

    // Force immediate load with a small delay to ensure DOM is ready
    console.log('Scheduling loadMetrics...');
    setTimeout(() => {
        console.log('Calling loadMetrics from initializeMetrics');
        loadMetrics();
        
        // Initialize performance dashboard
        initializePerformanceDashboard();
        loadScriptMetrics();
    }, 100);
}

async function loadMetrics(timeRange = '1h', isPolling = false) {
    console.log('loadMetrics called with timeRange:', timeRange, 'isPolling:', isPolling);
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
        console.log('Fetching metrics data...');
        const [health, collections, cacheStats] = await Promise.all([
            apiRequest('/api/health').catch(err => { console.error('Health API error:', err); return null; }),
            apiRequest('/api/collections').catch(err => { console.error('Collections API error:', err); return { collections: [] }; }),
            apiRequest('/api/cache/stats').catch(err => { console.error('Cache stats API error:', err); return null; })
        ]);
        
        console.log('API responses:', { health, collections, cacheStats });
        
        // Fetch metrics data from _metrics collection
        console.log('Fetching metrics from _metrics collection...');
        const metricsData = await apiRequest('/api/collections/_metrics/documents').catch(err => {
            console.error('Failed to fetch metrics data:', err);
            return { documents: [] };
        });
        
        console.log('Metrics data:', metricsData);
        
        // Extract metrics documents by type
        const metricsDocuments = metricsData.documents || [];
        const operationsDoc = metricsDocuments.find(doc => doc.type === 'operations');
        const performanceDoc = metricsDocuments.find(doc => doc.type === 'performance');
        const cacheDoc = metricsDocuments.find(doc => doc.type === 'cache');
        const memoryDoc = metricsDocuments.find(doc => doc.type === 'memory');
        const connectionsDoc = metricsDocuments.find(doc => doc.type === 'connections');
        
        console.log('Found metrics documents:', {
            operations: !!operationsDoc,
            performance: !!performanceDoc,
            cache: !!cacheDoc,
            memory: !!memoryDoc,
            connections: !!connectionsDoc
        });
        
        // Get real document counts from collections
        let totalDocs = 0;
        let estimatedTotalSizeKB = 0;
        const collectionList = Array.isArray(collections) ? collections : (collections.collections || []);
        
        // Collections API returns array of objects with {name, documentCount, isSystem}
        // Sum up document counts directly from the response
        if (Array.isArray(collectionList)) {
            collectionList.forEach(col => {
                if (col && typeof col === 'object' && col.documentCount !== undefined) {
                    totalDocs += col.documentCount;
                    // Estimate 1KB per document
                    estimatedTotalSizeKB += col.documentCount * 1;
                }
            });
        }
        
        // Store this for use in storage chart later
        window.estimatedDatabaseSizeKB = estimatedTotalSizeKB;
        
        // Get current metrics from the latest data point
        let totalOps = 0;
        let readOps = 0;
        let writeOps = 0;
        let avgResponseTime = null;
        let previousTotalOps = 0;
        let previousReadOps = 0;
        let previousWriteOps = 0;
        
        // Extract latest and previous values from operations document
        if (operationsDoc && operationsDoc.data && operationsDoc.data.length > 0) {
            const latestOps = operationsDoc.data[operationsDoc.data.length - 1];
            totalOps = latestOps.total || 0;
            readOps = latestOps.read || 0;
            writeOps = latestOps.write || 0;
            
            // Get previous data point for trend calculation
            if (operationsDoc.data.length > 1) {
                const previousOps = operationsDoc.data[operationsDoc.data.length - 2];
                previousTotalOps = previousOps.total || 0;
                previousReadOps = previousOps.read || 0;
                previousWriteOps = previousOps.write || 0;
            }
        }
        
        // Get performance metrics
        if (performanceDoc && performanceDoc.data && performanceDoc.data.length > 0) {
            const latestPerf = performanceDoc.data[performanceDoc.data.length - 1];
            avgResponseTime = latestPerf.avg_response_time_ms || null;
        }
        
        console.log('Health API response:', health);
        
        if (health && health.metrics) {
            console.log('Health metrics:', health.metrics);
            totalOps = health.metrics.operations.total || 0;
            readOps = health.metrics.operations.read || 0;
            writeOps = health.metrics.operations.write || 0;
            
            // Fix: Ensure total operations is at least the sum of read and write
            const calculatedTotal = readOps + writeOps;
            if (calculatedTotal > totalOps) {
                console.log('Fixing total operations:', totalOps, '->', calculatedTotal);
                totalOps = calculatedTotal;
            }
            
            if (health.metrics.performance) {
                avgResponseTime = health.metrics.performance.avg_response_time_ms;
            }
            console.log('Parsed metrics - totalOps:', totalOps, 'readOps:', readOps, 'writeOps:', writeOps);
        } else {
            console.log('No health metrics available');
        }
        
        // Add visual indicator for updates
        if (isPolling) {
            const metricsLastUpdate = document.querySelector('#metrics-view .last-update');
            if (metricsLastUpdate) {
                metricsLastUpdate.textContent = `Last updated: ${new Date().toLocaleTimeString()}`;
            }
        }
        
        // Update metrics display with real values
        console.log('Updating DOM elements - totalOps:', totalOps, 'readOps:', readOps, 'writeOps:', writeOps);
        const totalOpsElement = document.getElementById('totalOps');
        const readOpsElement = document.getElementById('readOps');
        const writeOpsElement = document.getElementById('writeOps');
        const avgResponseTimeElement = document.getElementById('avgResponseTime');
        
        console.log('DOM elements found:', {
            totalOps: !!totalOpsElement,
            readOps: !!readOpsElement,
            writeOps: !!writeOpsElement,
            avgResponseTime: !!avgResponseTimeElement
        });
        
        if (totalOpsElement) {
            totalOpsElement.textContent = totalOps > 0 ? formatNumber(totalOps) : '0';
            // Update trend indicator
            const totalOpsCard = totalOpsElement.closest('.metric-card');
            if (totalOpsCard) {
                const changeElement = totalOpsCard.querySelector('.metric-change');
                if (changeElement && previousTotalOps > 0) {
                    const change = totalOps - previousTotalOps;
                    const changePercent = ((change / previousTotalOps) * 100).toFixed(1);
                    const changeText = change > 0 ? `+${change} (${changePercent}%)` : `${change} (${changePercent}%)`;
                    const changeClass = change > 0 ? 'text-success' : change < 0 ? 'text-danger' : 'text-muted';
                    changeElement.innerHTML = `<small class="${changeClass}"><i class="bi bi-arrow-${change > 0 ? 'up' : 'down'}"></i> ${changeText}</small>`;
                }
            }
        }
        
        if (readOpsElement) {
            readOpsElement.textContent = readOps > 0 ? formatNumber(readOps) : '0';
            // Update trend indicator
            const readOpsCard = readOpsElement.closest('.metric-card');
            if (readOpsCard) {
                const changeElement = readOpsCard.querySelector('.metric-change');
                if (changeElement && previousReadOps > 0) {
                    const change = readOps - previousReadOps;
                    const changePercent = ((change / previousReadOps) * 100).toFixed(1);
                    const changeText = change > 0 ? `+${change} (${changePercent}%)` : `${change} (${changePercent}%)`;
                    const changeClass = change > 0 ? 'text-success' : change < 0 ? 'text-danger' : 'text-muted';
                    changeElement.innerHTML = `<small class="${changeClass}"><i class="bi bi-arrow-${change > 0 ? 'up' : 'down'}"></i> ${changeText}</small>`;
                }
            }
        }
        
        if (writeOpsElement) {
            writeOpsElement.textContent = writeOps > 0 ? formatNumber(writeOps) : '0';
            // Update trend indicator
            const writeOpsCard = writeOpsElement.closest('.metric-card');
            if (writeOpsCard) {
                const changeElement = writeOpsCard.querySelector('.metric-change');
                if (changeElement && previousWriteOps > 0) {
                    const change = writeOps - previousWriteOps;
                    const changePercent = ((change / previousWriteOps) * 100).toFixed(1);
                    const changeText = change > 0 ? `+${change} (${changePercent}%)` : `${change} (${changePercent}%)`;
                    const changeClass = change > 0 ? 'text-success' : change < 0 ? 'text-danger' : 'text-muted';
                    changeElement.innerHTML = `<small class="${changeClass}"><i class="bi bi-arrow-${change > 0 ? 'up' : 'down'}"></i> ${changeText}</small>`;
                }
            }
        }
        
        // Show real response time if available
        if (avgResponseTimeElement) {
            avgResponseTimeElement.textContent = avgResponseTime !== null ? `${avgResponseTime.toFixed(2)}ms` : 'N/A';
        }
        
        // Update cache metrics from time-series data
        let cacheHitRate = 0;
        let cacheHits = 0;
        let cacheMisses = 0;
        let previousCacheHitRate = 0;
        
        if (cacheDoc && cacheDoc.data && cacheDoc.data.length > 0) {
            const latestCache = cacheDoc.data[cacheDoc.data.length - 1];
            cacheHits = latestCache.hits || 0;
            cacheMisses = latestCache.misses || 0;
            const total = cacheHits + cacheMisses;
            cacheHitRate = total > 0 ? (cacheHits / total * 100) : 0;
            
            // Get previous hit rate for trend
            if (cacheDoc.data.length > 1) {
                const previousCache = cacheDoc.data[cacheDoc.data.length - 2];
                const prevHits = previousCache.hits || 0;
                const prevMisses = previousCache.misses || 0;
                const prevTotal = prevHits + prevMisses;
                previousCacheHitRate = prevTotal > 0 ? (prevHits / prevTotal * 100) : 0;
            }
        }
        
        // Update cache stats from current API if available
        if (cacheStats) {
            const size = cacheStats.size || 0;
            const memoryMB = (cacheStats.memory_mb || 0).toFixed(2);
            const cacheSizeElement = document.getElementById('cacheSize');
            const cacheMemoryElement = document.getElementById('cacheMemory');
            if (cacheSizeElement) cacheSizeElement.textContent = formatNumber(size);
            if (cacheMemoryElement) cacheMemoryElement.textContent = `${memoryMB} MB`;
        } else {
            const cacheSizeElement = document.getElementById('cacheSize');
            const cacheMemoryElement = document.getElementById('cacheMemory');
            if (cacheSizeElement) cacheSizeElement.textContent = '0';
            if (cacheMemoryElement) cacheMemoryElement.textContent = '0 MB';
        }
        
        // Update cache hit rate with trend
        const cacheHitRateElement = document.getElementById('cacheHitRate');
        if (cacheHitRateElement) {
            cacheHitRateElement.textContent = `${cacheHitRate.toFixed(1)}%`;
            const cacheCard = cacheHitRateElement.closest('.metric-card');
            if (cacheCard) {
                const changeElement = cacheCard.querySelector('.metric-change');
                if (changeElement && previousCacheHitRate > 0) {
                    const change = cacheHitRate - previousCacheHitRate;
                    const changeText = change > 0 ? `+${change.toFixed(1)}%` : `${change.toFixed(1)}%`;
                    const changeClass = change > 0 ? 'text-success' : change < 0 ? 'text-danger' : 'text-muted';
                    changeElement.innerHTML = `<small class="${changeClass}"><i class="bi bi-arrow-${change > 0 ? 'up' : 'down'}"></i> ${changeText}</small>`;
                }
            }
        }
        
                const cacheStatsElement = document.getElementById('cacheStats');
        if (cacheStatsElement) {
            cacheStatsElement.textContent = `${formatNumber(cacheHits)} hits / ${formatNumber(cacheMisses)} misses`;
        }
        
        // Process historical data for charts
        const labels = [];
        const readData = [];
        const writeData = [];
        const responseTimeData = [];
        
        // Extract data from operations time-series and convert to operations per second
        if (operationsDoc && operationsDoc.data && operationsDoc.data.length > 0) {
            // Calculate operations per second based on deltas between data points
            for (let i = 0; i < operationsDoc.data.length; i++) {
                const point = operationsDoc.data[i];
                const date = new Date(point.timestamp);
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
                
                // Calculate ops/sec from the delta since last data point
                if (i > 0) {
                    const prevPoint = operationsDoc.data[i - 1];
                    const timeDiff = (new Date(point.timestamp) - new Date(prevPoint.timestamp)) / 1000; // seconds
                    
                    // Calculate the delta in operations
                    const readDelta = (point.read || 0) - (prevPoint.read || 0);
                    const writeDelta = (point.write || 0) - (prevPoint.write || 0);
                    
                    // Convert to operations per second
                    const readOpsPerSec = timeDiff > 0 ? readDelta / timeDiff : 0;
                    const writeOpsPerSec = timeDiff > 0 ? writeDelta / timeDiff : 0;
                    
                    // Round to 2 decimal places and ensure non-negative
                    readData.push(Math.max(0, Math.round(readOpsPerSec * 100) / 100));
                    writeData.push(Math.max(0, Math.round(writeOpsPerSec * 100) / 100));
                } else {
                    // For the first point, we can't calculate a rate
                    readData.push(0);
                    writeData.push(0);
                }
            }
            
            console.log('Extracted operations per second data:', {
                labels: labels,
                readData: readData,
                writeData: writeData
            });
        }
        
        // Extract response time data from performance metrics
        if (performanceDoc && performanceDoc.data && performanceDoc.data.length > 0) {
            performanceDoc.data.forEach(point => {
                responseTimeData.push(point.avg_response_time_ms || 0);
            });
        }
        
        // If no data available, show empty state
        if (labels.length === 0) {
            labels.push('No data');
            readData.push(0);
            writeData.push(0);
            responseTimeData.push(0);
        }
        
        // Update operations chart with real data
        if (operationsChart) {
            try {
                operationsChart.data.labels = labels;
                if (operationsChart.data.datasets && operationsChart.data.datasets.length >= 2) {
                    operationsChart.data.datasets[0].data = readData;
                    operationsChart.data.datasets[1].data = writeData;
                }
                operationsChart.update();
            } catch (error) {
                console.error('Error updating operations chart:', error);
            }
        }
        
        // Calculate operation types for pie chart from latest data
        const currentReadOps = readOps;
        const currentWriteOps = writeOps;
        const currentDeleteOps = 0; // TODO: Add delete operations tracking
        const currentQueryOps = 0; // TODO: Add query operations tracking
        
        if (operationTypesChart) {
            try {
                const total = currentReadOps + currentWriteOps + currentDeleteOps + currentQueryOps;
                if (operationTypesChart.data.datasets && operationTypesChart.data.datasets[0]) {
                    operationTypesChart.data.datasets[0].data = [
                        currentReadOps,
                        currentWriteOps,
                        currentDeleteOps,
                        currentQueryOps
                    ];
                }
                
                // Update labels to show percentages
                if (total > 0) {
                    operationTypesChart.data.labels = [
                        `Read (${((currentReadOps/total)*100).toFixed(1)}%)`,
                        `Write (${((currentWriteOps/total)*100).toFixed(1)}%)`,
                        `Delete (${((currentDeleteOps/total)*100).toFixed(1)}%)`,
                        `Query (${((currentQueryOps/total)*100).toFixed(1)}%)`
                    ];
                }
                operationTypesChart.update();
            } catch (error) {
                console.error('Error updating operation types chart:', error);
            }
        }
        
        // Process cache hit rate data from time-series
        const cacheHitRateData = [];
        const cacheSizeData = [];
        if (cacheDoc && cacheDoc.data && cacheDoc.data.length > 0) {
            // Calculate hit rate and extract cache size for each time point
            cacheDoc.data.forEach(point => {
                const hits = point.hits || 0;
                const misses = point.misses || 0;
                const total = hits + misses;
                const hitRate = total > 0 ? (hits / total) : 0; // Store as decimal (0-1) for dual Y-axis
                const sizeBytes = point.size_bytes || 0;
                
                cacheHitRateData.push(hitRate);
                cacheSizeData.push(sizeBytes);
            });
            
            // Ensure cache data matches operations data length
            while (cacheHitRateData.length < labels.length) {
                cacheHitRateData.push(cacheHitRateData[cacheHitRateData.length - 1] || 0);
                cacheSizeData.push(cacheSizeData[cacheSizeData.length - 1] || 0);
            }
        } else {
            // Fill with zeros if no data
            for (let i = 0; i < labels.length; i++) {
                cacheHitRateData.push(0);
                cacheSizeData.push(0);
            }
        }
        
        // Update cache hit rate chart with dual Y-axis support
        if (cacheHitRateChart) {
            try {
                cacheHitRateChart.data.labels = labels;
                if (cacheHitRateChart.data.datasets && cacheHitRateChart.data.datasets[0]) {
                    cacheHitRateChart.data.datasets[0].data = cacheHitRateData;
                }
                // Update cache size data if second dataset exists (dual Y-axis configuration)
                if (cacheHitRateChart.data.datasets && cacheHitRateChart.data.datasets.length > 1) {
                    cacheHitRateChart.data.datasets[1].data = cacheSizeData;
                }
                cacheHitRateChart.update();
            } catch (error) {
                console.error('Error updating cache hit rate chart:', error);
            }
        }
        
        // Process memory data from time-series
        const memoryData = [];
        const processMemoryData = [];
        let latestMemoryStats = null;
        
        if (memoryDoc && memoryDoc.data && memoryDoc.data.length > 0) {
            // Extract memory usage over time
            memoryDoc.data.forEach(point => {
                const usedMB = (point.used_kb || 0) / 1024; // Convert KB to MB
                const processMB = (point.process_kb || 0) / 1024; // Convert KB to MB
                memoryData.push(Math.round(usedMB * 10) / 10); // Round to 1 decimal
                processMemoryData.push(Math.round(processMB * 10) / 10);
            });
            
            // Get latest memory stats for process memory chart
            latestMemoryStats = memoryDoc.data[memoryDoc.data.length - 1];
            
            // Ensure memory data matches operations data length
            while (memoryData.length < labels.length) {
                memoryData.push(memoryData[memoryData.length - 1] || 0);
                processMemoryData.push(processMemoryData[processMemoryData.length - 1] || 0);
            }
        } else {
            // Fill with zeros if no data
            for (let i = 0; i < labels.length; i++) {
                memoryData.push(0);
                processMemoryData.push(0);
            }
        }
        
        // Update memory usage chart with enhanced dual dataset support
        if (memoryUsageChart) {
            try {
                memoryUsageChart.data.labels = labels;
                if (memoryUsageChart.data.datasets && memoryUsageChart.data.datasets[0]) {
                    memoryUsageChart.data.datasets[0].data = processMemoryData;
                }
                // Update system memory data if second dataset exists (enhanced configuration)
                if (memoryUsageChart.data.datasets && memoryUsageChart.data.datasets.length > 1) {
                    memoryUsageChart.data.datasets[1].data = memoryData;
                }
                memoryUsageChart.update();
            } catch (error) {
                console.error('Error updating memory usage chart:', error);
            }
        }
        
        // Update database size chart
        if (databaseSizeChart) {
            try {
                // Use unified database size calculation for consistency
                const currentSizeBytes = await getActualDatabaseSize();
                const currentSizeMB = currentSizeBytes / (1024 * 1024);
                
                const dbSizeData = [];
                
                // For historical chart data, we'll show the current size across all time points
                // In a real implementation, you'd want to store historical size data
                for (let i = 0; i < labels.length; i++) {
                    dbSizeData.push(Math.round(currentSizeMB * 100) / 100);
                }
                
                databaseSizeChart.data.labels = labels;
                if (databaseSizeChart.data.datasets && databaseSizeChart.data.datasets[0]) {
                    databaseSizeChart.data.datasets[0].data = dbSizeData;
                }
                databaseSizeChart.update();
            } catch (error) {
                console.error('Error updating database size chart:', error);
            }
        }
        
        // Update storage chart with collection sizes
        if (storageChart && collections) {
            try {
                const collectionSizes = [];
                const collectionLabels = [];
                const collectionList = Array.isArray(collections) ? collections : (collections.collections || []);
                let totalSizeKB = 0;
                
                console.log('Collection list:', collectionList);
                
                // Collections API returns array of objects with {name, documentCount, isSystem}
                // Process collections directly from the response
                if (Array.isArray(collectionList)) {
                    collectionList.forEach(col => {
                        if (col && typeof col === 'object' && col.name) {
                            const docCount = col.documentCount || 0;
                            // Estimate size based on document count (rough estimate: 1KB per doc)
                            // For system collections without documents, use a minimum size
                            const estimatedSizeKB = docCount > 0 ? docCount * 1 : (col.isSystem ? 5 : 0);
                            totalSizeKB += estimatedSizeKB;
                            
                            console.log(`Collection ${col.name}: ${docCount} docs, ${estimatedSizeKB}KB`);
                            
                            if (estimatedSizeKB > 0) {
                                collectionSizes.push(estimatedSizeKB);
                                collectionLabels.push(col.name);
                            }
                        }
                    });
                }
                
                // Use unified database size calculation
                const totalSizeBytes = await getActualDatabaseSize();
                const displaySize = formatSize(totalSizeBytes);
                
                const dbSizeElement = document.getElementById('databaseSize');
                const collElement = document.getElementById('totalCollections');
                
                if (dbSizeElement) dbSizeElement.textContent = displaySize;
                if (collElement) collElement.textContent = `${collectionList.length} collections`;
                
                // Sort by size and take top 10
                const sizeData = collectionLabels.map((label, i) => ({
                    label: label,
                    size: collectionSizes[i]
                })).sort((a, b) => b.size - a.size).slice(0, 10);
                
                console.log('Storage chart data:', sizeData);
                
                if (sizeData.length > 0) {
                    storageChart.data.labels = sizeData.map(d => d.label);
                    if (storageChart.data.datasets && storageChart.data.datasets[0]) {
                        storageChart.data.datasets[0].data = sizeData.map(d => d.size);
                    }
                    storageChart.update();
                } else {
                    console.log('No data for storage chart, showing placeholder');
                    // Show placeholder data when no collections have documents
                    storageChart.data.labels = ['System Collections'];
                    if (storageChart.data.datasets && storageChart.data.datasets[0]) {
                        storageChart.data.datasets[0].data = [20]; // 20KB for system collections
                    }
                    storageChart.update();
                }
            } catch (error) {
                console.error('Error updating storage chart:', error);
            }
        }
        
        // Generate insights based on the data
        generateMetricsInsights({
            operationsDoc,
            performanceDoc,
            cacheDoc,
            memoryDoc,
            connectionsDoc,
            totalOps,
            readOps,
            writeOps,
            previousTotalOps,
            cacheHitRate,
            previousCacheHitRate,
            avgResponseTime
        });
        
    } catch (error) {
        console.error('Error loading metrics:', error);
        // Show N/A on error - with null checks
        const totalOpsElement = document.getElementById('totalOps');
        const readOpsElement = document.getElementById('readOps');
        const writeOpsElement = document.getElementById('writeOps');
        const avgResponseTimeElement = document.getElementById('avgResponseTime');
        
        if (totalOpsElement) totalOpsElement.textContent = 'N/A';
        if (readOpsElement) readOpsElement.textContent = 'N/A';
        if (writeOpsElement) writeOpsElement.textContent = 'N/A';
        if (avgResponseTimeElement) avgResponseTimeElement.textContent = 'N/A';
    }
}

function generateMetricsInsights(data) {
    const insightsContainer = document.getElementById('metricsInsights');
    if (!insightsContainer) return;
    
    const insights = [];
    
    // Analyze operations trend
    if (data.operationsDoc && data.operationsDoc.data && data.operationsDoc.data.length > 1) {
        const trend = analyzeOperationsTrend(data.operationsDoc.data);
        if (trend) insights.push(trend);
    }
    
    // Analyze cache performance
    if (data.cacheHitRate !== undefined) {
        const cacheInsight = analyzeCachePerformance(data.cacheHitRate, data.previousCacheHitRate);
        if (cacheInsight) insights.push(cacheInsight);
    }
    
    // Analyze read/write ratio
    if (data.readOps > 0 || data.writeOps > 0) {
        const ratioInsight = analyzeReadWriteRatio(data.readOps, data.writeOps);
        if (ratioInsight) insights.push(ratioInsight);
    }
    
    // Analyze response time
    if (data.avgResponseTime !== null) {
        const perfInsight = analyzePerformance(data.avgResponseTime, data.performanceDoc);
        if (perfInsight) insights.push(perfInsight);
    }
    
    // Analyze system health
    const healthInsight = analyzeSystemHealth(data);
    if (healthInsight) insights.push(healthInsight);
    
    // Render insights
    if (insights.length > 0) {
        insightsContainer.innerHTML = `
            <div class="insights-list">
                ${insights.map(insight => `
                    <div class="insight-item ${insight.type}">
                        <div class="insight-icon">
                            <i class="bi ${insight.icon}"></i>
                        </div>
                        <div class="insight-content">
                            <h6>${insight.title}</h6>
                            <p>${insight.description}</p>
                            ${insight.recommendation ? `<small class="text-muted"><i class="bi bi-lightbulb"></i> ${insight.recommendation}</small>` : ''}
                        </div>
                    </div>
                `).join('')}
            </div>
        `;
    } else {
        insightsContainer.innerHTML = `
            <div class="text-center text-muted py-4">
                <i class="bi bi-check-circle" style="font-size: 2rem;"></i>
                <p>System is performing optimally. No issues detected.</p>
            </div>
        `;
    }
}

function analyzeOperationsTrend(data) {
    if (data.length < 3) return null;
    
    // Calculate trend over last 3 data points
    const recent = data.slice(-3);
    const totalOps = recent.map(d => d.total || 0);
    const avgIncrease = (totalOps[2] - totalOps[0]) / 2;
    const percentChange = totalOps[0] > 0 ? ((totalOps[2] - totalOps[0]) / totalOps[0] * 100) : 0;
    
    if (Math.abs(percentChange) < 5) return null; // Ignore small changes
    
    if (percentChange > 20) {
        return {
            type: 'warning',
            icon: 'bi-graph-up-arrow',
            title: 'High Traffic Detected',
            description: `Operations increased by ${percentChange.toFixed(1)}% in the last ${recent.length} intervals.`,
            recommendation: 'Monitor system resources. Consider scaling if this trend continues.'
        };
    } else if (percentChange < -20) {
        return {
            type: 'info',
            icon: 'bi-graph-down-arrow',
            title: 'Reduced Activity',
            description: `Operations decreased by ${Math.abs(percentChange).toFixed(1)}% in the last ${recent.length} intervals.`,
            recommendation: 'This may indicate reduced usage or potential connectivity issues.'
        };
    }
    
    return null;
}

function analyzeCachePerformance(currentRate, previousRate) {
    if (currentRate < 50) {
        return {
            type: 'warning',
            icon: 'bi-speedometer2',
            title: 'Low Cache Hit Rate',
            description: `Current cache hit rate is ${currentRate.toFixed(1)}%, which is below optimal levels.`,
            recommendation: 'Consider adjusting cache size or reviewing query patterns to improve performance.'
        };
    } else if (previousRate > 0 && currentRate - previousRate < -10) {
        return {
            type: 'info',
            icon: 'bi-arrow-down-circle',
            title: 'Cache Performance Degraded',
            description: `Cache hit rate dropped by ${(previousRate - currentRate).toFixed(1)}% from the previous period.`,
            recommendation: 'Review recent query patterns or data changes that might affect cache efficiency.'
        };
    } else if (currentRate > 90) {
        return {
            type: 'success',
            icon: 'bi-trophy',
            title: 'Excellent Cache Performance',
            description: `Cache hit rate is ${currentRate.toFixed(1)}%, indicating highly efficient caching.`
        };
    }
    
    return null;
}

function analyzeReadWriteRatio(reads, writes) {
    const total = reads + writes;
    if (total === 0) return null;
    
    const readPercent = (reads / total) * 100;
    const writePercent = (writes / total) * 100;
    
    if (writePercent > 40) {
        return {
            type: 'info',
            icon: 'bi-pencil-square',
            title: 'Write-Heavy Workload',
            description: `${writePercent.toFixed(1)}% of operations are writes, indicating a write-intensive workload.`,
            recommendation: 'Ensure write performance is optimized and consider batching writes if possible.'
        };
    } else if (readPercent > 95) {
        return {
            type: 'info',
            icon: 'bi-book',
            title: 'Read-Heavy Workload',
            description: `${readPercent.toFixed(1)}% of operations are reads. Cache optimization is crucial.`,
            recommendation: 'Maximize cache efficiency and consider read replicas for scaling.'
        };
    }
    
    return null;
}

function analyzePerformance(avgResponseTime, performanceDoc) {
    if (avgResponseTime > 100) {
        return {
            type: 'warning',
            icon: 'bi-clock-history',
            title: 'High Response Times',
            description: `Average response time is ${avgResponseTime.toFixed(2)}ms, which may impact user experience.`,
            recommendation: 'Review slow queries and consider optimizing database indices.'
        };
    } else if (avgResponseTime < 10) {
        return {
            type: 'success',
            icon: 'bi-lightning',
            title: 'Excellent Performance',
            description: `Average response time is ${avgResponseTime.toFixed(2)}ms, indicating fast query processing.`
        };
    }
    
    return null;
}

function analyzeSystemHealth(data) {
    const issues = [];
    
    if (data.totalOps === 0) {
        issues.push('No operations recorded');
    }
    
    if (data.cacheHitRate !== undefined && data.cacheHitRate < 30) {
        issues.push('Very low cache hit rate');
    }
    
    if (data.avgResponseTime && data.avgResponseTime > 200) {
        issues.push('Very high response times');
    }
    
    if (issues.length > 0) {
        return {
            type: 'danger',
            icon: 'bi-exclamation-triangle',
            title: 'System Health Issues Detected',
            description: `The following issues need attention: ${issues.join(', ')}.`,
            recommendation: 'Review system logs and consider immediate investigation.'
        };
    }
    
    return null;
}

function changeTimeRange(range) {
    // Update active button
    document.querySelectorAll('.time-range-selector .btn').forEach(btn => {
        btn.classList.remove('active');
        if (btn.getAttribute('onclick').includes(range)) {
            btn.classList.add('active');
        }
    });
    
    // Store current time range
    window.currentMetricsTimeRange = range;
    
    // Reload metrics with new time range
    loadMetrics(range);
}

// ===== PERFORMANCE DASHBOARD FUNCTIONALITY =====

// Update performance dashboard display
function updatePerformanceDashboard() {
    const dashboardData = getPerformanceDashboardData();
    
    // Update health score
    updateHealthScore(dashboardData.overall);
    
    // Update performance type cards
    updatePerformanceTypeCard('validation', dashboardData.validation);
    updatePerformanceTypeCard('transformation', dashboardData.transformation);
    updatePerformanceTypeCard('function_execution', dashboardData.function_execution);
    
    // Update optimization suggestions
    updateOptimizationSuggestions(dashboardData);
}

// Update health score display
function updateHealthScore(overallData) {
    const scoreElement = document.getElementById('healthScoreValue');
    const statusElement = document.getElementById('healthStatus');
    const circleElement = document.getElementById('healthScoreCircle');
    const criticalElement = document.getElementById('criticalCount');
    const warningElement = document.getElementById('warningCount');
    const totalElement = document.getElementById('totalSuggestions');
    
    if (!scoreElement || !overallData) return;
    
    const score = overallData.healthScore || 100;
    scoreElement.textContent = score;
    
    // Update status text and color
    let status, colorClass;
    if (score >= 90) {
        status = 'Excellent';
        colorClass = 'excellent';
    } else if (score >= 70) {
        status = 'Good';
        colorClass = 'good';
    } else if (score >= 50) {
        status = 'Fair';
        colorClass = 'fair';
    } else {
        status = 'Poor';
        colorClass = 'poor';
    }
    
    statusElement.textContent = status;
    
    // Update circle color
    circleElement.className = `health-score-circle ${colorClass}`;
    
    // Update summary counts
    if (criticalElement) criticalElement.textContent = overallData.criticalIssues || 0;
    if (warningElement) warningElement.textContent = overallData.warnings || 0;
    if (totalElement) totalElement.textContent = overallData.totalSuggestions || 0;
}

// Update performance type card
function updatePerformanceTypeCard(type, data) {
    if (!data) {
        console.warn(`No performance data available for ${type}`);
        return;
    }
    
    const stats = data.stats;
    const suggestions = data.suggestions || [];
    
    // Update stats
    const avgTimeElement = document.getElementById(`${type}AvgTime`);
    const scriptCountElement = document.getElementById(`${type}ScriptCount`);
    const errorRateElement = document.getElementById(`${type}ErrorRate`);
    const suggestionsElement = document.getElementById(`${type}Suggestions`);
    
    if (stats) {
        if (avgTimeElement) avgTimeElement.textContent = `${stats.avgTime}ms`;
        if (scriptCountElement) scriptCountElement.textContent = stats.avgScriptCount;
        if (errorRateElement) errorRateElement.textContent = `${stats.avgErrorRate}%`;
    } else {
        if (avgTimeElement) avgTimeElement.textContent = '-';
        if (scriptCountElement) scriptCountElement.textContent = '-';
        if (errorRateElement) errorRateElement.textContent = '-';
    }
    
    // Update suggestions for this type
    if (suggestionsElement) {
        if (suggestions.length > 0) {
            suggestionsElement.innerHTML = suggestions.slice(0, 2).map(suggestion => 
                `<div class="type-suggestion ${suggestion.type}">
                    <i class="bi bi-${suggestion.type === 'critical' ? 'exclamation-triangle-fill' : 
                                   suggestion.type === 'warning' ? 'exclamation-triangle' : 'info-circle'}"></i>
                    <span>${suggestion.message}</span>
                </div>`
            ).join('');
        } else {
            suggestionsElement.innerHTML = '<div class="type-suggestion success"><i class="bi bi-check-circle"></i><span>No issues</span></div>';
        }
    }
}

// Update optimization suggestions
function updateOptimizationSuggestions(dashboardData) {
    const suggestionsListElement = document.getElementById('suggestionsList');
    if (!suggestionsListElement) return;
    
    const allSuggestions = [
        ...(dashboardData.validation?.suggestions || []),
        ...(dashboardData.transformation?.suggestions || []),
        ...(dashboardData.function_execution?.suggestions || [])
    ];
    
    if (allSuggestions.length === 0) {
        suggestionsListElement.innerHTML = `
            <div class="no-suggestions text-muted">
                <i class="bi bi-check-circle"></i> No performance issues detected
            </div>
        `;
        return;
    }
    
    // Sort by priority
    const priorityOrder = { high: 3, medium: 2, low: 1 };
    allSuggestions.sort((a, b) => priorityOrder[b.priority] - priorityOrder[a.priority]);
    
    suggestionsListElement.innerHTML = allSuggestions.map(suggestion => `
        <div class="suggestion-item ${suggestion.type}" data-priority="${suggestion.priority}">
            <div class="suggestion-header">
                <div class="suggestion-icon">
                    <i class="bi bi-${suggestion.type === 'critical' ? 'exclamation-triangle-fill text-danger' : 
                                    suggestion.type === 'warning' ? 'exclamation-triangle text-warning' : 'info-circle text-info'}"></i>
                </div>
                <div class="suggestion-content">
                    <div class="suggestion-title">${suggestion.message}</div>
                    <div class="suggestion-text">${suggestion.suggestion}</div>
                </div>
                <div class="suggestion-category">
                    <span class="badge bg-${suggestion.type === 'critical' ? 'danger' : 
                                          suggestion.type === 'warning' ? 'warning' : 'info'}">${suggestion.category}</span>
                </div>
            </div>
        </div>
    `).join('');
}

// Refresh performance dashboard
function refreshPerformanceDashboard() {
    updatePerformanceDashboard();
    showOperationResult(true, 'Performance dashboard refreshed');
}

// Clear performance history
function clearPerformanceHistory() {
    if (confirm('Are you sure you want to clear all performance history? This action cannot be undone.')) {
        performanceMetrics = {
            validation: [],
            transformation: [],
            function_execution: []
        };
        
        try {
            localStorage.removeItem('jdbx_performance_metrics');
        } catch (e) {
            console.warn('Failed to clear performance metrics from localStorage:', e);
        }
        
        updatePerformanceDashboard();
        showOperationResult(true, 'Performance history cleared');
    }
}

// Show performance report
function showPerformanceReport() {
    const dashboardData = getPerformanceDashboardData();
    const report = generatePerformanceReport(dashboardData);
    
    // Create modal to show report
    const modal = document.createElement('div');
    modal.className = 'modal fade';
    modal.innerHTML = `
        <div class="modal-dialog modal-lg">
            <div class="modal-content">
                <div class="modal-header">
                    <h5 class="modal-title"><i class="bi bi-file-earmark-text"></i> Performance Report</h5>
                    <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                </div>
                <div class="modal-body">
                    <pre class="performance-report">${report}</pre>
                </div>
                <div class="modal-footer">
                    <button type="button" class="btn btn-unified" onclick="copyPerformanceReport()">
                        <i class="bi bi-clipboard"></i> Copy Report
                    </button>
                    <button type="button" class="btn btn-unified-primary" data-bs-dismiss="modal">Close</button>
                </div>
            </div>
        </div>
    `;
    
    document.body.appendChild(modal);
    const bootstrapModal = new bootstrap.Modal(modal);
    bootstrapModal.show();
    
    // Clean up modal when closed
    modal.addEventListener('hidden.bs.modal', () => {
        document.body.removeChild(modal);
    });
}

// Generate performance report
function generatePerformanceReport(dashboardData) {
    const timestamp = new Date().toISOString();
    
    let report = `JDBX JavaScript Performance Report
Generated: ${timestamp}
======================================

OVERALL HEALTH SCORE: ${dashboardData.overall.healthScore}/100
Status: ${dashboardData.overall.healthScore >= 90 ? 'Excellent' : 
          dashboardData.overall.healthScore >= 70 ? 'Good' : 
          dashboardData.overall.healthScore >= 50 ? 'Fair' : 'Poor'}

Summary:
- Critical Issues: ${dashboardData.overall.criticalIssues}
- Warnings: ${dashboardData.overall.warnings}
- Total Suggestions: ${dashboardData.overall.totalSuggestions}

PERFORMANCE BY TYPE
==================

Validation:
${formatTypeStats(dashboardData.validation)}

Transformation:
${formatTypeStats(dashboardData.transformation)}

Function Execution:
${formatTypeStats(dashboardData.function_execution)}

OPTIMIZATION SUGGESTIONS
=======================
`;

    const allSuggestions = [
        ...dashboardData.validation.suggestions,
        ...dashboardData.transformation.suggestions,
        ...dashboardData.function_execution.suggestions
    ];

    if (allSuggestions.length === 0) {
        report += '\nNo performance issues detected. Your JavaScript scripts are performing well!\n';
    } else {
        allSuggestions.forEach((suggestion, index) => {
            report += `\n${index + 1}. [${suggestion.type.toUpperCase()}] ${suggestion.message}
   Category: ${suggestion.category}
   Suggestion: ${suggestion.suggestion}
   Priority: ${suggestion.priority}
`;
        });
    }
    
    return report;
}

// Format type stats for report
function formatTypeStats(typeData) {
    if (!typeData.stats) {
        return '  No data available\n';
    }
    
    const stats = typeData.stats;
    return `  Average Time: ${stats.avgTime}ms
  Script Count: ${stats.avgScriptCount}
  Error Rate: ${stats.avgErrorRate}%
  Sample Size: ${stats.sampleSize} executions
  Issues: ${typeData.suggestions.length}
`;
}

// Export performance data
function exportPerformanceData() {
    const data = {
        timestamp: new Date().toISOString(),
        dashboard: getPerformanceDashboardData(),
        metrics: performanceMetrics
    };
    
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `jdbx-performance-${new Date().toISOString().split('T')[0]}.json`;
    a.click();
    URL.revokeObjectURL(url);
    
    showOperationResult(true, 'Performance data exported');
}

// Initialize performance dashboard when metrics are loaded
function initializePerformanceDashboard() {
    if (currentView === 'metrics') {
        updatePerformanceDashboard();
        
        // Set up periodic updates
        setInterval(() => {
            if (currentView === 'metrics') {
                updatePerformanceDashboard();
            }
        }, 30000); // Update every 30 seconds
    }
}

// ===== RBAC FUNCTIONALITY =====
function initializeRBAC() {
    console.log('=== INITIALIZING RBAC VIEW ===');
    const rbacView = document.getElementById('rbac-view');
    console.log('RBAC View element:', rbacView);
    console.log('RBAC View HTML length:', rbacView ? rbacView.innerHTML.length : 'null');
    console.log('RBAC View text content:', rbacView ? rbacView.textContent.trim().substring(0, 100) : 'null');
    
    
    // Monitor for content being overwritten
    const rbacInterval = setInterval(() => {
        const rbacView = document.getElementById('rbac-view');
        if (rbacView && rbacView.textContent.trim() === 'admin') {
            console.error('DETECTED: RBAC view has been overwritten with "admin"!');
            console.error('Current HTML length:', rbacView.innerHTML.length);
            console.error('This should not happen!');
            clearInterval(rbacInterval);
            
            // Try to restore it
            const rolesTab = document.getElementById('roles');
            if (rolesTab) {
                console.log('Attempting to restore by re-showing roles tab');
                rolesTab.classList.add('show', 'active');
            }
        }
    }, 100);
    
    // Clear monitor after 5 seconds
    setTimeout(() => clearInterval(rbacInterval), 5000);
    
    // Add Bootstrap tab event listeners
    const rbacTabs = document.querySelectorAll('#rbacTabs button[data-bs-toggle="tab"]');
    rbacTabs.forEach(tab => {
        tab.addEventListener('shown.bs.tab', function(event) {
            console.log('Tab shown:', event.target.getAttribute('data-bs-target'));
            const targetId = event.target.getAttribute('data-bs-target').substring(1); // Remove #
            
            // Render content for the shown tab
            switch(targetId) {
                case 'users':
                    if (allUsers.length > 0) renderUsers();
                    break;
                case 'roles':
                    if (allRoles.length > 0) renderRoles();
                    break;
                case 'permissions':
                    renderPermissionMatrix();
                    break;
                case 'sessions':
                    if (window.lastSessionsData && window.lastSessionsData.length > 0) {
                        renderSessions(window.lastSessionsData);
                    } else {
                        // If no sessions data, load it
                        loadSessions();
                    }
                    break;
            }
        });
    });
    
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
            loadAuditLog(),
            loadSessions()
        ]);
        
        // After loading, ensure the active tab's content is rendered
        const activeTab = document.querySelector('#rbacTabContent .tab-pane.active');
        if (activeTab) {
            console.log('Active tab after load:', activeTab.id);
            // If roles tab is active but empty, re-render
            if (activeTab.id === 'roles') {
                const rolesList = document.getElementById('rolesList');
                if (rolesList && !rolesList.innerHTML.trim()) {
                    console.log('Roles tab is active but empty, re-rendering');
                    renderRoles();
                }
            }
        }
        
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
    // Only render if we're in RBAC view and users tab is active
    if (currentView !== 'rbac') {
        console.log('Not in RBAC view, skipping renderUsers');
        return;
    }
    
    const usersTabPane = document.getElementById('users');
    if (!usersTabPane || !usersTabPane.classList.contains('active')) {
        console.log('Users tab not active, skipping render');
        return;
    }
    
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
    
    tbody.innerHTML = allUsers.map(user => {
        // Map role IDs to role names
        const roleNames = (user.roles || []).map(roleId => {
            const role = allRoles.find(r => r.id === roleId);
            return role ? role.name : roleId;
        });
        
        return `
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
                    ${roleNames.map(roleName => 
                        `<span class="badge bg-primary me-1">${roleName}</span>`
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
        `;
    }).join('');
}

async function loadRoles() {
    console.log('=== loadRoles called ===');
    console.log('Current view:', currentView);
    console.log('Auth token exists:', !!localStorage.getItem('jdbx_auth_token'));
    
    try {
        const response = await apiRequest('/api/rbac/roles');
        console.log('Roles API response:', response);
        
        // Handle both direct array and object with roles property
        allRoles = Array.isArray(response) ? response : (response.roles || []);
        console.log('Processed roles:', allRoles);
        
        renderRoles();
        populateRoleSelects();
    } catch (error) {
        console.error('Error loading roles:', error);
        console.log('Error details:', error.message, error.stack);
        // Show error message in the roles tab
        const rolesList = document.getElementById('rolesList');
        if (rolesList) {
            let errorMessage = 'Failed to load roles';
            if (error.message?.includes('Unauthorized') || error.message?.includes('401')) {
                errorMessage = 'Authentication expired. Please log in again.';
            } else if (error.message) {
                errorMessage = error.message;
            }
            
            rolesList.innerHTML = `
                <div class="text-center text-muted py-4">
                    <i class="bi bi-exclamation-triangle" style="font-size: 2rem; color: var(--bs-danger);"></i>
                    <p class="text-danger">${errorMessage}</p>
                    ${error.message?.includes('Unauthorized') ? 
                        '<button class="btn btn-primary btn-sm" onclick="window.location.href=\'/login.html\'">Go to Login</button>' : 
                        '<button class="btn btn-secondary btn-sm" onclick="loadRoles()">Retry</button>'
                    }
                </div>
            `;
        }
        // Set empty array to prevent errors in other functions
        allRoles = [];
    }
}

function renderRoles() {
    console.log('=== renderRoles called ===');
    
    // Ensure we're in the RBAC view
    if (currentView !== 'rbac') {
        console.log('Not in RBAC view, skipping renderRoles');
        return;
    }
    
    // Check if roles tab exists and is active
    const rolesTabPane = document.getElementById('roles');
    if (!rolesTabPane) {
        console.log('Roles tab pane not found');
        return;
    }
    
    // Only render if the roles tab is currently active
    if (!rolesTabPane.classList.contains('active')) {
        console.log('Roles tab not active, skipping render');
        return;
    }
    
    // Now wait a moment for DOM to update
    setTimeout(() => {
        const rolesList = document.getElementById('rolesList');
        console.log('rolesList element:', rolesList);
        console.log('Roles tab classes:', rolesTabPane ? rolesTabPane.className : 'null');
        
        if (!rolesList) {
            console.error('rolesList element not found!');
            console.error('Roles tab HTML preview:', rolesTabPane ? rolesTabPane.innerHTML.substring(0, 200) : 'null');
            return;
        }
    
        console.log('renderRoles called with:', allRoles);
    
    if (!Array.isArray(allRoles) || allRoles.length === 0) {
        rolesList.innerHTML = `
            <div class="text-center text-muted py-4">
                <i class="bi bi-shield-x" style="font-size: 2rem;"></i>
                <p>No roles found</p>
            </div>
        `;
        return;
    }
    
    try {
        rolesList.innerHTML = allRoles.map(role => `
            <div class="card role-card mb-2 ${selectedRole?.id === role.id ? 'active' : ''}" 
                 onclick="selectRole('${role.id}')">
                <div class="card-body">
                    <h6 class="card-title mb-1">${role.name}</h6>
                    <p class="card-text small text-muted mb-2">${role.description || 'No description'}</p>
                    <div>
                        <span class="badge bg-secondary">${(role.users || []).length} users</span>
                        <span class="badge bg-info">${Object.keys(role.permissions || {}).length} permissions</span>
                    </div>
                </div>
            </div>
        `).join('');
    } catch (error) {
        console.error('Error rendering roles:', error);
        rolesList.innerHTML = '<div class="alert alert-danger">Error rendering roles</div>';
    }
    }, 0); // Close setTimeout
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
            ${Object.entries(selectedRole.permissions || {}).map(([resource, perms]) => {
                const permBitmask = parseInt(perms);
                const permStrings = [];
                if (permBitmask & 1) permStrings.push('Read');
                if (permBitmask & 2) permStrings.push('Write');
                if (permBitmask & 4) permStrings.push('Delete');
                if (permBitmask & 8) permStrings.push('Admin');
                return `<div class="mb-2">
                    <span class="text-muted small">${resource}:</span>
                    ${permStrings.map(p => `<span class="permission-badge">${p}</span>`).join(' ')}
                </div>`;
            }).join('')}
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
        
        // Extract permissions from roles
        const permissionsByResource = {};
        const roles = response.roles || [];
        const resourceTypes = response.resource_types || {};
        const permissionTypes = response.permission_types || {};
        
        // Process each role's permissions
        roles.forEach(role => {
            if (role.permissions) {
                Object.entries(role.permissions).forEach(([key, value]) => {
                    // Parse the resource type and ID from the key (e.g., "1:collection1" -> type: 1, id: collection1)
                    const [typeNum, ...idParts] = key.split(':');
                    const resourceId = idParts.join(':') || '*';
                    const resourceType = resourceTypes[typeNum] || 'unknown';
                    const resourceName = `${resourceType}:${resourceId}`;
                    
                    if (!permissionsByResource[resourceName]) {
                        permissionsByResource[resourceName] = {
                            roles: [],
                            permissions: {}
                        };
                    }
                    
                    // Add role name if not already there
                    if (!permissionsByResource[resourceName].roles.includes(role.name)) {
                        permissionsByResource[resourceName].roles.push(role.name);
                    }
                    
                    // Parse permission bitmask
                    const permBitmask = parseInt(value);
                    if (permBitmask & 1) permissionsByResource[resourceName].permissions.read = true;
                    if (permBitmask & 2) permissionsByResource[resourceName].permissions.write = true;
                    if (permBitmask & 4) permissionsByResource[resourceName].permissions.delete = true;
                    if (permBitmask & 8) permissionsByResource[resourceName].permissions.admin = true;
                });
            }
        });
        
        renderPermissionMatrix(permissionsByResource);
    } catch (error) {
        console.error('Error loading permissions:', error);
    }
}

function renderPermissionMatrix(permissionsByResource) {
    const tbody = document.getElementById('permissionsTableBody');
    if (!tbody) return;
    
    if (Object.keys(permissionsByResource).length === 0) {
        tbody.innerHTML = `
            <tr>
                <td colspan="6" class="text-center text-muted py-4">
                    <i class="bi bi-shield-x" style="font-size: 2rem;"></i>
                    <p>No permissions configured</p>
                </td>
            </tr>
        `;
        return;
    }
    
    tbody.innerHTML = Object.entries(permissionsByResource).map(([resource, data]) => {
        const perms = data.permissions;
        return `
            <tr>
                <td>
                    <div class="fw-bold">${resource}</div>
                    <small class="text-muted">Roles: ${data.roles.join(', ')}</small>
                </td>
                <td class="text-center">
                    ${perms.create ? '<span class="badge bg-success">✓</span>' : '<span class="badge bg-light text-muted">−</span>'}
                </td>
                <td class="text-center">
                    ${perms.read ? '<span class="badge bg-success">✓</span>' : '<span class="badge bg-light text-muted">−</span>'}
                </td>
                <td class="text-center">
                    ${perms.write ? '<span class="badge bg-success">✓</span>' : '<span class="badge bg-light text-muted">−</span>'}
                </td>
                <td class="text-center">
                    ${perms.delete ? '<span class="badge bg-success">✓</span>' : '<span class="badge bg-light text-muted">−</span>'}
                </td>
                <td class="text-center">
                    ${perms.admin ? '<span class="badge bg-success">✓</span>' : '<span class="badge bg-light text-muted">−</span>'}
                </td>
            </tr>
        `;
    }).join('');
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

// Sessions management functions
async function loadSessions() {
    try {
        const response = await apiRequest('/api/collections/_sessions');
        const sessions = response.documents || [];
        window.lastSessionsData = sessions; // Store for tab switching
        renderSessions(sessions);
    } catch (error) {
        console.error('Error loading sessions:', error);
        window.lastSessionsData = []; // Store empty array on error
        renderSessionsError();
    }
}

function renderSessions(sessions) {
    // Only render if we're in RBAC view and sessions tab is active
    if (currentView !== 'rbac') {
        console.log('Not in RBAC view, skipping renderSessions');
        return;
    }
    
    const sessionsTabPane = document.getElementById('sessions');
    if (!sessionsTabPane || !sessionsTabPane.classList.contains('active')) {
        console.log('Sessions tab not active, skipping render');
        return;
    }
    
    const tbody = document.getElementById('sessionsTableBody');
    if (!tbody) return;
    
    // Get filter value
    const filterSelect = document.getElementById('sessionFilter');
    const filterValue = filterSelect ? filterSelect.value : 'active';
    
    // Filter sessions based on selected filter
    const filteredSessions = sessions.filter(session => {
        const expiresAt = new Date(session.expires_at || Date.now() + 86400000);
        const isExpired = expiresAt < new Date();
        const isActive = session.active !== false && !isExpired;
        
        switch (filterValue) {
            case 'active':
                return isActive;
            case 'expired':
                return !isActive;
            case 'all':
            default:
                return true;
        }
    });
    
    if (filteredSessions.length === 0) {
        const emptyMessage = filterValue === 'active' ? 'No active sessions' :
                           filterValue === 'expired' ? 'No expired sessions' :
                           'No sessions found';
        tbody.innerHTML = `
            <tr>
                <td colspan="9" class="text-center text-muted py-4">
                    <i class="bi bi-clock-history" style="font-size: 2rem;"></i>
                    <p>${emptyMessage}</p>
                </td>
            </tr>
        `;
        return;
    }
    
    tbody.innerHTML = filteredSessions.map(session => {
        const createdAt = new Date(session.created_at || Date.now());
        const lastActivity = new Date(session.last_seen || session.last_activity || session.created_at || Date.now());
        const expiresAt = new Date(session.expires_at || Date.now() + 86400000);
        const now = new Date();
        const isExpired = expiresAt < now;
        const status = session.active === false ? 'Terminated' : (isExpired ? 'Expired' : 'Active');
        const statusClass = session.active === false ? 'bg-secondary' : (isExpired ? 'bg-danger' : 'bg-success');
        
        // Check if session was recently extended (last activity within 2 minutes)
        const recentlyActive = (now - lastActivity) < (2 * 60 * 1000);
        const rowClass = recentlyActive && !isExpired ? 'table-success' : '';
        
        // Calculate time remaining
        const timeRemaining = expiresAt - now;
        let timeRemainingText = '';
        if (timeRemaining > 0) {
            const minutes = Math.floor(timeRemaining / (60 * 1000));
            if (minutes > 60) {
                const hours = Math.floor(minutes / 60);
                const remainingMins = minutes % 60;
                timeRemainingText = `${hours}h ${remainingMins}m`;
            } else {
                timeRemainingText = `${minutes}m`;
            }
            timeRemainingText = `<small class="text-muted">(${timeRemainingText} left)</small>`;
        }
        
        // Format IP address
        const ipAddress = session.ip_address || 'Unknown';
        
        // Format user agent - truncate if too long
        let userAgent = session.user_agent || 'Unknown';
        if (userAgent.length > 50) {
            userAgent = userAgent.substring(0, 47) + '...';
        }
        
        return `
            <tr class="${rowClass}">
                <td class="font-monospace small">${session.uuid || session._id || session.id || 'N/A'}</td>
                <td>${session.username || session.user || 'Unknown'}</td>
                <td class="font-monospace">${ipAddress}</td>
                <td class="small" title="${session.user_agent || 'Unknown'}">${userAgent}</td>
                <td>${formatDate(createdAt)}</td>
                <td>${formatDate(lastActivity)}</td>
                <td>${formatDate(expiresAt)} ${timeRemainingText}</td>
                <td>
                    <span class="badge ${statusClass}">
                        ${status}
                    </span>
                </td>
                <td>
                    <button class="btn btn-sm btn-outline-danger" onclick="revokeSession('${session.uuid || session._id || session.id}')">
                        <i class="bi bi-x-circle"></i> Revoke
                    </button>
                </td>
            </tr>
        `;
    }).join('');
}

function renderSessionsError() {
    const tbody = document.getElementById('sessionsTableBody');
    if (tbody) {
        tbody.innerHTML = `
            <tr>
                <td colspan="9" class="text-center text-danger py-4">
                    <i class="bi bi-exclamation-triangle" style="font-size: 2rem;"></i>
                    <p>Failed to load sessions</p>
                </td>
            </tr>
        `;
    }
}

async function refreshSessions() {
    showNotification('Refreshing sessions...', 'info');
    await loadSessions();
    showNotification('Sessions refreshed', 'success');
}

function filterSessions() {
    // Re-render sessions with current data when filter changes
    if (window.lastSessionsData) {
        renderSessions(window.lastSessionsData);
    }
}

async function clearAllSessions() {
    if (!confirm('Are you sure you want to clear all sessions? This will log out all users.')) {
        return;
    }
    
    try {
        // Get all sessions
        const response = await apiRequest('/api/collections/_sessions');
        const sessions = response.documents || [];
        
        // Delete each session
        await Promise.all(sessions.map(session => 
            apiRequest(`/api/collections/_sessions/documents/${session.uuid || session._id}`, {
                method: 'DELETE'
            })
        ));
        
        showNotification('All sessions cleared', 'success');
        await loadSessions();
    } catch (error) {
        console.error('Error clearing sessions:', error);
        showNotification('Failed to clear sessions', 'error');
    }
}

async function revokeSession(sessionId) {
    if (!confirm('Are you sure you want to revoke this session?')) {
        return;
    }
    
    try {
        // Use the proper session termination endpoint
        await apiRequest(`/api/sessions/${sessionId}/terminate`, {
            method: 'POST'
        });
        
        showNotification('Session revoked', 'success');
        await loadSessions();
    } catch (error) {
        console.error('Error revoking session:', error);
        showNotification('Failed to revoke session', 'error');
    }
}

function populateRoleSelects() {
    // Be specific - only target select elements, not the tab pane
    const roleSelects = document.querySelectorAll('select#userRolesSelect, select[name="roles"]');
    roleSelects.forEach(roleSelect => {
        if (roleSelect && roleSelect.tagName === 'SELECT') {
            roleSelect.innerHTML = allRoles.map(role => 
                `<option value="${role.id}">${role.name}</option>`
            ).join('');
        }
    });
}

// ===== API DOCUMENTATION =====
function initializeAPI() {
    if (!window.swaggerUI) {
        const token = localStorage.getItem('jdbx_auth_token');
        
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

// ===== GLOBAL ERROR HANDLING =====
// Add global error handler to prevent JavaScript errors from breaking the dashboard
window.addEventListener('error', function(event) {
    console.error('Global JavaScript error:', event.error);
    // Don't prevent default to allow normal error reporting
});

window.addEventListener('unhandledrejection', function(event) {
    console.error('Unhandled promise rejection:', event.reason);
    // Don't prevent default to allow normal error reporting
});

// ===== UTILITY FUNCTIONS =====
function formatNumber(num) {
    // Handle null, undefined, and non-numeric values
    if (num === null || num === undefined || isNaN(num)) {
        return '0';
    }
    
    // Convert to number if it's a string
    const numValue = typeof num === 'string' ? parseFloat(num) : num;
    
    // Handle invalid conversions
    if (isNaN(numValue)) {
        return '0';
    }
    
    if (numValue >= 1000000) return `${(numValue / 1000000).toFixed(1)}M`;
    if (numValue >= 1000) return `${(numValue / 1000).toFixed(1)}K`;
    return numValue.toString();
}

function formatSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
}

function formatBytes(bytes) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// Standardized JSON parsing with consistent error handling
function parseJSONSafely(jsonString, context = 'JSON', showNotificationOnError = true) {
    try {
        if (!jsonString || jsonString.trim() === '') {
            return { success: true, data: {} };
        }
        const parsed = JSON.parse(jsonString);
        return { success: true, data: parsed };
    } catch (error) {
        const errorMessage = `Invalid ${context}: ${error.message}`;
        if (showNotificationOnError) {
            showNotification(errorMessage, 'error');
        }
        return { success: false, error: errorMessage, data: null };
    }
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
    
    // Initialize terminal
    initializeTerminal();
    
    // Initialize Bootstrap tooltips for taskbar buttons
    const tooltipTriggerList = document.querySelectorAll('[data-bs-toggle="tooltip"]');
    tooltipTriggerList.forEach(el => new bootstrap.Tooltip(el));
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
                downloadBackup(data.backup_data, data.filename || 'jdbx_backup.json');
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
            const text = await file.text();
            const parseResult = parseJSONSafely(text, 'backup file');
            if (!parseResult.success) {
                showOperationResult(false, 'Invalid backup file', { error: parseResult.error });
                return;
            }
            
            if (confirm('Are you sure you want to restore from this backup? This will overwrite existing data.')) {
                restoreFromBackup(parseResult.data);
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
        const response = await makeAuthenticatedRequest('/health');
        
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
            downloadBackup(data, `jdbx_export_${new Date().toISOString().split('T')[0]}.json`);
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
            const text = await file.text();
            const parseResult = parseJSONSafely(text, 'import file');
            if (!parseResult.success) {
                showOperationResult(false, 'Invalid JSON file', { error: parseResult.error });
                return;
            }
            
            if (confirm('Import this data? Existing collections may be overwritten.')) {
                importFromJSON(parseResult.data);
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
            "uuid": {
                "type": "string",
                "description": "Document UUID"
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
    const parseResult = parseJSONSafely(definitionText, 'schema JSON');
    if (!parseResult.success) {
        return;
    }
    const schema = parseResult.data;
    
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

// ===== WELCOME PANEL FUNCTIONALITY =====
async function loadWelcomePanel() {
    // Always load the welcome panel content
    
    try {
        // Check if _system_config collection exists first
        const systemConfigResponse = await apiRequest('/api/collections/_system_config/documents', 'GET', null, true);
        
        if (systemConfigResponse && systemConfigResponse.documents) {
            // Look for welcome message in system config
            const welcomeConfig = systemConfigResponse.documents.find(doc => 
                doc.type === 'welcome_message'
            );
            
            if (welcomeConfig) {
                // Display the welcome panel
                const contentElement = document.getElementById('welcomeContent');
                
                // Render markdown content
                if (welcomeConfig.message && contentElement) {
                    let content = welcomeConfig.message;
                    
                    // Clean up escaped content
                    content = content.replace(/\\n/g, '\n');
                    
                    // Check if marked.js is available for markdown rendering
                    if (typeof marked !== 'undefined') {
                        const htmlContent = marked.parse(content);
                        contentElement.innerHTML = htmlContent;
                    } else {
                        // Fallback: simple text with line breaks
                        contentElement.innerHTML = content.replace(/\n/g, '<br>');
                    }
                }
                return;
            }
        }
        
        // Fall back to checking _config collection for legacy support
        const configResponse = await apiRequest('/api/collections/_config/documents', 'GET', null, true);
        
        if (configResponse && configResponse.documents) {
            const welcomeConfig = configResponse.documents.find(doc => 
                doc.id === 'welcome_panel' || doc.type === 'ui_config'
            );
            
            if (welcomeConfig && welcomeConfig.enabled !== false) {
                // Display the welcome panel
                const contentElement = document.getElementById('welcomeContent');
                
                // Render markdown content
                if (welcomeConfig.content && contentElement) {
                    let content = welcomeConfig.content;
                    
                    // Clean up escaped content - remove excessive backslashes
                    content = content.replace(/\\+n/g, '\n');
                    content = content.replace(/\\+"/g, '"');
                    content = content.replace(/\\+\\/g, '/');
                    
                    // Check if marked.js is available for markdown rendering
                    if (typeof marked !== 'undefined') {
                        const htmlContent = marked.parse(content);
                        contentElement.innerHTML = htmlContent;
                    } else {
                        // Fallback: simple text with line breaks
                        contentElement.innerHTML = content.replace(/\n/g, '<br>');
                    }
                }
                
                // Content is now always visible in the fixed dashboard panel
            }
        }
    } catch (error) {
        // Silently fail if config collection doesn't exist
        console.log('Welcome panel config not found or error loading:', error);
    }
}

// Toggle and dismiss functions removed - welcome panel is now always visible

// ===== NAVIGATION FUNCTIONS =====
// Note: switchView function is defined earlier in the file (around line 202)

function logout() {
    // Clear auth token
    localStorage.removeItem('jdbx_auth_token');
    authToken = null;
    
    // Clear any intervals
    if (refreshInterval) {
        clearInterval(refreshInterval);
        refreshInterval = null;
    }
    
    // Redirect to login
    window.location.href = 'login.html';
}

// ===== JAVASCRIPT SCRIPTS INITIALIZATION =====
function initializeScripts() {
    console.log('Initializing Scripts view...');
    loadScripts();
}

// ===== TERMINAL OPERATIONS =====
let terminalContent = null;

function initializeTerminal() {
    terminalContent = document.getElementById('terminalContent');
}

function addTerminalLine(text, type = 'text') {
    if (!terminalContent) {
        terminalContent = document.getElementById('terminalContent');
    }
    
    const line = document.createElement('div');
    line.className = 'terminal-line';
    
    const prompt = document.createElement('span');
    prompt.className = 'terminal-prompt';
    prompt.textContent = 'jdbx>';
    
    const content = document.createElement('span');
    content.className = `terminal-${type}`;
    content.textContent = text;
    
    line.appendChild(prompt);
    line.appendChild(content);
    
    terminalContent.appendChild(line);
    terminalContent.scrollTop = terminalContent.scrollHeight;
}

function clearTerminal() {
    if (!terminalContent) {
        terminalContent = document.getElementById('terminalContent');
    }
    
    terminalContent.innerHTML = `
        <div class="terminal-line">
            <span class="terminal-prompt">jdbx&gt;</span>
            <span class="terminal-text">Terminal cleared.</span>
        </div>
    `;
}

async function runTerminalCommand(command) {
    switch(command) {
        case 'backup':
            addTerminalLine('Creating database backup...', 'info');
            try {
                const response = await apiRequest('/api/backup', {
                    method: 'POST',
                    body: JSON.stringify({})
                });
                if (response.success) {
                    addTerminalLine(`Backup created successfully: ${response.filename || 'backup.json'}`, 'success');
                } else {
                    addTerminalLine(`Backup failed: ${response.error || 'Unknown error'}`, 'error');
                }
            } catch (error) {
                addTerminalLine(`Error: ${error.message}`, 'error');
            }
            break;
            
        case 'compact':
            addTerminalLine('Database compaction is not currently available.', 'warning');
            addTerminalLine('This feature is under development.', 'info');
            // TODO: Implement database compaction endpoint in backend
            // Original code commented out until backend endpoint is available:
            /*
            try {
                const response = await apiRequest('/api/admin/compact', 'POST');
                if (response.success) {
                    addTerminalLine('Database compacted successfully', 'success');
                    if (response.stats) {
                        addTerminalLine(`Before: ${response.stats.before_size || 'N/A'} bytes`, 'text');
                        addTerminalLine(`After: ${response.stats.after_size || 'N/A'} bytes`, 'text');
                        addTerminalLine(`Saved: ${response.stats.saved || '0'} bytes`, 'text');
                    }
                } else {
                    addTerminalLine(`Compact failed: ${response.error || 'Unknown error'}`, 'error');
                }
            } catch (error) {
                addTerminalLine(`Error: ${error.message}`, 'error');
            }
            */
            break;
            
        case 'test':
            addTerminalLine('Testing database connection...', 'info');
            try {
                const response = await apiRequest('/api/health', {
                    method: 'GET'
                });
                if (response.status === 'healthy') {
                    addTerminalLine('Database connection: OK', 'success');
                    addTerminalLine(`Server version: ${response.version || 'Unknown'}`, 'text');
                    addTerminalLine(`Uptime: ${response.uptime || 'Unknown'}`, 'text');
                } else {
                    addTerminalLine('Database connection: FAILED', 'error');
                }
            } catch (error) {
                addTerminalLine(`Connection test failed: ${error.message}`, 'error');
            }
            break;
            
        case 'export':
            addTerminalLine('Exporting database...', 'info');
            try {
                const response = await apiRequest('/api/export', {
                    method: 'POST',
                    body: JSON.stringify({})
                });
                if (response) {
                    // Create download link
                    const blob = new Blob([JSON.stringify(response, null, 2)], {type: 'application/json'});
                    const url = window.URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    a.download = `jdbx-export-${new Date().toISOString().split('T')[0]}.json`;
                    document.body.appendChild(a);
                    a.click();
                    window.URL.revokeObjectURL(url);
                    document.body.removeChild(a);
                    
                    addTerminalLine('Database exported successfully', 'success');
                    addTerminalLine(`Collections exported: ${Object.keys(response.collections || {}).length}`, 'text');
                }
            } catch (error) {
                addTerminalLine(`Export failed: ${error.message}`, 'error');
            }
            break;
            
        case 'import':
            addTerminalLine('Import functionality requires file selection...', 'warning');
            // Show file input dialog
            const input = document.createElement('input');
            input.type = 'file';
            input.accept = '.json';
            input.onchange = async (e) => {
                const file = e.target.files[0];
                if (file) {
                    addTerminalLine(`Importing from ${file.name}...`, 'info');
                    try {
                        const text = await file.text();
                        const parseResult = parseJSONSafely(text, 'import file');
                        if (!parseResult.success) {
                            addTerminalLine('Import failed: Invalid JSON format', 'error');
                            return;
                        }
                        const response = await apiRequest('/api/import', {
                            method: 'POST',
                            body: JSON.stringify(parseResult.data)
                        });
                        if (response.success) {
                            addTerminalLine('Import completed successfully', 'success');
                            if (response.stats) {
                                addTerminalLine(`Collections imported: ${response.stats.collections || 0}`, 'text');
                                addTerminalLine(`Documents imported: ${response.stats.documents || 0}`, 'text');
                            }
                        } else {
                            addTerminalLine(`Import failed: ${response.error || 'Unknown error'}`, 'error');
                        }
                    } catch (error) {
                        addTerminalLine(`Import error: ${error.message}`, 'error');
                    }
                }
            };
            input.click();
            break;
            
        case 'cache':
            addTerminalLine('Clearing cache...', 'info');
            try {
                const response = await apiRequest('/api/cache/clear', {
                    method: 'POST',
                    body: JSON.stringify({})
                });
                if (response.success) {
                    addTerminalLine('Cache cleared successfully', 'success');
                    if (response.stats) {
                        addTerminalLine(`Items cleared: ${response.stats.items_cleared || 0}`, 'text');
                        addTerminalLine(`Memory freed: ${response.stats.memory_freed || '0'} bytes`, 'text');
                    }
                } else {
                    addTerminalLine(`Cache clear failed: ${response.error || 'Unknown error'}`, 'error');
                }
            } catch (error) {
                addTerminalLine(`Error: ${error.message}`, 'error');
            }
            break;
            
        default:
            addTerminalLine(`Unknown command: ${command}`, 'error');
    }
}


// ===== EDIT/SAVE BUTTON FUNCTIONALITY =====
let isEditMode = false;

function toggleEditMode() {
    isEditMode = !isEditMode;
    const editor = document.getElementById('documentEditor');
    const editBtn = document.getElementById('editBtn');
    const saveBtn = document.getElementById('saveBtn');
    
    if (isEditMode) {
        // Switch to edit mode
        editor.removeAttribute('readonly');
        editor.classList.add('editing');
        editBtn.style.display = 'none';
        saveBtn.style.display = 'inline-block';
        editor.focus();
    } else {
        // Switch to view mode
        editor.setAttribute('readonly', true);
        editor.classList.remove('editing');
        editBtn.style.display = 'inline-block';
        saveBtn.style.display = 'none';
    }
}

async function saveDocument() {
    const editor = document.getElementById('documentEditor');
    const newContent = editor.value.trim();
    
    // Skip if content is empty
    if (!newContent) {
        showNotification('Document content cannot be empty', 'error');
        return;
    }
    
    // Parse and validate the JSON
    const parseResult = parseJSONSafely(newContent, 'document JSON');
    if (!parseResult.success) {
        return;
    }
    
    const parsedDoc = parseResult.data;
    
    // Ensure the document has the correct uuid (prefer uuid over _id)
    if (!parsedDoc.uuid && !parsedDoc._id) {
        parsedDoc.uuid = currentDocument;
    }
    
    try {
        console.log(`Saving document - Collection: ${currentCollection}, Document ID: ${currentDocument}`);
        console.log('Document content:', parsedDoc);
        
        // Save the document using PUT method with document ID in URL
        const requestBody = JSON.stringify(parsedDoc);
        console.log('Sending request body:', requestBody);
        
        const response = await apiRequest(`/api/collections/${currentCollection}/documents/${currentDocument}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: requestBody
        });
        
        console.log('Save response:', response);
        
        if (response.uuid || response._id || response.success || response.id) {
            showNotification('Document saved successfully', 'success');
            // Update the local document
            documents[currentDocumentIndex] = parsedDoc;
            originalDocumentContent = newContent;
            isDocumentModified = false;
            // Hide save button
            const saveBtn = document.getElementById('saveBtn');
            saveBtn.style.display = 'none';
        } else {
            console.error('Save failed with response:', response);
            showNotification(response.error || 'Failed to save document', 'error');
        }
    } catch (error) {
        console.error('Save error:', error);
        showNotification(`Failed to save document: ${error.message}`, 'error');
    }
}

// Update the selectDocument function to show edit button
function updateDocumentButtons() {
    const editBtn = document.getElementById('editBtn');
    const saveBtn = document.getElementById('saveBtn');
    const deleteBtn = document.getElementById('deleteBtn');
    const transformPreviewBtn = document.getElementById('transformPreviewBtn');
    
    if (currentDocument) {
        // Always hide edit button initially
        editBtn.style.display = 'none';
        deleteBtn.disabled = false;
        saveBtn.style.display = 'none';
        isEditMode = false;
        
        // Show transformation preview button if transformers are available
        if (currentTransformers.length > 0) {
            transformPreviewBtn.style.display = 'inline-block';
            transformPreviewBtn.title = `${currentTransformers.length} transformer(s) available for ${currentCollection}`;
        } else {
            transformPreviewBtn.style.display = 'none';
        }
    } else {
        editBtn.style.display = 'none';
        deleteBtn.disabled = true;
        saveBtn.style.display = 'none';
        transformPreviewBtn.style.display = 'none';
    }
}

// Show save button when content is modified
function handleDocumentEdit() {
    const editor = document.getElementById('documentEditor');
    const saveBtn = document.getElementById('saveBtn');
    
    if (editor && originalDocumentContent !== editor.value) {
        // Content has changed, show green Save button
        saveBtn.style.display = 'inline-block';
        isDocumentModified = true;
    }
}

// Create new document function
function createNewDocument() {
    if (!currentCollection) {
        showNotification('Please select a collection first', 'warning');
        return;
    }
    
    // Generate a new document ID
    const newId = 'doc-' + Date.now() + '-' + Math.random().toString(36).substr(2, 5);
    
    // Create a template document
    const templateDocument = {
        uuid: newId,
        name: "New Document",
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
        // Add some example fields
        content: "Enter your content here",
        status: "draft"
    };
    
    // Add to documents array
    documents.unshift(templateDocument); // Add at beginning
    
    // Update UI
    renderDocuments();
    document.getElementById('documentCount').textContent = documents.length;
    
    // Select the new document (index 0 since we added it at the beginning)
    selectDocument(0);
    
    // Show save button immediately since this is a new document
    const saveBtn = document.getElementById('saveBtn');
    saveBtn.style.display = 'inline-block';
    isDocumentModified = true;
    
    showNotification('New document created. Edit and save to persist.', 'info');
}

// ========================
// JavaScript Scripts Management
// ========================

// Current script data

