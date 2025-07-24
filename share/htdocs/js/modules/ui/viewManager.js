/**
 * View Manager
 * Handles all view switching and rendering logic
 */

import { state } from '../core/state.js';
import { VIEW_TYPES } from '../core/constants.js';
import { UnifiedRenderer } from './unifiedRenderer.js';

export class ViewManager {
    constructor() {
        this.isInitialized = false;
        this.currentView = null;
        this.viewRenderers = new Map();
        this.renderer = new UnifiedRenderer();
    }

    /**
     * Initialize view manager
     */
    async initialize() {
        if (this.isInitialized) return;

        console.log('🖼️ Initializing View Manager');

        // Setup view renderers
        this.setupViewRenderers();

        // Setup navigation handling
        this.setupNavigation();

        // Load initial view
        const initialView = window.location.hash.substring(1) || VIEW_TYPES.DASHBOARD;
        await this.switchView(initialView);

        this.isInitialized = true;
    }

    /**
     * Setup view renderers
     */
    setupViewRenderers() {
        this.viewRenderers.set(VIEW_TYPES.DASHBOARD, this.renderDashboard.bind(this));
        this.viewRenderers.set(VIEW_TYPES.BROWSER, this.renderBrowser.bind(this));
        this.viewRenderers.set(VIEW_TYPES.RBAC, this.renderRBAC.bind(this));
        this.viewRenderers.set(VIEW_TYPES.METRICS, this.renderMetrics.bind(this));
        this.viewRenderers.set(VIEW_TYPES.OPERATIONS, this.renderOperations.bind(this));
        this.viewRenderers.set(VIEW_TYPES.SCRIPTS, this.renderScripts.bind(this));
        this.viewRenderers.set('api', this.renderAPI.bind(this));
    }

    /**
     * Setup navigation handling
     */
    setupNavigation() {
        // Handle browser back/forward
        window.addEventListener('hashchange', () => {
            const view = window.location.hash.substring(1) || VIEW_TYPES.DASHBOARD;
            this.switchView(view);
        });
    }

    /**
     * Switch to view
     * @param {string} viewName - View name
     */
    async switchView(viewName) {
        if (this.currentView === viewName) return;

        console.log(`🔄 Switching to view: ${viewName}`);

        try {
            // Update state
            state.setState('currentView', viewName);

            // Update navigation
            this.updateNavigation(viewName);

            // Hide all views
            this.hideAllViews();

            // Show target view
            this.showView(viewName);

            // Render view content
            await this.renderView(viewName);

            // Update URL
            window.location.hash = viewName;

            this.currentView = viewName;

        } catch (error) {
            console.error(`Failed to switch to view ${viewName}:`, error);
        }
    }

    /**
     * Update navigation active state
     * @param {string} viewName - View name
     */
    updateNavigation(viewName) {
        // Remove active class from all nav links
        document.querySelectorAll('.nav-pills .nav-link').forEach(link => {
            link.classList.remove('active');
        });

        // Add active class to current nav link
        const activeLink = document.querySelector(`a[href="#${viewName}"]`);
        if (activeLink) {
            activeLink.classList.add('active');
        }
    }

    /**
     * Hide all views
     */
    hideAllViews() {
        const views = document.querySelectorAll('.view-container');
        views.forEach(view => {
            view.classList.remove('active');
        });
    }

    /**
     * Show specific view
     * @param {string} viewName - View name
     */
    showView(viewName) {
        const viewElement = document.getElementById(`${viewName}-view`);
        console.log(`🔍 showView(${viewName}): element found:`, !!viewElement);
        if (viewElement) {
            viewElement.classList.add('active');
            console.log(`✅ Added 'active' class to ${viewName}-view`);
            console.log(`🔍 Element classes:`, viewElement.classList.toString());
            console.log(`🔍 Element style.display:`, viewElement.style.display);
            console.log(`🔍 Computed display:`, getComputedStyle(viewElement).display);
        } else {
            console.error(`❌ Element not found: ${viewName}-view`);
        }
    }

