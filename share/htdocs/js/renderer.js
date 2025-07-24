/**
 * JDBX Renderer - Single UI Update Point
 * All DOM updates flow through this renderer
 */

class RendererClass {
    constructor() {
        this.charts = new Map();
    }

    /**
     * Render complete UI based on state
     * @param {object} state - Complete application state
     */
    render(state) {
        console.log('🎨 Renderer.render() called');
        
        this.renderAuthentication(state);
        this.renderNavigation(state);
        this.renderCurrentView(state);
        this.renderNotifications(state);
        
        console.log('✅ Render complete');
    }

    /**
     * Render authentication state
     */
    renderAuthentication(state) {
        console.log('🔐 renderAuthentication called:', {
            isAuthenticated: state.auth.isAuthenticated,
            hasStateToken: !!state.auth.token,
            pathname: window.location.pathname
        });
        
        if (!state.auth.isAuthenticated) {
            // Only redirect if we don't have a token (true logout)
            // Use State as single source of truth for token
            if (!state.auth.token && window.location.pathname !== '/login.html') {
                console.log('🚪 No token found, redirecting to login');
                window.location.href = '/login.html';
            }
            return;
        }

        // Update library selector
        this.renderLibrarySelector(state);
        console.log('✅ Authentication render complete');
    }

    /**
     * Render navigation
     */
    renderNavigation(state) {
        // Update active navigation pill
        document.querySelectorAll('.nav-pills .nav-link').forEach(link => {
            link.classList.remove('active');
        });

        const activeLink = document.querySelector(`a[href="#${state.navigation.currentView}"]`);
        if (activeLink) {
            activeLink.classList.add('active');
        }

        // Show current view
        this.showView(state.navigation.currentView);
    }

    /**
     * Show specific view, hide others
     */
    showView(viewName) {
        // Hide all views
        document.querySelectorAll('.view-container').forEach(view => {
            view.classList.remove('active');
        });

        // Show target view
        const viewElement = document.getElementById(`${viewName}-view`);
        if (viewElement) {
            viewElement.classList.add('active');
            console.log(`✅ Showing view: ${viewName}`);
        } else {
            console.error(`❌ View not found: ${viewName}-view`);
        }
    }

    /**
     * Render current view content
     */
    renderCurrentView(state) {
        switch (state.navigation.currentView) {
            case 'dashboard':
                this.renderDashboard(state);
                break;
            case 'browser':
                this.renderBrowser(state);
                break;
            case 'rbac':
                this.renderRBAC(state);
                break;
            case 'metrics':
                this.renderMetrics(state);
                break;
            case 'operations':
                this.renderOperations(state);
                break;
            case 'api':
                this.renderAPI(state);
                break;
        }
    }

    /**
     * Render dashboard view
     */
    renderDashboard(state) {
        const { stats, collections } = state.data;

        // Update stats
        this.updateElement('statTotalCollections', collections.length);
        this.updateElement('totalDocuments', stats.totalDocuments || 0);
        this.updateElement('statDatabaseSize', this.formatBytes(stats.databaseSize || 0));
        this.updateElement('systemStatus', 'Active');

        // Render collections chart
        this.renderCollectionsChart(collections);

        console.log('✅ Dashboard rendered');
    }

    /**
     * Render browser view
     */
    renderBrowser(state) {
        const { collections, documents } = state.data;

        // Render collections sidebar
        this.renderCollectionsList(collections);

        // Render documents list
        this.renderDocumentsList(documents);

        console.log('✅ Browser rendered');
    }

    /**
     * Render RBAC view
     */
    renderRBAC(state) {
        const { users, roles } = state.data;

        // Render users list
        this.renderUsersList(users);

        // Render roles list
        this.renderRolesList(roles);

        console.log('✅ RBAC rendered');
    }

    /**
     * Render metrics view
     */
    renderMetrics(state) {
        const { metrics } = state.data;

        // Render metrics display
        this.renderMetricsDisplay(metrics);

        console.log('✅ Metrics rendered');
    }

    /**
     * Render operations view
     */
    renderOperations(state) {
        console.log('✅ Operations rendered');
    }

    /**
     * Render API documentation view
     */
    renderAPI(state) {
        console.log('✅ API documentation rendered');
    }

