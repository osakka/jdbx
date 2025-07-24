/**
 * JDBX Controller - Single Entry Point
 * All user interactions flow through this controller
 */

import { State } from './state.js';
import { DataService } from './dataService.js';
import { Renderer } from './renderer.js';

class ControllerClass {
    constructor() {
        this.pollingInterval = null;
        this.isInitialized = false;
    }

    /**
     * Initialize application
     */
    async initialize() {
        if (this.isInitialized) return;

        console.log('🚀 Controller initializing...');

        // Check authentication FIRST (before setting up state subscription)
        const token = localStorage.getItem('jdbx_auth_token');
        console.log('🔍 Checking authentication:', { hasToken: !!token });
        
        if (token) {
            console.log('🔐 Found stored token, validating session...');
            const sessionData = await DataService.validateSession();
            
            if (sessionData && sessionData.username) {
                console.log('✅ Session validated for user:', sessionData.username);
                
                // Ensure token is in localStorage (fix for current session)
                if (!localStorage.getItem('jdbx_auth_token')) {
                    localStorage.setItem('jdbx_auth_token', token);
                    console.log('🔧 Token restored to localStorage');
                }
                
                // Set auth state BEFORE setting up renderer subscription
                State.update('auth.user', { 
                    username: sessionData.username, 
                    uuid: sessionData.user_uuid 
                });
                State.update('auth.token', token);
                State.update('auth.isAuthenticated', true);
                State.update('ui.currentLibrary', sessionData.library || 'default');
                
                // NOW set up state subscription (after auth is established)
                State.subscribe((state) => {
                    Renderer.render(state);
                });
                
                // Load initial data
                await this.loadInitialData();
            } else {
                // Invalid session
                console.log('❌ Session validation failed');
                this.logout();
                return;
            }
        } else {
            // No token - redirect to login
            console.log('❌ No token found');
            this.logout();
            return;
        }

        // Setup navigation
        this.setupNavigation();

        // Start polling
        this.startPolling();

        // Show page content
        document.documentElement.classList.add('loaded');

        this.isInitialized = true;
        console.log('✅ Controller initialized successfully');
    }

    /**
     * Load initial application data
     */
    async loadInitialData() {
        console.log('📊 Loading initial data...');
        
        try {
            // Load libraries first
            const libraries = await DataService.fetchLibraries();
            if (libraries && libraries.length > 0) {
                State.update('data.libraries', libraries);
                console.log(`✅ Loaded ${libraries.length} libraries`);
            } else {
                console.warn('⚠️ No libraries returned, using fallback');
                State.update('data.libraries', [
                    { name: 'default', displayName: 'Default Library' },
                    { name: 'system', displayName: 'System Library' }
                ]);
            }

            // Load dashboard data
            await this.loadDashboard();
        } catch (error) {
            console.error('❌ Error loading initial data:', error);
            // Don't let data loading errors affect authentication
        }
    }

    /**
     * Setup navigation handling
     */
    setupNavigation() {
        // Handle hash changes
        window.addEventListener('hashchange', () => {
            const view = window.location.hash.substring(1) || 'dashboard';
            this.switchView(view);
        });

        // Handle initial view
        const initialView = window.location.hash.substring(1) || 'dashboard';
        this.switchView(initialView);
    }

    /**
     * Start polling for current view
     */
    startPolling() {
        if (this.pollingInterval) {
            clearInterval(this.pollingInterval);
        }

        this.pollingInterval = setInterval(() => {
            const currentView = State.get('navigation.currentView');
            this.refreshCurrentView();
        }, 30000); // 30 second polling

        console.log('⏰ Polling started (30s interval)');
    }

    /**
     * Stop polling
     */
    stopPolling() {
        if (this.pollingInterval) {
            clearInterval(this.pollingInterval);
            this.pollingInterval = null;
            console.log('⏹️ Polling stopped');
        }
    }

