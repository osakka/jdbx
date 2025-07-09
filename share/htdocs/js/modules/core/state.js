/**
 * JDBX UI State Management
 * Centralized state management with reactivity
 */

import { VIEW_TYPES, STORAGE_KEYS } from './constants.js';

/**
 * Application state store
 */
class StateStore {
    constructor() {
        this.state = {
            // Authentication
            isAuthenticated: false,
            user: null,
            
            // Navigation
            currentView: VIEW_TYPES.DASHBOARD,
            
            // Libraries and Collections
            currentLibrary: localStorage.getItem(STORAGE_KEYS.CURRENT_LIBRARY) || 'default',
            libraries: [],
            collections: [],
            
            // Documents
            documents: [],
            currentDocument: null,
            selectedDocuments: [],
            
            // RBAC
            allUsers: [],
            allRoles: [],
            allPermissions: [],
            selectedRole: null,
            
            // UI State
            isLoading: false,
            queryBuilderVisible: false,
            
            // Charts
            chartInstances: new Map(),
            
            // Polling
            refreshInterval: null,
            
            // Previous data for optimization
            previousData: {
                totalCollections: null,
                totalDocuments: null,
                databaseSize: null,
                collectionsData: {},
                systemHealth: null,
                lastUpdate: null
            }
        };
        
        this.subscribers = new Map();
        this.nextSubscriberId = 0;
    }

    /**
     * Get current state
     * @returns {object} Current state
     */
    getState() {
        return { ...this.state };
    }

    /**
     * Get specific state value
     * @param {string} key - State key
     * @returns {*} State value
     */
    get(key) {
        return this.state[key];
    }

    /**
     * Set state value and notify subscribers
     * @param {string|object} key - State key or state object
     * @param {*} value - State value (if key is string)
     */
    setState(key, value) {
        const prevState = { ...this.state };
        
        if (typeof key === 'object') {
            // Merge state object
            this.state = { ...this.state, ...key };
        } else {
            // Set single value
            this.state[key] = value;
        }
        
        // Persist certain state to localStorage
        this.persistState(key, value);
        
        // Notify subscribers
        this.notifySubscribers(prevState, this.state);
    }

    /**
     * Subscribe to state changes
     * @param {Function} callback - Callback function
     * @param {Array} keys - Specific keys to watch (optional)
     * @returns {Function} Unsubscribe function
     */
    subscribe(callback, keys = null) {
        const subscriberId = this.nextSubscriberId++;
        
        this.subscribers.set(subscriberId, {
            callback,
            keys
        });
        
        // Return unsubscribe function
        return () => {
            this.subscribers.delete(subscriberId);
        };
    }

    /**
     * Notify all subscribers of state changes
     * @param {object} prevState - Previous state
     * @param {object} newState - New state
     */
    notifySubscribers(prevState, newState) {
        this.subscribers.forEach(({ callback, keys }) => {
            if (!keys) {
                // No specific keys, notify of all changes
                callback(newState, prevState);
            } else {
                // Check if any watched keys changed
                const hasChanges = keys.some(key => 
                    prevState[key] !== newState[key]
                );
                
                if (hasChanges) {
                    callback(newState, prevState);
                }
            }
        });
    }

    /**
     * Persist certain state values to localStorage
     * @param {string|object} key - State key or state object
     * @param {*} value - State value
     */
    persistState(key, value) {
        const persistKeys = {
            currentLibrary: STORAGE_KEYS.CURRENT_LIBRARY
        };
        
        if (typeof key === 'string' && persistKeys[key]) {
            localStorage.setItem(persistKeys[key], value);
        } else if (typeof key === 'object') {
            Object.entries(key).forEach(([k, v]) => {
                if (persistKeys[k]) {
                    localStorage.setItem(persistKeys[k], v);
                }
            });
        }
    }

    /**
     * Reset state to initial values
     */
    reset() {
        const initialState = {
            isAuthenticated: false,
            user: null,
            currentView: VIEW_TYPES.DASHBOARD,
            currentLibrary: 'default',
            libraries: [],
            collections: [],
            documents: [],
            currentDocument: null,
            selectedDocuments: [],
            allUsers: [],
            allRoles: [],
            allPermissions: [],
            selectedRole: null,
            isLoading: false,
            queryBuilderVisible: false,
            chartInstances: new Map(),
            refreshInterval: null,
            previousData: {
                totalCollections: null,
                totalDocuments: null,
                databaseSize: null,
                collectionsData: {},
                systemHealth: null,
                lastUpdate: null
            }
        };
        
        this.setState(initialState);
    }

    /**
     * Add chart instance
     * @param {string} chartId - Chart identifier
     * @param {object} chartInstance - Chart.js instance
     */
    addChart(chartId, chartInstance) {
        const charts = this.state.chartInstances;
        charts.set(chartId, chartInstance);
        this.setState({ chartInstances: charts });
    }

    /**
     * Remove chart instance
     * @param {string} chartId - Chart identifier
     */
    removeChart(chartId) {
        const charts = this.state.chartInstances;
        const chartInstance = charts.get(chartId);
        
        if (chartInstance && typeof chartInstance.destroy === 'function') {
            chartInstance.destroy();
        }
        
        charts.delete(chartId);
        this.setState({ chartInstances: charts });
    }

    /**
     * Clear all chart instances
     */
    clearAllCharts() {
        const charts = this.state.chartInstances;
        
        charts.forEach((chartInstance, chartId) => {
            if (chartInstance && typeof chartInstance.destroy === 'function') {
                chartInstance.destroy();
            }
        });
        
        charts.clear();
        this.setState({ chartInstances: new Map() });
    }

    /**
     * Update previous data for optimization
     * @param {object} data - New data to compare against
     */
    updatePreviousData(data) {
        this.setState({
            previousData: {
                ...this.state.previousData,
                ...data,
                lastUpdate: Date.now()
            }
        });
    }

    /**
     * Check if data has changed since last update
     * @param {object} newData - New data to compare
     * @returns {boolean} True if data has changed
     */
    hasDataChanged(newData) {
        const prev = this.state.previousData;
        
        return (
            prev.totalCollections !== newData.totalCollections ||
            prev.totalDocuments !== newData.totalDocuments ||
            prev.databaseSize !== newData.databaseSize ||
            JSON.stringify(prev.collectionsData) !== JSON.stringify(newData.collectionsData)
        );
    }
}

// Create global state instance
export const state = new StateStore();

// Export convenience functions
export const getState = () => state.getState();
export const setState = (key, value) => state.setState(key, value);
export const subscribe = (callback, keys) => state.subscribe(callback, keys);

// Make state globally available for debugging
if (typeof window !== 'undefined') {
    window.jdbxState = state;
}