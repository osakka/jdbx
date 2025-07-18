/**
 * Data Manager
 * Handles all data operations with proper state management
 */

import { state } from '../core/state.js';
import { api } from '../core/api.js';

export class DataManager {
    constructor() {
        this.isInitialized = false;
        this.cache = new Map();
    }

    /**
     * Initialize data manager
     */
    async initialize() {
        if (this.isInitialized) return;

        console.log('📊 Initializing Data Manager');

        // Load initial data
        await this.loadInitialData();

        this.isInitialized = true;
    }

    /**
     * Load initial application data
     */
    async loadInitialData() {
        try {
            // Load libraries
            const libraries = await this.loadLibraries();
            state.setState('libraries', libraries);

            // Load collections for current library
            const collections = await this.loadCollections();
            state.setState('collections', collections);

        } catch (error) {
            console.error('Failed to load initial data:', error);
        }
    }

    /**
     * Load libraries
     * @returns {Array} Libraries array
     */
    async loadLibraries() {
        try {
            const response = await api('/api/libraries');
            return response.libraries || [];
        } catch (error) {
            console.error('Failed to load libraries:', error);
            return [];
        }
    }

    /**
     * Load collections
     * @returns {Array} Collections array
     */
    async loadCollections() {
        try {
            const currentLibrary = state.get('currentLibrary');
            const response = await api(`/api/collections?library=${currentLibrary}`);
            return response.collections || [];
        } catch (error) {
            console.error('Failed to load collections:', error);
            return [];
        }
    }

    /**
     * Load documents
     * @param {string} query - Query parameters
     * @returns {Array} Documents array
     */
    async loadDocuments(query = '') {
        try {
            const currentLibrary = state.get('currentLibrary');
            const url = `/api/documents?library=${currentLibrary}${query ? '&' + query : ''}`;
            const response = await api(url);
            
            const documents = response.documents || [];
            state.setState('documents', documents);
            
            return documents;
        } catch (error) {
            console.error('Failed to load documents:', error);
            return [];
        }
    }

    /**
     * Load RBAC data
     * @returns {object} RBAC data
     */
    async loadRBACData() {
        try {
            const currentLibrary = state.get('currentLibrary');
            
            // Load users
            const usersResponse = await api(`/api/rbac/users?library=${currentLibrary}`);
            const users = usersResponse.users || [];
            
            // Load roles
            const rolesResponse = await api(`/api/rbac/roles?library=${currentLibrary}`);
            const roles = rolesResponse.roles || [];
            
            // Load permissions
            const permissionsResponse = await api(`/api/rbac/permissions?library=${currentLibrary}`);
            const permissions = permissionsResponse.permissions || [];
            
            // Update state
            state.setState({
                allUsers: users,
                allRoles: roles,
                allPermissions: permissions
            });
            
            return { users, roles, permissions };
        } catch (error) {
            console.error('Failed to load RBAC data:', error);
            return { users: [], roles: [], permissions: [] };
        }
    }

    /**
     * Load dashboard data
     * @returns {object} Dashboard data
     */
    async loadDashboardData() {
        try {
            const currentLibrary = state.get('currentLibrary');
            
            // Load system stats
            const statsResponse = await api(`/api/metrics/stats?library=${currentLibrary}`);
            const stats = statsResponse.stats || {};
            
            // Load collections data
            const collectionsResponse = await api(`/api/collections?library=${currentLibrary}&stats=true`);
            const collections = collectionsResponse.collections || [];
            
            // Update previous data for optimization
            state.updatePreviousData({
                totalCollections: collections.length,
                totalDocuments: stats.totalDocuments || 0,
                databaseSize: stats.databaseSize || 0,
                collectionsData: collections
            });
            
            return { stats, collections };
        } catch (error) {
            console.error('Failed to load dashboard data:', error);
            return { stats: {}, collections: [] };
        }
    }

    /**
     * Load metrics data
     * @param {string} timeRange - Time range for metrics
     * @returns {object} Metrics data
     */
    async loadMetricsData(timeRange = '1h') {
        try {
            const response = await api(`/api/metrics?range=${timeRange}`);
            return response.metrics || {};
        } catch (error) {
            console.error('Failed to load metrics data:', error);
            return {};
        }
    }

    /**
     * Switch library
     * @param {string} libraryName - Library name
     */
    async switchLibrary(libraryName) {
        if (libraryName === state.get('currentLibrary')) return;
        
        console.log(`📚 Switching to library: ${libraryName}`);
        
        // Update state
        state.setState('currentLibrary', libraryName);
        
        // Clear current data
        state.setState({
            collections: [],
            documents: [],
            allUsers: [],
            allRoles: [],
            allPermissions: []
        });
        
        // Reload data for new library
        await this.loadInitialData();
    }

    /**
     * Get cached data
     * @param {string} key - Cache key
     * @returns {*} Cached data
     */
    getCachedData(key) {
        return this.cache.get(key);
    }

    /**
     * Set cached data
     * @param {string} key - Cache key
     * @param {*} data - Data to cache
     * @param {number} ttl - Time to live in milliseconds
     */
    setCachedData(key, data, ttl = 300000) { // 5 minutes default
        this.cache.set(key, {
            data,
            expiry: Date.now() + ttl
        });
    }

    /**
     * Clear cache
     */
    clearCache() {
        this.cache.clear();
    }

    /**
     * Cleanup resources
     */
    cleanup() {
        this.clearCache();
    }
}