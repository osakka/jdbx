/**
 * JDBX State - Single Source of Truth
 * All application state flows through this store
 */

class StateStore {
    constructor() {
        this.state = {
            // Authentication
            auth: {
                user: null,
                token: null,
                isAuthenticated: false
            },
            
            // Navigation
            navigation: {
                currentView: 'dashboard',
                previousView: null
            },
            
            // Data (All Views)
            data: {
                collections: [],
                documents: [],
                stats: {},
                metrics: [],
                users: [],
                roles: [],
                libraries: []
            },
            
            // UI State
            ui: {
                loading: false,
                currentLibrary: 'default',
                notifications: [],
                polling: { enabled: true }
            }
        };
        
        this.subscribers = new Set();
    }

    /**
     * Get state value by path
     * @param {string} path - Dot notation path (e.g., 'auth.user')
     * @returns {any} State value
     */
    get(path) {
        return this.getByPath(this.state, path);
    }

    /**
     * Update state and notify subscribers
     * @param {string} path - Dot notation path
     * @param {any} value - New value
     */
    update(path, value) {
        console.log(`🏪 State update: ${path} =`, value);
        this.setByPath(this.state, path, value);
        this.notifySubscribers();
    }

    /**
     * Subscribe to state changes
     * @param {Function} callback - Function to call on state change
     */
    subscribe(callback) {
        this.subscribers.add(callback);
        return () => this.subscribers.delete(callback);
    }

    /**
     * Notify all subscribers of state change
     */
    notifySubscribers() {
        this.subscribers.forEach(callback => {
            try {
                callback(this.state);
            } catch (error) {
                console.error('State subscriber error:', error);
            }
        });
    }

    /**
     * Get value by dot notation path
     */
    getByPath(obj, path) {
        return path.split('.').reduce((current, key) => current?.[key], obj);
    }

    /**
     * Set value by dot notation path
     */
    setByPath(obj, path, value) {
        const keys = path.split('.');
        const lastKey = keys.pop();
        const target = keys.reduce((current, key) => {
            if (!(key in current)) current[key] = {};
            return current[key];
        }, obj);
        target[lastKey] = value;
    }

    /**
     * Get complete state (for debugging)
     */
    getAll() {
        return { ...this.state };
    }
}

// Single state instance
export const State = new StateStore();