    /**
     * Render view content
     * @param {string} viewName - View name
     */
    async renderView(viewName) {
        const renderer = this.viewRenderers.get(viewName);
        if (renderer) {
            await renderer();
        } else {
            console.warn(`No renderer found for view: ${viewName}`);
        }
    }

    /**
     * Render dashboard view
     */
    async renderDashboard() {
        console.log('🎯 renderDashboard called');
        const dataManager = window.jdbxApp.getManager('data');
        const pollingManager = window.jdbxApp.getManager('polling');
        
        // Load dashboard data
        const { stats, collections } = await dataManager.loadDashboardData();
        console.log('📊 Dashboard data loaded:', { stats, collections });
        
        // Single render call - unified approach
        this.renderer.render({ stats, collections });
        
        // Start polling
        pollingManager.startPolling(VIEW_TYPES.DASHBOARD);
        console.log('✅ Dashboard rendering complete');
    }

    /**
     * Render browser view
     */
    async renderBrowser() {
        console.log('🗂️ renderBrowser called');
        const dataManager = window.jdbxApp.getManager('data');
        
        // Load all browser data
        const collections = await dataManager.loadCollections();
        const documents = await dataManager.loadDocuments();
        console.log('📚 Browser data loaded:', { collections: collections?.length, documents: documents?.length });
        
        // Single render call
        this.renderer.render({ collections, documents });
        console.log('✅ Browser rendering complete');
    }

    /**
     * Render RBAC view
     */
    async renderRBAC() {
        const dataManager = window.jdbxApp.getManager('data');
        const pollingManager = window.jdbxApp.getManager('polling');
        
        // Load RBAC data
        await dataManager.loadRBACData();
        
        // Update RBAC UI
        this.updateRBACUI();
        
        // Start polling
        pollingManager.startPolling(VIEW_TYPES.RBAC);
    }

    /**
     * Render metrics view
     */
    async renderMetrics() {
        const dataManager = window.jdbxApp.getManager('data');
        const pollingManager = window.jdbxApp.getManager('polling');
        
        // Load metrics data
        const metrics = await dataManager.loadMetricsData();
        
        // Update metrics UI
        this.updateMetricsUI(metrics);
        
        // Start polling
        pollingManager.startPolling(VIEW_TYPES.METRICS);
    }

    /**
     * Render operations view
     */
    async renderOperations() {
        const pollingManager = window.jdbxApp.getManager('polling');
        
        // Update operations UI
        this.updateOperationsUI();
        
        // Start polling
        pollingManager.startPolling(VIEW_TYPES.OPERATIONS);
    }

    /**
     * Render scripts view
     */
    async renderScripts() {
        // Update scripts UI
        this.updateScriptsUI();
        
        // No polling needed for scripts
    }

    /**
     * Update dashboard stats
     * @param {object} stats - Statistics data
     * @param {Array} collections - Collections data
     */
    updateDashboardStats(stats, collections) {
        console.log('📊 updateDashboardStats called with:', stats);
        
        // Update stats display
        const elements = {
            totalCollections: document.getElementById('statTotalCollections'),
            totalDocuments: document.getElementById('totalDocuments'),
            databaseSize: document.getElementById('statDatabaseSize')
        };

        console.log('🔍 DOM elements found:', {
            totalCollections: !!elements.totalCollections,
            totalDocuments: !!elements.totalDocuments,
            databaseSize: !!elements.databaseSize
        });

        if (elements.totalCollections) {
            // Get collections count from the collections data, not stats
            const collectionsCount = collections ? collections.length : (stats.totalCollections || 0);
            elements.totalCollections.textContent = collectionsCount;
            console.log('✅ Updated totalCollections to:', collectionsCount);
        }
        if (elements.totalDocuments) {
            elements.totalDocuments.textContent = stats.totalDocuments || 0;
            console.log('✅ Updated totalDocuments to:', stats.totalDocuments || 0);
        }
        if (elements.databaseSize) {
            elements.databaseSize.textContent = this.formatBytes(stats.databaseSize || 0);
            console.log('✅ Updated databaseSize to:', this.formatBytes(stats.databaseSize || 0));
        }
    }