    /**
     * Refresh data for current view
     */
    async refreshCurrentView() {
        const currentView = State.get('navigation.currentView');
        
        switch (currentView) {
            case 'dashboard':
                await this.loadDashboard();
                break;
            case 'browser':
                await this.loadBrowser();
                break;
            case 'rbac':
                await this.loadRBAC();
                break;
            case 'metrics':
                await this.loadMetrics();
                break;
        }
    }

    // ========== NAVIGATION ACTIONS ==========

    /**
     * Switch to different view
     * @param {string} viewName - View to switch to
     */
    async switchView(viewName) {
        const currentView = State.get('navigation.currentView');
        if (currentView === viewName) return;

        console.log(`🔄 Switching to view: ${viewName}`);
        
        State.update('navigation.previousView', currentView);
        State.update('navigation.currentView', viewName);
        
        // Load data for new view
        switch (viewName) {
            case 'dashboard':
                await this.loadDashboard();
                break;
            case 'browser':
                await this.loadBrowser();
                break;
            case 'rbac':
                await this.loadRBAC();
                break;
            case 'metrics':
                await this.loadMetrics();
                break;
            case 'operations':
                await this.loadOperations();
                break;
            case 'api':
                await this.loadAPI();
                break;
        }

        // Update URL
        window.location.hash = viewName;
    }

    // ========== AUTHENTICATION ACTIONS ==========

    /**
     * Login user
     * @param {string} username
     * @param {string} password
     * @returns {Promise<boolean>} Success status
     */
    async login(username, password) {
        console.log('🔐 Attempting login...');
        
        const authData = await DataService.authenticate(username, password);
        
        if (authData) {
            // Store token in localStorage for persistence
            localStorage.setItem('jdbx_auth_token', authData.token);
            
            // Update state
            State.update('auth.user', authData.user);
            State.update('auth.token', authData.token);
            State.update('auth.isAuthenticated', true);
            
            console.log('✅ Login successful, token stored');
            return true;
        } else {
            console.error('❌ Login failed');
            return false;
        }
    }

    /**
     * Logout user
     */
    logout() {
        console.log('🚪 Logging out...');
        
        // Clear state
        State.update('auth.user', null);
        State.update('auth.token', null);
        State.update('auth.isAuthenticated', false);
        
        // Clear storage
        localStorage.removeItem('jdbx_auth_token');
        
        // Stop polling
        this.stopPolling();
        
        // Redirect to login
        window.location.href = '/login.html';
    }

    // ========== DATA LOADING ACTIONS ==========

    /**
     * Load dashboard data
     */
    async loadDashboard() {
        console.log('📊 Loading dashboard data...');
        State.update('ui.loading', true);
        
        try {
            const dashboardData = await DataService.fetchDashboardData();
            
            if (dashboardData) {
                State.update('data.stats', dashboardData.stats || {});
                State.update('data.collections', dashboardData.collections || []);
                console.log('✅ Dashboard data loaded');
            } else {
                console.warn('⚠️ No dashboard data returned');
                State.update('data.stats', {});
                State.update('data.collections', []);
            }
        } catch (error) {
            console.error('❌ Error loading dashboard data:', error);
            // Don't let data loading errors affect authentication
            State.update('data.stats', {});
            State.update('data.collections', []);
        } finally {
            State.update('ui.loading', false);
        }
    }

    /**
     * Load browser data
     */
    async loadBrowser() {
        console.log('🗂️ Loading browser data...');
        State.update('ui.loading', true);
        
        const currentLibrary = State.get('ui.currentLibrary');
        
        const [collections, documents] = await Promise.all([
            DataService.fetchCollections(currentLibrary),
            DataService.fetchDocuments(currentLibrary)
        ]);
        
        State.update('data.collections', collections);
        State.update('data.documents', documents);
        State.update('ui.loading', false);
        
        console.log('✅ Browser data loaded');
    }

