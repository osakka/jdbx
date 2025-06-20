// JDBX API Compatibility Fix for v6.5.13
// This file addresses backend changes and ensures UI compatibility

// Fix document structure compatibility
function normalizeDocument(doc) {
    // Handle both old and new document structures
    return {
        uuid: doc.uuid || doc.id || doc._id,
        type: doc.type || 'document',
        library: doc.library || 'default',
        collection: doc.collection || doc.type || 'documents',
        owner: doc.owner || 'system',
        created_at: doc.created_at || doc.createdAt || new Date().toISOString(),
        modified_at: doc.modified_at || doc.modifiedAt || new Date().toISOString(),
        name: doc.name || doc.data?.name || doc.title || null,
        data: doc.data || doc
    };
}

// Fix API response normalization
function normalizeApiResponse(response) {
    if (response.documents && Array.isArray(response.documents)) {
        return {
            documents: response.documents.map(normalizeDocument),
            count: response.count || response.documents.length,
            offset: response.offset || 0,
            limit: response.limit || response.documents.length
        };
    }
    
    if (Array.isArray(response)) {
        return {
            documents: response.map(normalizeDocument),
            count: response.length,
            offset: 0,
            limit: response.length
        };
    }
    
    return response;
}

// Override the existing apiRequest function
const originalApiRequest = window.apiRequest;
window.apiRequest = async function(url, options = {}) {
    try {
        // Add authentication token
        const headers = {
            'Content-Type': 'application/json',
            ...options.headers
        };
        
        if (authToken) {
            headers['Authorization'] = `Bearer ${authToken}`;
        }
        
        // Handle unified documents API
        if (url.includes('/api/collections') && url.includes('/documents')) {
            // Convert old collection-based URLs to unified documents API
            const matches = url.match(/\/api\/collections\/([^/]+)\/documents/);
            if (matches) {
                const collection = matches[1];
                const params = new URLSearchParams();
                params.set('type', collection);
                
                // Extract other query parameters
                const urlParts = url.split('?');
                if (urlParts[1]) {
                    const existingParams = new URLSearchParams(urlParts[1]);
                    for (const [key, value] of existingParams) {
                        if (key !== 'collection') {
                            params.set(key, value);
                        }
                    }
                }
                
                url = `/api/documents?${params.toString()}`;
            }
        }
        
        const response = await fetch(API_BASE_URL + url, {
            ...options,
            headers
        });
        
        if (!response.ok) {
            const error = await response.json().catch(() => ({ error: response.statusText }));
            throw new Error(error.error || error.message || 'API request failed');
        }
        
        const data = await response.json();
        
        // Normalize responses for compatibility
        if (url.includes('/api/documents')) {
            return normalizeApiResponse(data);
        }
        
        return data;
    } catch (error) {
        console.error('API request failed:', error);
        throw error;
    }
};

// Fix for metrics endpoint changes
async function fetchMetricsWithFallback() {
    try {
        // Try new metrics endpoint first
        const response = await apiRequest('/api/metrics/system');
        return response;
    } catch (error) {
        console.warn('New metrics endpoint failed, trying legacy endpoint');
        
        // Fallback to querying metric documents
        try {
            const metricsResponse = await apiRequest('/api/documents?type=metric&library=system');
            
            // Transform metric documents into expected format
            const metrics = {};
            metricsResponse.documents.forEach(doc => {
                if (doc.data && doc.data.metrics) {
                    Object.assign(metrics, doc.data.metrics);
                }
            });
            
            return metrics;
        } catch (fallbackError) {
            console.error('Metrics fetch failed:', fallbackError);
            return null;
        }
    }
}

// Fix for authentication endpoint changes
async function loginWithCompatibility(username, password) {
    try {
        const response = await apiRequest('/api/auth/login', {
            method: 'POST',
            body: JSON.stringify({ username, password })
        });
        
        // Handle both old and new response formats
        if (response.token) {
            authToken = response.token;
            localStorage.setItem('jdbx_auth_token', authToken);
            
            // Store user info if provided
            if (response.user) {
                localStorage.setItem('jdbx_user', JSON.stringify(response.user));
            }
            
            return response;
        }
        
        throw new Error('Invalid login response');
    } catch (error) {
        console.error('Login failed:', error);
        throw error;
    }
}

// Fix for library stats
async function getLibraryStatsWithFallback(libraryName) {
    try {
        // Try new stats endpoint
        const response = await apiRequest(`/api/libraries/${libraryName}/stats`);
        return response;
    } catch (error) {
        // Fallback to counting documents
        try {
            const docsResponse = await apiRequest(`/api/documents?library=${libraryName}&count_only=true`);
            return {
                documents: docsResponse.count || 0,
                collections: docsResponse.types?.length || 0,
                size: 'N/A'
            };
        } catch (fallbackError) {
            console.error('Library stats fetch failed:', fallbackError);
            return { documents: 0, collections: 0, size: 'N/A' };
        }
    }
}

// Patch existing functions
if (window.loadDashboard) {
    const originalLoadDashboard = window.loadDashboard;
    window.loadDashboard = async function() {
        try {
            await originalLoadDashboard();
        } catch (error) {
            console.error('Dashboard load failed, applying compatibility fixes');
            
            // Apply compatibility fixes for dashboard
            const stats = await fetchMetricsWithFallback();
            if (stats) {
                updateDashboardWithStats(stats);
            }
        }
    };
}

// Helper function to update dashboard with normalized stats
function updateDashboardWithStats(stats) {
    // Update dashboard elements with normalized data
    const elements = {
        'total-documents': stats.totalDocuments || stats.total_documents || 0,
        'total-libraries': stats.totalLibraries || stats.total_libraries || 0,
        'total-collections': stats.totalCollections || stats.total_collections || 0,
        'requests-per-second': stats.requestsPerSecond || stats.requests_per_second || 0,
        'cache-hit-rate': stats.cacheHitRate || stats.cache_hit_rate || 0,
        'memory-usage': stats.memoryUsage || stats.memory_usage || 0
    };
    
    for (const [id, value] of Object.entries(elements)) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = typeof value === 'number' ? value.toLocaleString() : value;
        }
    }
}

// Add version indicator
document.addEventListener('DOMContentLoaded', function() {
    const versionBadge = document.querySelector('.version-badge');
    if (versionBadge) {
        versionBadge.textContent = 'v6.5.13';
    }
    
    // Add compatibility mode indicator
    const navbar = document.querySelector('.navbar-nav');
    if (navbar) {
        const compatIndicator = document.createElement('li');
        compatIndicator.className = 'nav-item';
        compatIndicator.innerHTML = `
            <span class="nav-link text-warning" title="API Compatibility Mode Active">
                <i class="bi bi-exclamation-triangle"></i>
                Compat Mode
            </span>
        `;
        navbar.appendChild(compatIndicator);
    }
});

console.log('JDBX API Compatibility Fix v6.5.13 loaded');