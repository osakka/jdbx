/**
 * Unified UI Renderer - Single Source of Truth
 * All UI updates flow through this single renderer
 */

export class UnifiedRenderer {
    constructor() {
        this.currentData = {};
    }

    /**
     * Update all UI with provided data
     * @param {object} data - Complete data object
     */
    render(data) {
        console.log('🎯 UnifiedRenderer.render() called with:', data);
        
        // Store current data
        this.currentData = { ...this.currentData, ...data };
        
        // Render all components with single data source
        this.renderStats(this.currentData);
        this.renderCollections(this.currentData);
        this.renderCharts(this.currentData);
        this.renderLibrarySelector(this.currentData);
        
        console.log('✅ All UI components rendered');
    }

    /**
     * Render stats section
     */
    renderStats(data) {
        const stats = data.stats || {};
        const collections = data.collections || [];
        
        // Single DOM update block
        const updates = [
            { id: 'statTotalCollections', value: collections.length },
            { id: 'totalDocuments', value: stats.totalDocuments || 0 },
            { id: 'statDatabaseSize', value: this.formatBytes(stats.databaseSize || 0) },
            { id: 'systemStatus', value: 'Active' }
        ];
        
        updates.forEach(({ id, value }) => {
            const element = document.getElementById(id);
            if (element) {
                element.textContent = value;
                console.log(`✅ Updated ${id}: ${value}`);
            } else {
                console.warn(`❌ Element not found: ${id}`);
            }
        });
    }

    /**
     * Render collections (for browser view)
     */
    renderCollections(data) {
        const collections = data.collections || [];
        
        // Find collections container
        const container = document.querySelector('.collections-sidebar .panel-content') || 
                         document.getElementById('collectionsContainer');
        
        if (!container) {
            console.warn('❌ Collections container not found');
            return;
        }
        
        // Clear and populate
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
        
        console.log(`✅ Rendered ${collections.length} collections`);
    }

    /**
     * Render charts
     */
    renderCharts(data) {
        const collections = data.collections || [];
        
        if (collections.length === 0) {
            console.warn('⚠️ No collections data for chart');
            return;
        }
        
        // Prepare chart data
        const chartData = {
            labels: collections.map(c => c.name || 'Unknown'),
            datasets: [{
                label: 'Documents per Collection',
                data: collections.map(c => c.documentCount || 0),
                backgroundColor: [
                    '#FF6384', '#36A2EB', '#FFCE56', '#4BC0C0', '#9966FF'
                ].slice(0, collections.length),
                borderWidth: 0
            }]
        };
        
        // Create/update chart
        const canvas = document.getElementById('collectionsChart');
        if (!canvas) {
            console.warn('❌ Chart canvas not found');
            return;
        }
        
        // Destroy existing chart if it exists
        if (canvas.chart) {
            canvas.chart.destroy();
        }
        
        // Create new chart
        canvas.chart = new Chart(canvas, {
            type: 'doughnut',
            data: chartData,
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'bottom'
                    }
                }
            }
        });
        
        console.log('✅ Chart rendered with', collections.length, 'collections');
    }

    /**
     * Render library selector
     */
    renderLibrarySelector(data) {
        const selector = document.getElementById('globalLibrarySelector');
        if (!selector) {
            console.warn('❌ Library selector not found');
            return;
        }
        
        // Default libraries (can be enhanced to load from API)
        const libraries = data.libraries || [
            { name: 'default', displayName: 'Default Library' },
            { name: 'system', displayName: 'System Library' }
        ];
        
        // Clear and populate
        selector.innerHTML = '';
        libraries.forEach(lib => {
            const option = document.createElement('option');
            option.value = lib.name;
            option.textContent = lib.displayName || lib.name;
            if (lib.name === 'default') option.selected = true;
            selector.appendChild(option);
        });
        
        console.log(`✅ Rendered ${libraries.length} libraries in selector`);
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