    /**
     * Load RBAC data
     */
    async loadRBAC() {
        console.log('👥 Loading RBAC data...');
        State.update('ui.loading', true);
        
        const currentLibrary = State.get('ui.currentLibrary');
        const rbacData = await DataService.fetchRBACData(currentLibrary);
        
        State.update('data.users', rbacData.users);
        State.update('data.roles', rbacData.roles);
        State.update('ui.loading', false);
        
        console.log('✅ RBAC data loaded');
    }

    /**
     * Load metrics data
     */
    async loadMetrics() {
        console.log('📈 Loading metrics data...');
        State.update('ui.loading', true);
        
        const currentLibrary = State.get('ui.currentLibrary');
        const metrics = await DataService.fetchMetrics(currentLibrary);
        
        State.update('data.metrics', metrics);
        State.update('ui.loading', false);
        
        console.log('✅ Metrics data loaded');
    }

    /**
     * Load operations data
     */
    async loadOperations() {
        console.log('⚙️ Loading operations data...');
        // Operations view - placeholder for now
        console.log('✅ Operations data loaded');
    }

    /**
     * Load API documentation
     */
    async loadAPI() {
        console.log('📚 Loading API documentation...');
        // API documentation - static content
        console.log('✅ API documentation loaded');
    }

    // ========== USER ACTIONS ==========

    /**
     * Switch library
     * @param {string} libraryName - Library to switch to
     */
    async switchLibrary(libraryName) {
        const currentLibrary = State.get('ui.currentLibrary');
        if (currentLibrary === libraryName) return;

        console.log(`📚 Switching to library: ${libraryName}`);
        
        State.update('ui.currentLibrary', libraryName);
        
        // Reload current view data
        await this.refreshCurrentView();
    }

    /**
     * Create document
     * @param {object} documentData - Document to create
     * @returns {Promise<boolean>} Success status
     */
    async createDocument(documentData) {
        console.log('📝 Creating document...');
        
        const currentLibrary = State.get('ui.currentLibrary');
        const result = await DataService.createDocument(documentData, currentLibrary);
        
        if (result) {
            // Reload browser data to show new document
            await this.loadBrowser();
            console.log('✅ Document created');
            return true;
        } else {
            console.error('❌ Document creation failed');
            return false;
        }
    }

    /**
     * Update document
     * @param {string} documentId - Document ID
     * @param {object} updates - Updates to apply
     * @returns {Promise<boolean>} Success status
     */
    async updateDocument(documentId, updates) {
        console.log(`📝 Updating document: ${documentId}`);
        
        const result = await DataService.updateDocument(documentId, updates);
        
        if (result) {
            // Reload browser data to show updated document
            await this.loadBrowser();
            console.log('✅ Document updated');
            return true;
        } else {
            console.error('❌ Document update failed');
            return false;
        }
    }

    /**
     * Delete document
     * @param {string} documentId - Document ID
     * @returns {Promise<boolean>} Success status
     */
    async deleteDocument(documentId) {
        console.log(`🗑️ Deleting document: ${documentId}`);
        
        const result = await DataService.deleteDocument(documentId);
        
        if (result) {
            // Reload browser data to hide deleted document
            await this.loadBrowser();
            console.log('✅ Document deleted');
            return true;
        } else {
            console.error('❌ Document deletion failed');
            return false;
        }
    }

    /**
     * Show notification
     * @param {string} type - Notification type (error, success, warning, info)
     * @param {string} message - Notification message
     */
    showNotification(type, message) {
        const notifications = State.get('ui.notifications');
        const newNotification = { type, message, id: Date.now() };
        
        State.update('ui.notifications', [...notifications, newNotification]);
        
        // Auto-remove after 5 seconds
        setTimeout(() => {
            const currentNotifications = State.get('ui.notifications');
            const filtered = currentNotifications.filter(n => n.id !== newNotification.id);
            State.update('ui.notifications', filtered);
        }, 5000);
    }
}

// Single controller instance
export const Controller = new ControllerClass();