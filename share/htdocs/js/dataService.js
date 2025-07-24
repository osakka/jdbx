/**
 * JDBX Data Service - Single Data Source
 * All API interactions flow through this service
 */

import { State } from './state.js';

class DataServiceClass {
    constructor() {
        this.apiBase = '';
    }

    /**
     * Make authenticated API request
     * @param {string} endpoint - API endpoint
     * @param {object} options - Fetch options
     * @returns {Promise<object|null>} API response or null on error
     */
    async apiRequest(endpoint, options = {}) {
        // Try localStorage first, then State as fallback
        let token = localStorage.getItem('jdbx_auth_token');
        if (!token && typeof State !== 'undefined') {
            token = State.get('auth.token');
        }
        
        if (!token && !endpoint.includes('/auth/login')) {
            console.error('❌ No authentication token');
            return null;
        }

        const url = this.apiBase + endpoint;
        const config = {
            headers: {
                'Content-Type': 'application/json',
                ...(token && { 'Authorization': `Bearer ${token}` }),
                ...options.headers
            },
            ...options
        };

        try {
            console.log(`🌐 API Request: ${endpoint}`);
            const response = await fetch(url, config);
            
            if (response.status === 401) {
                console.error('❌ Authentication failed');
                localStorage.removeItem('jdbx_auth_token');
                return null;
            }
            
            if (!response.ok) {
                const errorText = await response.text();
                console.error(`❌ API Error ${response.status}:`, errorText);
                return null;
            }

            const data = await response.json();
            console.log(`✅ API Success: ${endpoint}`);
            return data;
            
        } catch (error) {
            console.error(`❌ Network Error: ${endpoint}`, error);
            return null;
        }
    }

    /**
     * Authenticate user
     * @param {string} username
     * @param {string} password
     * @returns {Promise<object|null>} User data or null
     */
    async authenticate(username, password) {
        const response = await this.apiRequest('/api/auth/login', {
            method: 'POST',
            body: JSON.stringify({ username, password })
        });

        if (response?.token) {
            localStorage.setItem('jdbx_auth_token', response.token);
            return {
                user: { username, uuid: response.user_uuid },
                token: response.token
            };
        }
        
        return null;
    }

    /**
     * Validate current session
     * @returns {Promise<object|null>} Session data or null
     */
    async validateSession() {
        const response = await this.apiRequest('/api/auth/session');
        return response?.data || response;
    }

    /**
     * Fetch dashboard data
     * @returns {Promise<object>} Dashboard data
     */
    async fetchDashboardData() {
        const [statsResponse, collectionsResponse] = await Promise.all([
            this.apiRequest('/api/metrics/stats?format=json&library=default'),
            this.apiRequest('/api/collections?library=default&stats=true')
        ]);

        // Process collections
        const collections = collectionsResponse?.collections || [];
        
        // Calculate stats from collections
        const stats = {
            totalDocuments: collections.reduce((sum, col) => sum + (col.document_count || 0), 0),
            databaseSize: 0, // TODO: Get actual database size from API
            totalRequests: 0,
            totalOperations: 0
        };
        
        // Add metrics data to stats
        if (statsResponse?.metrics) {
            statsResponse.metrics.forEach(metric => {
                switch(metric.name) {
                    case 'server_requests_total':
                        stats.totalRequests = metric.value || 0;
                        break;
                    case 'db_operations_total':
                        stats.totalOperations = metric.value || 0;
                        break;
                }
            });
        }

        return { stats, collections };
    }

    /**
     * Fetch collections
     * @param {string} library - Library name
     * @returns {Promise<Array>} Collections array
     */
    async fetchCollections(library = 'default') {
        const response = await this.apiRequest(`/api/collections?library=${library}`);
        return response?.collections || [];
    }

    /**
     * Fetch documents
     * @param {string} library - Library name
     * @param {object} query - Query object
     * @returns {Promise<Array>} Documents array
     */
    async fetchDocuments(library = 'default', query = {}) {
        let endpoint = `/api/documents?library=${library}`;
        if (Object.keys(query).length > 0) {
            endpoint += `&query=${encodeURIComponent(JSON.stringify(query))}`;
        }
        
        const response = await this.apiRequest(endpoint);
        return response?.documents || [];
    }

    /**
     * Fetch RBAC data
     * @param {string} library - Library name
     * @returns {Promise<object>} RBAC data
     */
    async fetchRBACData(library = 'default') {
        const [usersResponse, rolesResponse] = await Promise.all([
            this.apiRequest(`/api/rbac/users?library=${library}`),
            this.apiRequest(`/api/rbac/roles?library=${library}`)
        ]);

        return {
            users: usersResponse?.users || [],
            roles: rolesResponse?.roles || []
        };
    }

    /**
     * Fetch metrics
     * @param {string} library - Library name
     * @param {string} range - Time range
     * @returns {Promise<Array>} Metrics array
     */
    async fetchMetrics(library = 'default', range = '1h') {
        const response = await this.apiRequest(`/api/metrics/stats?format=json&library=${library}&range=${range}`);
        return response?.metrics || [];
    }

    /**
     * Fetch libraries
     * @returns {Promise<Array>} Libraries array
     */
    async fetchLibraries() {
        const response = await this.apiRequest('/api/libraries');
        return response?.libraries || [
            { name: 'default', displayName: 'Default Library' },
            { name: 'system', displayName: 'System Library' }
        ];
    }

    /**
     * Create document
     * @param {object} document - Document data
     * @param {string} library - Library name
     * @returns {Promise<object|null>} Created document or null
     */
    async createDocument(document, library = 'default') {
        return await this.apiRequest('/api/documents', {
            method: 'POST',
            body: JSON.stringify({ ...document, library })
        });
    }

    /**
     * Update document
     * @param {string} id - Document ID
     * @param {object} updates - Document updates
     * @returns {Promise<object|null>} Updated document or null
     */
    async updateDocument(id, updates) {
        return await this.apiRequest(`/api/documents/${id}`, {
            method: 'PUT',
            body: JSON.stringify(updates)
        });
    }

    /**
     * Delete document
     * @param {string} id - Document ID
     * @returns {Promise<boolean>} Success status
     */
    async deleteDocument(id) {
        const response = await this.apiRequest(`/api/documents/${id}`, {
            method: 'DELETE'
        });
        return response !== null;
    }
}

// Single data service instance
export const DataService = new DataServiceClass();