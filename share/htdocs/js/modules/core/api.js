/**
 * JDBX UI API Client
 * Unified API client with authentication and error handling
 */

import { API_BASE_URL, UNIFIED_API } from './constants.js';
import { showNotification } from '../ui/notifications.js';

/**
 * Get current authentication token
 * @returns {string|null} JWT token or null
 */
export function getAuthToken() {
    return localStorage.getItem('jdbx_auth_token');
}

/**
 * Set authentication token
 * @param {string} token - JWT token to store
 */
export function setAuthToken(token) {
    if (token) {
        localStorage.setItem('jdbx_auth_token', token);
    } else {
        localStorage.removeItem('jdbx_auth_token');
    }
}

/**
 * Check if user is authenticated
 * @returns {boolean} True if authenticated
 */
export function isAuthenticated() {
    return !!getAuthToken();
}

/**
 * Redirect to login page
 */
export function redirectToLogin() {
    window.location.href = '/login.html';
}

/**
 * Logout and redirect to login
 */
export function logout() {
    setAuthToken(null);
    showNotification('Logged out successfully', 'info');
    redirectToLogin();
}

/**
 * Make authenticated API request
 * @param {string} endpoint - API endpoint
 * @param {object} options - Fetch options
 * @returns {Promise<object>} API response
 */
export async function apiRequest(endpoint, options = {}) {
    const token = getAuthToken();
    console.log('🌐 apiRequest called:', endpoint, 'token exists:', !!token);
    console.log('🌐 apiRequest token value:', token ? token.substring(0, 20) + '...' : 'null');
    
    if (!token) {
        console.log('❌ No token in apiRequest, redirecting to login');
        console.log('❌ localStorage keys:', Object.keys(localStorage));
        redirectToLogin();
        return null;
    }

    const url = API_BASE_URL + endpoint;
    const config = {
        headers: {
            'Authorization': `Bearer ${token}`,
            'Content-Type': 'application/json',
            ...options.headers
        },
        ...options
    };

    try {
        const response = await fetch(url, config);
        
        if (response.status === 401) {
            showNotification('Session expired. Please login again.', 'warning');
            logout();
            return null;
        }
        
        if (!response.ok) {
            const errorText = await response.text();
            let errorMessage;
            
            try {
                const errorJson = JSON.parse(errorText);
                errorMessage = errorJson.error || errorJson.message || `HTTP ${response.status}`;
            } catch (e) {
                errorMessage = errorText || `HTTP ${response.status}`;
            }
            
            throw new Error(errorMessage);
        }

        const data = await response.json();
        return data;
        
    } catch (error) {
        if (error.name === 'TypeError' && error.message.includes('fetch')) {
            showNotification('Network error. Please check your connection.', 'danger');
        } else {
            showNotification(`API Error: ${error.message}`, 'danger');
        }
        console.error('API request failed:', endpoint, error);
        return null; // Return null instead of throwing to match expected behavior
    }
}

/**
 * Validate current session
 * @returns {Promise<boolean>} True if session is valid
 */
export async function validateSession() {
    try {
        const response = await apiRequest('/api/auth/validate');
        return response && response.valid === true;
    } catch (error) {
        return false;
    }
}

/**
 * Get actual database size from unified documents API
 * @returns {Promise<number>} Database size in bytes
 */
export async function getActualDatabaseSize() {
    try {
        const response = await apiRequest('/api/documents?stats=true');
        let totalSize = 0;
        
        if (response && response.stats) {
            totalSize = response.stats.total_size || 0;
        } else if (response && response.documents) {
            // Fallback: estimate size from document count
            totalSize = response.documents.length * 1024; // Rough estimate
        }
        
        // Apply minimum size for system documents (20KB)
        const actualSize = Math.max(totalSize, 20480);
        
        // Store globally for consistency
        if (window) {
            window.currentDatabaseSizeBytes = actualSize;
        }
        
        return actualSize;
    } catch (error) {
        // Return cached value or minimum size if API fails
        return (window && window.currentDatabaseSizeBytes) || 20480;
    }
}

/**
 * Unified Documents API methods
 */
export const documentsAPI = {
    /**
     * Get all documents with optional query
     * @param {object} options - Query options
     * @returns {Promise<object>} Documents response
     */
    async getAll(options = {}) {
        const queryParams = new URLSearchParams();
        
        if (options.query) {
            queryParams.append('query', JSON.stringify(options.query));
        }
        if (options.library) {
            queryParams.append('library', options.library);
        }
        if (options.collection) {
            queryParams.append('collection', options.collection);
        }
        if (options.stats) {
            queryParams.append('stats', 'true');
        }
        
        const endpoint = UNIFIED_API.documents + (queryParams.toString() ? '?' + queryParams.toString() : '');
        return await apiRequest(endpoint);
    },

    /**
     * Create new document
     * @param {object} document - Document to create
     * @param {string} library - Library name
     * @returns {Promise<object>} Created document
     */
    async create(document, library = 'default') {
        return await apiRequest(UNIFIED_API.documents, {
            method: 'POST',
            body: JSON.stringify({ ...document, library })
        });
    },

    /**
     * Update document by ID
     * @param {string} id - Document ID
     * @param {object} updates - Updates to apply
     * @returns {Promise<object>} Updated document
     */
    async update(id, updates) {
        return await apiRequest(`${UNIFIED_API.documents}/${id}`, {
            method: 'PUT',
            body: JSON.stringify(updates)
        });
    },

    /**
     * Delete document by ID
     * @param {string} id - Document ID
     * @returns {Promise<object>} Deletion result
     */
    async delete(id) {
        return await apiRequest(`${UNIFIED_API.documents}/${id}`, {
            method: 'DELETE'
        });
    }
};

/**
 * Collections API methods
 */
export const collectionsAPI = {
    /**
     * Get all virtual collections
     * @returns {Promise<object>} Collections response
     */
    async getAll() {
        return await apiRequest(UNIFIED_API.collections);
    }
};

/**
 * Libraries API methods
 */
export const librariesAPI = {
    /**
     * Get all libraries
     * @returns {Promise<object>} Libraries response
     */
    async getAll() {
        return await apiRequest(UNIFIED_API.libraries);
    },

    /**
     * Create new library
     * @param {object} library - Library to create
     * @returns {Promise<object>} Created library
     */
    async create(library) {
        return await apiRequest(UNIFIED_API.libraries, {
            method: 'POST',
            body: JSON.stringify(library)
        });
    },

    /**
     * Switch to library
     * @param {string} libraryName - Library name to switch to
     * @returns {Promise<object>} Switch result
     */
    async switchTo(libraryName) {
        return await apiRequest('/api/auth/library', {
            method: 'POST',
            body: JSON.stringify({ library: libraryName })
        });
    }
};

// Default API function for backward compatibility
export const api = apiRequest;

// Export as default as well
export default apiRequest;