    /**
     * Update dashboard charts
     * @param {Array} collections - Collections data
     */
    updateDashboardCharts(collections) {
        console.log('📈 updateDashboardCharts called with collections:', collections);
        
        // Update collections chart
        if (collections && collections.length > 0) {
            console.log('🔍 Processing collections for chart:', collections.map(c => ({ name: c.name, count: c.documentCount })));
            
            const chartData = {
                labels: collections.map(c => c.name || 'Unknown'),
                datasets: [{
                    label: 'Documents',
                    data: collections.map(c => c.documentCount || 0),
                    backgroundColor: [
                        'rgba(255, 99, 132, 0.6)',
                        'rgba(54, 162, 235, 0.6)',
                        'rgba(255, 205, 86, 0.6)',
                        'rgba(75, 192, 192, 0.6)',
                        'rgba(153, 102, 255, 0.6)'
                    ],
                    borderColor: [
                        'rgba(255, 99, 132, 1)',
                        'rgba(54, 162, 235, 1)',
                        'rgba(255, 205, 86, 1)',
                        'rgba(75, 192, 192, 1)',
                        'rgba(153, 102, 255, 1)'
                    ],
                    borderWidth: 1
                }]
            };

            console.log('📊 Chart data prepared:', chartData);

            // Create chart if it doesn't exist, otherwise update it
            try {
                window.updateChart('collectionsChart', chartData);
                console.log('✅ Chart updated successfully');
            } catch (error) {
                console.log('🔄 Creating collections chart...', error);
                window.createChart('collectionsChart', 'collectionsChart', 'doughnut', chartData);
                console.log('✅ Chart created successfully');
            }
        } else {
            console.warn('⚠️ No collections data available for chart');
        }
    }

    /**
     * Update browser UI
     */
    updateBrowserUI() {
        const documents = state.get('documents');
        const collections = state.get('collections');
        
        // Update collections list
        this.updateCollectionsList(collections);
        
        // Update documents list
        this.updateDocumentsList(documents);
    }

    /**
     * Update RBAC UI
     */
    updateRBACUI() {
        const users = state.get('allUsers');
        const roles = state.get('allRoles');
        
        // Update users list
        this.updateUsersList(users);
        
        // Update roles list
        this.updateRolesList(roles);
    }

    /**
     * Update metrics UI
     * @param {object} metrics - Metrics data
     */
    updateMetricsUI(metrics) {
        // Update metrics charts and displays
        if (metrics.performance) {
            this.updatePerformanceMetrics(metrics.performance);
        }
        if (metrics.system) {
            this.updateSystemMetrics(metrics.system);
        }
    }

    /**
     * Update operations UI
     */
    updateOperationsUI() {
        // Update operations status
        this.updateOperationsStatus();
    }

    /**
     * Update scripts UI
     */
    updateScriptsUI() {
        // Update scripts list
        this.updateScriptsList();
    }

    /**
     * Render API view
     */
    async renderAPI() {
        // Update API documentation UI
        console.log('Rendering API view');
        // API view is static, no dynamic content to load
    }

    /**
     * Helper methods for UI updates
     */
    updateCollectionsList(collections) {
        // Implementation for collections list update
    }

    updateDocumentsList(documents) {
        // Implementation for documents list update
    }

    updateUsersList(users) {
        // Implementation for users list update
    }

    updateRolesList(roles) {
        // Implementation for roles list update
    }

    updatePerformanceMetrics(performance) {
        // Implementation for performance metrics update
    }

    updateSystemMetrics(system) {
        // Implementation for system metrics update
    }

    updateOperationsStatus() {
        // Implementation for operations status update
    }

    updateScriptsList() {
        // Implementation for scripts list update
    }

    /**
     * Format bytes to human readable format
     * @param {number} bytes - Bytes
     * @returns {string} Formatted string
     */
    formatBytes(bytes) {
        if (bytes === 0) return '0 Bytes';
        
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    /**
     * Cleanup resources
     */
    cleanup() {
        this.viewRenderers.clear();
    }
}