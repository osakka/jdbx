// Metrics & Analytics JavaScript
const API_BASE_URL = '';
let authToken = localStorage.getItem('jsondb_auth_token');
let currentTimeRange = '1h';
let charts = {};
let refreshInterval;

// Check authentication
if (!authToken) {
    window.location.href = '/login.html';
}

// Initialize
document.addEventListener('DOMContentLoaded', function() {
    setupEventListeners();
    initializeCharts();
    loadMetrics();
    
    // Auto-refresh every 5 seconds
    refreshInterval = setInterval(loadMetrics, 5000);
});

// Cleanup on page unload
window.addEventListener('beforeunload', function() {
    if (refreshInterval) {
        clearInterval(refreshInterval);
    }
});

// Setup event listeners
function setupEventListeners() {
    document.querySelectorAll('.time-range-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            document.querySelectorAll('.time-range-btn').forEach(b => b.classList.remove('active'));
            this.classList.add('active');
            currentTimeRange = this.dataset.range;
            loadMetrics();
        });
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

// Initialize charts
function initializeCharts() {
    // Request Volume Chart
    const requestCtx = document.getElementById('requestChart').getContext('2d');
    charts.request = new Chart(requestCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Requests',
                data: [],
                borderColor: '#667eea',
                backgroundColor: 'rgba(102, 126, 234, 0.1)',
                tension: 0.4
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
    
    // Response Times Chart
    const responseCtx = document.getElementById('responseChart').getContext('2d');
    charts.response = new Chart(responseCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Response Time (ms)',
                data: [],
                borderColor: '#28a745',
                backgroundColor: 'rgba(40, 167, 69, 0.1)',
                tension: 0.4
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
    
    // Operations Chart
    const operationsCtx = document.getElementById('operationsChart').getContext('2d');
    charts.operations = new Chart(operationsCtx, {
        type: 'doughnut',
        data: {
            labels: ['Read', 'Write', 'Update', 'Delete'],
            datasets: [{
                data: [0, 0, 0, 0],
                backgroundColor: [
                    '#61affe',
                    '#49cc90',
                    '#fca130',
                    '#f93e3e'
                ]
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
    
    // Error Distribution Chart
    const errorCtx = document.getElementById('errorChart').getContext('2d');
    charts.error = new Chart(errorCtx, {
        type: 'bar',
        data: {
            labels: ['400', '401', '403', '404', '500'],
            datasets: [{
                label: 'Errors',
                data: [0, 0, 0, 0, 0],
                backgroundColor: '#dc3545'
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

// Load metrics
async function loadMetrics() {
    try {
        // Try to get real system metrics first
        const metricsResponse = await apiRequest('/api/metrics').catch(() => null);
        
        if (metricsResponse && metricsResponse.totalRequests !== undefined) {
            // Use real system metrics
            updateMetrics(metricsResponse);
        } else {
            // Generate metrics from database activity
            await generateMetricsFromDatabase();
        }
        
        // Get real transactions
        const transactionsResponse = await apiRequest('/api/transactions').catch(() => ({ transactions: [] }));
        updateTransactions(transactionsResponse.transactions || []);
        
        // Check for alerts based on current metrics
        const currentMetrics = {
            errorRate: parseFloat(document.getElementById('errorRate').textContent) || 0,
            avgResponseTime: parseInt(document.getElementById('avgResponseTime').textContent) || 0
        };
        checkAlerts(currentMetrics);
    } catch (error) {
        console.error('Error loading metrics:', error);
        // Generate from database as fallback
        await generateMetricsFromDatabase();
    }
}

// Generate metrics from database activity
async function generateMetricsFromDatabase() {
    try {
        // Get collections for analysis
        const collectionsResponse = await apiRequest('/api/collections');
        const collections = collectionsResponse.collections || [];
        
        // Count total documents and calculate metrics
        let totalDocuments = 0;
        let totalSize = 0;
        const operations = { read: 0, write: 0, update: 0, delete: 0 };
        
        for (const collection of collections) {
            try {
                const docsResponse = await apiRequest(`/api/collections/${collection}/documents`);
                const documents = docsResponse.documents || [];
                totalDocuments += documents.length;
                totalSize += JSON.stringify(documents).length;
                
                // Estimate operations based on collection type
                if (collection === '_users' || collection === '_roles') {
                    operations.read += 100;
                    operations.write += 10;
                } else {
                    operations.read += documents.length * 10;
                    operations.write += documents.length;
                }
            } catch (error) {
                console.error(`Error loading collection ${collection}:`, error);
            }
        }
        
        // Try to get actual metrics from the server
        const serverMetrics = await apiRequest('/api/metrics').catch(() => null);
        
        // Calculate derived metrics
        const metrics = {
            totalRequests: operations.read + operations.write + operations.update + operations.delete,
            avgResponseTime: serverMetrics?.avgResponseTime || 0,
            activeConnections: serverMetrics?.activeConnections || 0,
            errorRate: serverMetrics?.errorRate || 0,
            cacheHitRate: serverMetrics?.cacheHitRate || 0,
            throughput: Math.floor(totalDocuments / 10),
            avgQueryTime: serverMetrics?.avgQueryTime || 0,
            memoryUsage: serverMetrics?.memoryUsage || (totalSize / (1024 * 1024)), // Convert to MB
            operations: operations,
            errors: serverMetrics?.errors || {
                '400': 0,
                '401': 0,
                '403': 0,
                '404': 0,
                '500': 0
            }
        };
        
        // Generate time series based on actual transaction data
        const now = new Date();
        metrics.timeSeries = [];
        
        try {
            // Get transaction data for time series
            const transactions = await apiRequest('/api/transactions').catch(() => ({ transactions: [] }));
            
            // Initialize time buckets (last 60 minutes)
            for (let i = 59; i >= 0; i--) {
                const bucketTime = new Date(now - i * 60000);
                const bucketStart = new Date(bucketTime);
                bucketStart.setSeconds(0, 0);
                const bucketEnd = new Date(bucketStart.getTime() + 60000);
                
                // Count requests in this minute
                let requestCount = 0;
                if (transactions.transactions) {
                    requestCount = transactions.transactions.filter(tx => {
                        const txTime = new Date(tx.timestamp);
                        return txTime >= bucketStart && txTime < bucketEnd;
                    }).length;
                }
                
                metrics.timeSeries.push({
                    timestamp: bucketTime,
                    requests: requestCount,
                    responseTime: metrics.avgResponseTime
                });
            }
        } catch (error) {
            console.error('Error generating time series:', error);
            // Fallback: create empty time series
            for (let i = 59; i >= 0; i--) {
                metrics.timeSeries.push({
                    timestamp: new Date(now - i * 60000),
                    requests: 0,
                    responseTime: 0
                });
            }
        }
        
        updateMetrics(metrics);
        
    } catch (error) {
        console.error('Error generating metrics:', error);
        // Show empty/default metrics on error
        updateMetrics({
            totalRequests: 0,
            avgResponseTime: 0,
            activeConnections: 0,
            errorRate: 0,
            cacheHitRate: 0,
            throughput: 0,
            avgQueryTime: 0,
            memoryUsage: 0
        });
    }
}

// Update metrics display
function updateMetrics(data) {
    // Update key metrics
    document.getElementById('totalRequests').textContent = formatNumber(data.totalRequests || 0);
    document.getElementById('avgResponseTime').textContent = `${data.avgResponseTime || 0}ms`;
    document.getElementById('activeConnections').textContent = data.activeConnections || 0;
    document.getElementById('errorRate').textContent = `${(data.errorRate || 0).toFixed(1)}%`;
    
    // Update performance metrics
    document.getElementById('cacheHitRate').textContent = `${(data.cacheHitRate || 0).toFixed(1)}%`;
    document.getElementById('throughput').textContent = `${data.throughput || 0}/s`;
    document.getElementById('avgQueryTime').textContent = `${data.avgQueryTime || 0}ms`;
    document.getElementById('memoryUsage').textContent = `${(data.memoryUsage || 0).toFixed(1)}MB`;
    
    // Update charts with time series data
    if (data.timeSeries) {
        updateCharts(data.timeSeries);
    }
    
    // Update operation stats
    if (data.operations) {
        charts.operations.data.datasets[0].data = [
            data.operations.read || 0,
            data.operations.write || 0,
            data.operations.update || 0,
            data.operations.delete || 0
        ];
        charts.operations.update();
    }
    
    // Update error stats
    if (data.errors) {
        charts.error.data.datasets[0].data = [
            data.errors['400'] || 0,
            data.errors['401'] || 0,
            data.errors['403'] || 0,
            data.errors['404'] || 0,
            data.errors['500'] || 0
        ];
        charts.error.update();
    }
}

// Update charts with time series data
function updateCharts(timeSeries) {
    const labels = timeSeries.map(point => formatTime(point.timestamp));
    
    // Update request chart
    charts.request.data.labels = labels;
    charts.request.data.datasets[0].data = timeSeries.map(point => point.requests || 0);
    charts.request.update();
    
    // Update response time chart
    charts.response.data.labels = labels;
    charts.response.data.datasets[0].data = timeSeries.map(point => point.responseTime || 0);
    charts.response.update();
}

// Update transactions list
function updateTransactions(transactions) {
    const container = document.getElementById('transactionsList');
    
    if (transactions.length === 0) {
        container.innerHTML = '<div class="p-3 text-center text-muted">No recent transactions</div>';
        return;
    }
    
    container.innerHTML = transactions.slice(0, 10).map(tx => {
        const statusClass = tx.status === 'active' ? 'status-active' : 
                          tx.status === 'committed' ? 'status-committed' : 'status-rolled-back';
        const opClass = tx.operation === 'read' ? 'op-read' :
                       tx.operation === 'write' ? 'op-write' : 'op-delete';
        
        return `
            <div class="transaction-item">
                <div>
                    <span class="transaction-status ${statusClass}"></span>
                    <strong>${tx.id || 'Transaction'}</strong>
                    <span class="operation-badge ${opClass} ms-2">${tx.operation || 'unknown'}</span>
                </div>
                <div class="text-muted">
                    <small>${tx.collection || 'N/A'} • ${formatTime(tx.timestamp)} • ${tx.duration || 0}ms</small>
                </div>
            </div>
        `;
    }).join('');
}

// Check for alerts
function checkAlerts(metrics) {
    const alertBox = document.getElementById('alertBox');
    const alertMessage = document.getElementById('alertMessage');
    
    if (metrics.errorRate > 5) {
        alertMessage.textContent = `High error rate detected: ${metrics.errorRate.toFixed(1)}%`;
        alertBox.classList.add('show');
    } else if (metrics.avgResponseTime > 1000) {
        alertMessage.textContent = `Slow response times detected: ${metrics.avgResponseTime}ms average`;
        alertBox.classList.add('show');
    } else {
        alertBox.classList.remove('show');
    }
}

// Format number
function formatNumber(num) {
    if (num >= 1000000) return `${(num / 1000000).toFixed(1)}M`;
    if (num >= 1000) return `${(num / 1000).toFixed(1)}K`;
    return num.toString();
}

// Format time
function formatTime(timestamp) {
    if (!timestamp) return 'N/A';
    const date = new Date(timestamp);
    const now = new Date();
    const diff = now - date;
    
    if (diff < 60000) return 'Just now';
    if (diff < 3600000) return `${Math.floor(diff / 60000)}m ago`;
    if (diff < 86400000) return `${Math.floor(diff / 3600000)}h ago`;
    
    return date.toLocaleTimeString();
}

