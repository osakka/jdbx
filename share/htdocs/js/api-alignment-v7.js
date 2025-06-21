// JDBX API Alignment v7.0.1 - Single Source of Truth
// This file aligns the UI with the actual server implementation
// NO PARALLEL IMPLEMENTATIONS - Server is the truth!

(function() {
    'use strict';
    
    console.log('JDBX API Alignment v7.0.1 loading...');
    
    // Store original functions for patching
    const originalFetch = window.fetch;
    
    // Fix authentication endpoints to match server
    const API_ENDPOINT_MAP = {
        '/api/auth/login': '/api/login',      // Server has /api/login, not /api/auth/login
        '/api/auth/logout': '/api/logout',    // Align logout endpoint
        '/api/auth/refresh': '/api/refresh',  // Align refresh endpoint
    };
    
    // Override fetch to fix API endpoints
    window.fetch = function(url, options = {}) {
        // Fix authentication endpoints
        let fixedUrl = url;
        for (const [oldPath, newPath] of Object.entries(API_ENDPOINT_MAP)) {
            if (url.includes(oldPath)) {
                fixedUrl = url.replace(oldPath, newPath);
                console.log(`API Alignment: ${oldPath} → ${newPath}`);
            }
        }
        
        return originalFetch.call(this, fixedUrl, options);
    };
    
    // Fix getActualDatabaseSize to work with server response format
    if (window.getActualDatabaseSize) {
        window.getActualDatabaseSize = async function() {
            try {
                // Get all documents to calculate size
                const response = await apiRequest('/api/documents');
                let totalSize = 0;
                
                if (response && response.documents) {
                    // Calculate actual size from document JSON
                    response.documents.forEach(doc => {
                        totalSize += JSON.stringify(doc).length;
                    });
                }
                
                // Apply minimum size for system documents (20KB)
                const actualSize = Math.max(totalSize, 20480);
                window.currentDatabaseSizeBytes = actualSize;
                
                return actualSize;
            } catch (error) {
                console.warn('Failed to get database size:', error);
                return window.currentDatabaseSizeBytes || 20480;
            }
        };
    }
    
    // Fix session validation to use lightweight endpoint
    if (window.validateSession) {
        const originalValidateSession = window.validateSession;
        window.validateSession = async function() {
            const currentToken = localStorage.getItem('jdbx_auth_token');
            authToken = currentToken;
            
            if (!currentToken) {
                console.log('No auth token, redirecting to login');
                window.location.href = '/login.html';
                return false;
            }
            
            try {
                // Use /api/health for lightweight session check
                const response = await fetch('/api/health', {
                    method: 'GET',
                    headers: {
                        'Authorization': `Bearer ${currentToken}`
                    }
                });
                
                if (response.status === 401) {
                    console.log('Session invalid (401), redirecting to login');
                    localStorage.removeItem('jdbx_auth_token');
                    localStorage.removeItem('jdbx_refresh_token');
                    if (sessionCheckInterval) {
                        clearInterval(sessionCheckInterval);
                    }
                    window.location.href = '/login.html';
                    return false;
                }
                
                return response.ok || response.status === 200;
            } catch (error) {
                console.error('Session validation error:', error);
                // On network error, assume session is still valid
                return true;
            }
        };
    }
    
    // Fix virtual collections to extract from unified documents
    window.getVirtualCollections = async function(library = 'default') {
        try {
            // Get all documents and extract unique types
            const response = await apiRequest(`/api/documents?library=${library}`);
            const types = new Map();
            
            if (response && response.documents) {
                response.documents.forEach(doc => {
                    if (doc.type && !types.has(doc.type)) {
                        types.set(doc.type, {
                            name: doc.type,
                            type: doc.type,
                            library: library,
                            count: 0
                        });
                    }
                    if (doc.type) {
                        types.get(doc.type).count++;
                    }
                });
            }
            
            return Array.from(types.values());
        } catch (error) {
            console.error('Failed to get virtual collections:', error);
            return [];
        }
    };
    
    // Fix loadCollections to use virtual collections
    if (window.loadCollections) {
        window.loadCollections = async function() {
            try {
                const virtualCollections = await getVirtualCollections(currentLibrary);
                
                // Update collections array
                collections = virtualCollections;
                
                // Update UI
                const collectionsList = document.getElementById('collections-list');
                if (collectionsList) {
                    collectionsList.innerHTML = '';
                    
                    if (virtualCollections.length === 0) {
                        collectionsList.innerHTML = '<div class="text-muted p-3">No document types found</div>';
                    } else {
                        virtualCollections.forEach(collection => {
                            const item = document.createElement('a');
                            item.href = '#';
                            item.className = 'list-group-item list-group-item-action';
                            if (currentCollection === collection.name) {
                                item.classList.add('active');
                            }
                            item.innerHTML = `
                                <div class="d-flex justify-content-between align-items-center">
                                    <span>${collection.name}</span>
                                    <span class="badge bg-secondary">${collection.count}</span>
                                </div>
                            `;
                            item.onclick = (e) => {
                                e.preventDefault();
                                selectCollection(collection.name);
                            };
                            collectionsList.appendChild(item);
                        });
                    }
                }
            } catch (error) {
                console.error('Error loading virtual collections:', error);
                showError('Failed to load document types');
            }
        };
    }
    
    // Fix metrics API to work with server format
    window.getMetricsData = async function() {
        try {
            const response = await apiRequest('/api/metrics');
            
            // Transform server response to expected format
            return {
                operations: response.operations_metrics || {
                    total_operations: 0,
                    reads: 0,
                    writes: 0,
                    deletes: 0
                },
                performance: response.performance_metrics || {
                    average_response_time: 0,
                    p95_response_time: 0,
                    p99_response_time: 0
                },
                cache: response.cache_metrics || {
                    hit_rate: 0,
                    total_hits: 0,
                    total_misses: 0
                },
                memory: response.memory_metrics || {
                    used_memory: 0,
                    total_memory: 0,
                    percentage: 0
                }
            };
        } catch (error) {
            console.error('Failed to get metrics:', error);
            // Return default structure on error
            return {
                operations: { total_operations: 0, reads: 0, writes: 0, deletes: 0 },
                performance: { average_response_time: 0, p95_response_time: 0, p99_response_time: 0 },
                cache: { hit_rate: 0, total_hits: 0, total_misses: 0 },
                memory: { used_memory: 0, total_memory: 0, percentage: 0 }
            };
        }
    };
    
    // Add indicator that alignment is active
    document.addEventListener('DOMContentLoaded', function() {
        // Update version badge
        const versionBadge = document.querySelector('.version-badge');
        if (versionBadge) {
            versionBadge.textContent = 'v7.0.1';
        }
        
        // Add alignment indicator
        const navbar = document.querySelector('.navbar-nav');
        if (navbar && !document.querySelector('.alignment-indicator')) {
            const alignmentIndicator = document.createElement('li');
            alignmentIndicator.className = 'nav-item alignment-indicator';
            alignmentIndicator.innerHTML = `
                <span class="nav-link text-success" title="API Alignment Active - Server is Truth">
                    <i class="bi bi-check-circle"></i>
                    v7.0.1 Aligned
                </span>
            `;
            navbar.appendChild(alignmentIndicator);
        }
        
        console.log('JDBX API Alignment v7.0.1 active - UI aligned with server implementation');
    });
    
})();