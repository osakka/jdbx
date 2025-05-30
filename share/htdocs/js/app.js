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
let memoryUsageChart = null;
let databaseSizeChart = null;
let storageChart = null;
let connectionsChart = null;
let responseTimesChart = null;

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

// Session validation check
let sessionCheckInterval = null;

async function validateSession() {
    if (!authToken) {
        console.log('No auth token, redirecting to login');
        window.location.href = '/login.html';
        return false;
    }
    
    try {
        // Make a lightweight request to check if session is valid
        // Using /api/collections endpoint which requires auth but is lightweight
        const response = await fetch(`${API_BASE_URL}/api/collections`, {
            method: 'HEAD',  // Use HEAD to minimize data transfer
            headers: {
                'Authorization': `Bearer ${authToken}`
            }
        });
        
        if (response.status === 401) {
            console.log('Session invalid (401), redirecting to login');
            // Clear tokens
            localStorage.removeItem('jsondb_auth_token');
            localStorage.removeItem('jsondb_refresh_token');
            // Clear session check interval
            if (sessionCheckInterval) {
                clearInterval(sessionCheckInterval);
            }
            // Redirect to login
            window.location.href = '/login.html';
            return false;
        }
        
        // If HEAD method not allowed, it's still a valid session (just not optimal)
        if (response.status === 405) {
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
    // Initial check after 5 seconds
    setTimeout(validateSession, 5000);
    
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
document.addEventListener('DOMContentLoaded', function() {
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
                                    'page-metrics', 'page-rbac', 'page-operations', 'page-api');
    
    // Add page-specific class
    document.body.classList.add(`page-${view}`);
    
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
                // For RBAC, admin, and metrics endpoints, don't kick out, just throw error
                if (endpoint.includes('/rbac/') || endpoint.includes('/admin/') || endpoint.includes('/metrics/')) {
                    const error = await response.json().catch(() => ({ error: 'Unauthorized' }));
                    throw new Error(error.error || 'Unauthorized');
                }
                // For other endpoints, kick out
                localStorage.removeItem('jsondb_auth_token');
                localStorage.removeItem('jsondb_refresh_token');
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
        }
    }
    
    // Initialize connections chart
    if (!connectionsChart) {
        const ctx = document.getElementById('connectionsChart');
        if (ctx) {
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
        }
    }
    
    // Initialize response times chart
    if (!responseTimesChart) {
        const ctx = document.getElementById('responseTimesChart');
        if (ctx) {
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
        const response = await apiRequest('/api/collections');
        let collectionsData = Array.isArray(response) ? response : (response.collections || []);
        
        // Ensure we don't have mixed formats that could cause issues
        const collections = collectionsData.map(item => {
            if (typeof item === 'string') {
                return { name: item, documentCount: 0, isSystem: item.startsWith('_') };
            }
            return item;
        });
        
        if (previousData.totalCollections !== collections.length) {
            document.getElementById('statTotalCollections').textContent = collections.length;
            previousData.totalCollections = collections.length;
        }
        
        let totalDocuments = 0;
        let totalSize = 0;
        
        for (const collectionInfo of collections) {
            // Handle both old format (string) and new format (object)
            const collectionName = typeof collectionInfo === 'string' ? collectionInfo : (collectionInfo?.name || 'unknown');
            
            try {
                const docCountFromInfo = typeof collectionInfo === 'object' ? collectionInfo.documentCount : null;
                
                // If we already have the count from the API, use it for efficiency
                let docCount = docCountFromInfo;
                let size = 0;
                
                if (docCount === null || docCount > 0) {
                    // Only fetch documents if we don't have count or if there are documents
                    const docsResponse = await apiRequest(`/api/collections/${collectionName}/documents`);
                    const documents = Array.isArray(docsResponse) ? docsResponse : (docsResponse.documents || []);
                    docCount = docCount !== null ? docCount : documents.length;
                    size = JSON.stringify(documents).length;
                }
                
                totalDocuments += docCount;
                totalSize += size;
                
                const collectionKey = `${collectionName}_${docCount}_${size}`;
                previousData.collectionsData[collectionName] = collectionKey;
                
            } catch (error) {
                console.error(`Error loading collection ${collectionName}:`, error);
            }
        }
        
        if (previousData.totalDocuments !== totalDocuments) {
            document.getElementById('totalDocuments').textContent = formatNumber(totalDocuments);
            previousData.totalDocuments = totalDocuments;
        }
        
        // Ensure we show a minimum size for the database (system collections exist)
        const displaySize = totalSize > 0 ? totalSize : 20480; // 20KB minimum for system collections
        if (previousData.databaseSize !== displaySize) {
            document.getElementById('statDatabaseSize').textContent = formatSize(displaySize);
            previousData.databaseSize = displaySize;
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
    collectionsChart.data.labels = labels;
    collectionsChart.data.datasets[0].data = data;
    collectionsChart.update();
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
    
    connectionsChart.data.labels = labels;
    connectionsChart.data.datasets[0].data = data;
    connectionsChart.update();
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
    
    responseTimesChart.data.labels = labels;
    responseTimesChart.data.datasets[0].data = avgData; // Average response time
    responseTimesChart.update();
}

async function loadDashboardMetrics() {
    try {
        // Fetch metrics data from _system_metrics collection
        const metricsData = await apiRequest('/api/collections/_system_metrics').catch(err => {
            console.error('Failed to fetch metrics data:', err);
            return { documents: [] };
        });
        
        console.log('Dashboard metrics data:', metricsData);
        
        // Extract metrics documents by type
        const metricsDocuments = metricsData.documents || [];
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
        let rawCollections = [];
        if (Array.isArray(collectionsResponse)) {
            rawCollections = collectionsResponse;
        } else if (collectionsResponse.collections) {
            rawCollections = collectionsResponse.collections;
        }
        
        // Normalize collections to always be objects with name, documentCount, isSystem
        collections = rawCollections.map(item => {
            if (typeof item === 'string') {
                return { name: item, documentCount: 0, isSystem: item.startsWith('_') };
            }
            // Ensure the object has all required fields
            return {
                name: item.name || (typeof item === 'string' ? item : 'unknown'),
                documentCount: item.documentCount || 0,
                isSystem: item.isSystem !== undefined ? item.isSystem : ((item.name || (typeof item === 'string' ? item : '')).startsWith('_'))
            };
        });
        
        // Store schemas for reference
        if (schemasResponse && schemasResponse.schemas) {
            schemas = schemasResponse.schemas;
        } else if (Array.isArray(schemasResponse)) {
            schemas = schemasResponse;
        } else {
            schemas = [];
        }
        
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
                    return `
                        <div class="collection-item ${currentCollection === name ? 'active' : ''}" 
                             data-collection="${name}" 
                             onclick="selectCollection('${name}')">
                            <i class="bi bi-gear-fill me-2" style="font-size: 0.875rem;"></i>
                            <span class="collection-name">${name}</span>
                            ${hasSchema ? '<i class="bi bi-shield-check text-success ms-1" title="Schema defined"></i>' : ''}
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
        const docName = doc.name || docId; // Use name if available, otherwise fallback to ID
        const docSize = JSON.stringify(doc).length;
        const sizeStr = docSize < 1024 ? `${docSize} B` : `${(docSize / 1024).toFixed(1)} KB`;
        const updatedAt = doc.updated_at || doc.created_at || '';
        
        // Format the display based on whether we have a custom name
        const hasCustomName = doc.name && doc.name !== docId;
        
        return `
            <div class="document-item ${currentDocument === (doc._id || doc.id) ? 'active' : ''}" 
                 onclick="selectDocument(${index})">
                <div class="d-flex align-items-center w-100">
                    <i class="bi bi-file-text me-2" style="font-size: 0.875rem;"></i>
                    <div class="flex-grow-1">
                        ${hasCustomName ? `
                            <div class="document-name fw-bold">${docName}</div>
                            <div class="document-id text-muted small">${docId}${updatedAt ? ` • ${new Date(updatedAt).toLocaleDateString()}` : ''}</div>
                        ` : `
                            <span class="document-name">${docId}</span>
                        `}
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
    updateDocumentButtons();
}

// ===== QUERY BUILDER FUNCTIONALITY =====
let queryBuilderVisible = false;

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
            try {
                const customJSON = document.getElementById('customJSON').value;
                if (customJSON) {
                    query = JSON.parse(customJSON);
                }
            } catch (e) {
                query = { error: 'Invalid JSON' };
            }
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
            try {
                const customJSON = document.getElementById('customJSON').value;
                if (customJSON) {
                    query = JSON.parse(customJSON);
                }
            } catch (e) {
                showNotification('Invalid JSON query', 'error');
                return;
            }
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

// Old editDocument function removed - now using toggleEditMode/saveDocument

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
    console.log('initializeMetrics called');
    
    // Initialize charts if not already done
    if (!operationsChart) {
        const ctx = document.getElementById('operationsChart');
        if (ctx) {
            operationsChart = new Chart(ctx.getContext('2d'), {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Read Ops/sec',
                        data: [],
                        borderColor: '#61affe',
                        backgroundColor: 'rgba(97, 175, 254, 0.1)',
                        tension: 0.4
                    }, {
                        label: 'Write Ops/sec',
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
    
    // Initialize memory usage chart
    if (!memoryUsageChart) {
        const ctx = document.getElementById('memoryUsageChart');
        if (ctx) {
            memoryUsageChart = new Chart(ctx.getContext('2d'), {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Process Memory (MB)',
                        data: [],
                        borderColor: '#f93e3e',
                        backgroundColor: 'rgba(249, 62, 62, 0.1)',
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
    
    // Initialize database size chart
    if (!databaseSizeChart) {
        const ctx = document.getElementById('processMemoryChart');
        if (ctx) {
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
    
    // Force immediate load with a small delay to ensure DOM is ready
    console.log('Scheduling loadMetrics...');
    setTimeout(() => {
        console.log('Calling loadMetrics from initializeMetrics');
        loadMetrics();
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
        
        // Fetch metrics data from _system_metrics collection
        console.log('Fetching metrics from _system_metrics collection...');
        const metricsData = await apiRequest('/api/collections/_system_metrics').catch(err => {
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
            document.getElementById('cacheSize').textContent = formatNumber(size);
            document.getElementById('cacheMemory').textContent = `${memoryMB} MB`;
        } else {
            document.getElementById('cacheSize').textContent = '0';
            document.getElementById('cacheMemory').textContent = '0 MB';
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
        
        document.getElementById('cacheStats').textContent = `${formatNumber(cacheHits)} hits / ${formatNumber(cacheMisses)} misses`;
        
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
            operationsChart.data.labels = labels;
            operationsChart.data.datasets[0].data = readData;
            operationsChart.data.datasets[1].data = writeData;
            operationsChart.update();
        }
        
        // Calculate operation types for pie chart from latest data
        const currentReadOps = readOps;
        const currentWriteOps = writeOps;
        const currentDeleteOps = 0; // TODO: Add delete operations tracking
        const currentQueryOps = 0; // TODO: Add query operations tracking
        
        if (operationTypesChart) {
            const total = currentReadOps + currentWriteOps + currentDeleteOps + currentQueryOps;
            operationTypesChart.data.datasets[0].data = [
                currentReadOps,
                currentWriteOps,
                currentDeleteOps,
                currentQueryOps
            ];
            
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
        }
        
        // Process cache hit rate data from time-series
        const cacheHitRateData = [];
        if (cacheDoc && cacheDoc.data && cacheDoc.data.length > 0) {
            // Calculate hit rate for each time point
            cacheDoc.data.forEach(point => {
                const hits = point.hits || 0;
                const misses = point.misses || 0;
                const total = hits + misses;
                const hitRate = total > 0 ? (hits / total * 100) : 0;
                cacheHitRateData.push(hitRate);
            });
            
            // Ensure cache data matches operations data length
            while (cacheHitRateData.length < labels.length) {
                cacheHitRateData.push(cacheHitRateData[cacheHitRateData.length - 1] || 0);
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
        
        // Update memory usage chart
        if (memoryUsageChart) {
            memoryUsageChart.data.labels = labels;
            memoryUsageChart.data.datasets[0].data = processMemoryData;
            memoryUsageChart.update();
        }
        
        // Update database size chart
        if (databaseSizeChart) {
            // Create database size data from document counts over time
            const dbSizeData = [];
            
            // Calculate database size for each time point based on operations count
            // Estimate: each operation adds approximately 0.5KB to database
            if (operationsDoc && operationsDoc.data && operationsDoc.data.length > 0) {
                operationsDoc.data.forEach(point => {
                    // Estimate database size in MB based on total operations
                    const estimatedSizeMB = (point.total || 0) * 0.5 / 1024;
                    dbSizeData.push(Math.round(estimatedSizeMB * 100) / 100);
                });
            } else {
                // If no operations data, use the current estimated size
                const currentSizeMB = window.estimatedDatabaseSizeKB ? window.estimatedDatabaseSizeKB / 1024 : 0.02;
                for (let i = 0; i < labels.length; i++) {
                    dbSizeData.push(currentSizeMB);
                }
            }
            
            databaseSizeChart.data.labels = labels;
            databaseSizeChart.data.datasets[0].data = dbSizeData;
            databaseSizeChart.update();
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
                        if (col && typeof col === 'object' && col.name && col.documentCount !== undefined) {
                            // Estimate size based on document count (rough estimate: 1KB per doc)
                            const estimatedSizeKB = col.documentCount * 1; // 1KB per document estimate
                            totalSizeKB += estimatedSizeKB;
                            
                            console.log(`Collection ${col.name}: ${col.documentCount} docs, ${estimatedSizeKB}KB`);
                            
                            if (estimatedSizeKB > 0) {
                                collectionSizes.push(estimatedSizeKB);
                                collectionLabels.push(col.name);
                            }
                        }
                    });
                }
                
                // Update database size metric
                // Use the estimated size we calculated earlier or the local calculation
                const finalSizeKB = window.estimatedDatabaseSizeKB || totalSizeKB;
                let displaySize;
                if (finalSizeKB > 0) {
                    const databaseSizeMB = (finalSizeKB / 1024).toFixed(2);
                    displaySize = `${databaseSizeMB} MB`;
                } else {
                    // Show actual database file size if available
                    // The binary database file is approximately 25KB based on file system
                    displaySize = "0.02 MB"; // ~20KB for system collections
                }
                
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
                    storageChart.data.datasets[0].data = sizeData.map(d => d.size);
                    storageChart.update();
                } else {
                    console.log('No data for storage chart');
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
        // Show N/A on error
        document.getElementById('totalOps').textContent = 'N/A';
        document.getElementById('readOps').textContent = 'N/A';
        document.getElementById('writeOps').textContent = 'N/A';
        document.getElementById('avgResponseTime').textContent = 'N/A';
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
    console.log('Auth token exists:', !!localStorage.getItem('jsondb_auth_token'));
    
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
                    ${perms.create ? '<i class="bi bi-check-circle text-success"></i>' : '<i class="bi bi-x-circle text-muted"></i>'}
                </td>
                <td class="text-center">
                    ${perms.read ? '<i class="bi bi-check-circle text-success"></i>' : '<i class="bi bi-x-circle text-muted"></i>'}
                </td>
                <td class="text-center">
                    ${perms.write ? '<i class="bi bi-check-circle text-success"></i>' : '<i class="bi bi-x-circle text-muted"></i>'}
                </td>
                <td class="text-center">
                    ${perms.delete ? '<i class="bi bi-check-circle text-success"></i>' : '<i class="bi bi-x-circle text-muted"></i>'}
                </td>
                <td class="text-center">
                    ${perms.admin ? '<i class="bi bi-check-circle text-success"></i>' : '<i class="bi bi-x-circle text-muted"></i>'}
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
        const isExpired = expiresAt < new Date();
        const status = session.active === false ? 'Terminated' : (isExpired ? 'Expired' : 'Active');
        const statusClass = session.active === false ? 'bg-secondary' : (isExpired ? 'bg-danger' : 'bg-success');
        
        // Format IP address
        const ipAddress = session.ip_address || 'Unknown';
        
        // Format user agent - truncate if too long
        let userAgent = session.user_agent || 'Unknown';
        if (userAgent.length > 50) {
            userAgent = userAgent.substring(0, 47) + '...';
        }
        
        return `
            <tr>
                <td class="font-monospace small">${session._id || session.id || 'N/A'}</td>
                <td>${session.username || session.user || 'Unknown'}</td>
                <td class="font-monospace">${ipAddress}</td>
                <td class="small" title="${session.user_agent || 'Unknown'}">${userAgent}</td>
                <td>${formatDate(createdAt)}</td>
                <td>${formatDate(lastActivity)}</td>
                <td>${formatDate(expiresAt)}</td>
                <td>
                    <span class="badge ${statusClass}">
                        ${status}
                    </span>
                </td>
                <td>
                    <button class="btn btn-sm btn-outline-danger" onclick="revokeSession('${session._id || session.id}')">
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
            apiRequest(`/api/collections/_sessions/documents/${session._id}`, {
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

function formatBytes(bytes) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
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

// ===== WELCOME PANEL FUNCTIONALITY =====
async function loadWelcomePanel() {
    // Always load the welcome panel content
    
    try {
        // Check if _config collection exists
        const configResponse = await apiRequest('/api/collections/_config/documents', 'GET', null, true);
        
        if (configResponse && configResponse.documents) {
            // Look for welcome panel configuration
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
function switchView(viewName) {
    // Clear any existing polling
    if (refreshInterval) {
        clearInterval(refreshInterval);
        refreshInterval = null;
    }
    
    // Hide all views
    document.querySelectorAll('.view-container').forEach(view => {
        view.classList.remove('active');
    });
    
    // Update navigation
    document.querySelectorAll('.nav-link').forEach(link => {
        link.classList.remove('active');
    });
    
    // Show selected view
    const targetView = document.getElementById(`${viewName}-view`);
    if (targetView) {
        targetView.classList.add('active');
    }
    
    // Activate corresponding nav link
    const navLink = document.querySelector(`.nav-link[href="#${viewName}"]`);
    if (navLink) {
        navLink.classList.add('active');
    }
    
    // Store current view
    currentView = viewName;
    
    // Initialize view and set up polling
    switch (viewName) {
        case 'dashboard':
            initializeDashboard();
            refreshInterval = setInterval(() => loadDashboard(true), POLLING_INTERVALS.dashboard);
            break;
        case 'browser':
            initializeBrowser();
            refreshInterval = setInterval(() => loadBrowserCollections(), POLLING_INTERVALS.browser);
            break;
        case 'metrics':
            initializeMetrics();
            refreshInterval = setInterval(() => loadMetrics(true), POLLING_INTERVALS.metrics);
            break;
        case 'rbac':
            initializeRBAC();
            refreshInterval = setInterval(() => loadUsersAndRoles(), POLLING_INTERVALS.rbac);
            break;
        case 'operations':
            initializeOperations();
            initializeTerminal();
            // Operations page doesn't need polling
            break;
        case 'api':
            initializeAPI();
            break;
        default:
            console.warn('Unknown view:', viewName);
    }
}

function logout() {
    // Clear auth token
    localStorage.removeItem('jsondb_auth_token');
    authToken = null;
    
    // Clear any intervals
    if (refreshInterval) {
        clearInterval(refreshInterval);
        refreshInterval = null;
    }
    
    // Redirect to login
    window.location.href = 'login.html';
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
    prompt.textContent = 'jsondb>';
    
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
            <span class="terminal-prompt">jsondb&gt;</span>
            <span class="terminal-text">Terminal cleared.</span>
        </div>
    `;
}

async function runTerminalCommand(command) {
    switch(command) {
        case 'backup':
            addTerminalLine('Creating database backup...', 'info');
            try {
                const response = await apiRequest('/api/admin/backup', 'POST');
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
            addTerminalLine('Compacting database...', 'info');
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
            break;
            
        case 'test':
            addTerminalLine('Testing database connection...', 'info');
            try {
                const response = await apiRequest('/api/health', 'GET');
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
                const response = await apiRequest('/api/export', 'GET');
                if (response) {
                    // Create download link
                    const blob = new Blob([JSON.stringify(response, null, 2)], {type: 'application/json'});
                    const url = window.URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    a.download = `jsondb-export-${new Date().toISOString().split('T')[0]}.json`;
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
                        const data = JSON.parse(text);
                        const response = await apiRequest('/api/import', 'POST', data);
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
                const response = await apiRequest('/api/cache/clear', 'POST');
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
    const newContent = editor.value;
    
    try {
        // Parse the JSON to validate it
        const parsedDoc = JSON.parse(newContent);
        
        // Ensure the document has the correct _id
        if (!parsedDoc._id) {
            parsedDoc._id = currentDocument;
        }
        
        // Save the document
        const response = await apiRequest(`/api/collections/${currentCollection}/documents`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(parsedDoc)
        });
        
        if (response.success || response.id) {
            showNotification('Document saved successfully', 'success');
            // Update the local document
            documents[currentDocumentIndex] = parsedDoc;
            originalDocumentContent = newContent;
            isDocumentModified = false;
            // Hide save button
            const saveBtn = document.getElementById('saveBtn');
            saveBtn.style.display = 'none';
        } else {
            showNotification(response.error || 'Failed to save document', 'error');
        }
    } catch (error) {
        showNotification(`Invalid JSON: ${error.message}`, 'error');
    }
}

// Update the selectDocument function to show edit button
function updateDocumentButtons() {
    const editBtn = document.getElementById('editBtn');
    const saveBtn = document.getElementById('saveBtn');
    const deleteBtn = document.getElementById('deleteBtn');
    
    if (currentDocument) {
        // Always hide edit button initially
        editBtn.style.display = 'none';
        deleteBtn.disabled = false;
        saveBtn.style.display = 'none';
        isEditMode = false;
    } else {
        editBtn.style.display = 'none';
        deleteBtn.disabled = true;
        saveBtn.style.display = 'none';
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