    /**
     * Render library selector
     */
    renderLibrarySelector(state) {
        const selector = document.getElementById('globalLibrarySelector');
        if (!selector) return;

        const { libraries } = state.data;
        const currentLibrary = state.ui.currentLibrary;

        selector.innerHTML = '';
        libraries.forEach(lib => {
            const option = document.createElement('option');
            option.value = lib.name;
            option.textContent = lib.displayName || lib.name;
            option.selected = lib.name === currentLibrary;
            selector.appendChild(option);
        });

        console.log(`✅ Library selector rendered: ${libraries.length} libraries`);
    }

    /**
     * Render collections chart
     */
    renderCollectionsChart(collections) {
        const canvas = document.getElementById('collectionsChart');
        if (!canvas || collections.length === 0) return;

        // Destroy existing chart
        if (this.charts.has('collectionsChart')) {
            this.charts.get('collectionsChart').destroy();
        }

        // Create new chart
        const chart = new Chart(canvas, {
            type: 'doughnut',
            data: {
                labels: collections.map(c => c.name || 'Unknown'),
                datasets: [{
                    label: 'Documents per Collection',
                    data: collections.map(c => c.documentCount || 0),
                    backgroundColor: [
                        '#FF6384', '#36A2EB', '#FFCE56', '#4BC0C0', '#9966FF'
                    ].slice(0, collections.length),
                    borderWidth: 0
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: { position: 'bottom' }
                }
            }
        });

        this.charts.set('collectionsChart', chart);
        console.log(`✅ Collections chart rendered: ${collections.length} collections`);
    }

    /**
     * Render collections list for browser
     */
    renderCollectionsList(collections) {
        const container = document.querySelector('.collections-sidebar .panel-content');
        if (!container) return;

        container.innerHTML = '';

        if (collections.length === 0) {
            container.innerHTML = '<p class="text-muted p-3">No collections found</p>';
            return;
        }

        collections.forEach(collection => {
            const item = document.createElement('div');
            item.className = 'collection-item';
            item.innerHTML = `
                <div class="collection-name">${collection.name || 'Unknown'}</div>
                <div class="collection-count">${collection.documentCount || 0} docs</div>
            `;
            container.appendChild(item);
        });

        console.log(`✅ Collections list rendered: ${collections.length} collections`);
    }

    /**
     * Render documents list
     */
    renderDocumentsList(documents) {
        const container = document.querySelector('.documents-panel .panel-content');
        if (!container) return;

        container.innerHTML = '';

        if (documents.length === 0) {
            container.innerHTML = '<p class="text-muted p-3">No documents found</p>';
            return;
        }

        documents.forEach(doc => {
            const item = document.createElement('div');
            item.className = 'document-item';
            item.innerHTML = `
                <div class="document-name">${doc.type || 'Document'}</div>
                <div class="document-id">${doc.id}</div>
            `;
            container.appendChild(item);
        });

        console.log(`✅ Documents list rendered: ${documents.length} documents`);
    }

    /**
     * Render users list
     */
    renderUsersList(users) {
        // Implementation for RBAC users list
        console.log(`✅ Users list rendered: ${users.length} users`);
    }

    /**
     * Render roles list
     */
    renderRolesList(roles) {
        // Implementation for RBAC roles list
        console.log(`✅ Roles list rendered: ${roles.length} roles`);
    }

    /**
     * Render metrics display
     */
    renderMetricsDisplay(metrics) {
        // Implementation for metrics display
        console.log(`✅ Metrics display rendered: ${metrics.length} metrics`);
    }

    /**
     * Render notifications
     */
    renderNotifications(state) {
        const { notifications } = state.ui;
        
        // Simple notification display - can be enhanced
        notifications.forEach(notification => {
            console.log(`📢 Notification: ${notification.type} - ${notification.message}`);
        });
    }

    /**
     * Update DOM element text content
     */
    updateElement(id, value) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = value;
            console.log(`✅ Updated ${id}: ${value}`);
        } else {
            console.warn(`❌ Element not found: ${id}`);
        }
    }

    /**
     * Format bytes to human readable
     */
    formatBytes(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }
}

// Single renderer instance
export const Renderer = new RendererClass();