/**
 * JDBX - Streamlined & Simple
 * Maximum functionality, minimum complexity
 */

// Global state (just what we need)
let currentView = 'dashboard';
let currentLibrary = 'default';

// Simple API call
async function api(endpoint) {
    const token = localStorage.getItem('jdbx_auth_token');
    if (!token && !endpoint.includes('login')) return null;
    
    const response = await fetch(endpoint, {
        headers: { 'Authorization': `Bearer ${token}`, 'Content-Type': 'application/json' }
    });
    return response.ok ? response.json() : null;
}

// Update any element by ID
function set(id, value) {
    const el = document.getElementById(id);
    if (el) el.textContent = value;
}

// Update any element's HTML by ID
function setHTML(id, html) {
    const el = document.getElementById(id);
    if (el) el.innerHTML = html;
}

// Show view and load its data
function showView(view) {
    // Hide all views
    document.querySelectorAll('.view-container').forEach(v => v.classList.remove('active'));
    document.querySelectorAll('.nav-link').forEach(n => n.classList.remove('active'));
    
    // Show target view
    const target = document.getElementById(`${view}-view`);
    const nav = document.querySelector(`a[href="#${view}"]`);
    if (target) target.classList.add('active');
    if (nav) nav.classList.add('active');
    
    currentView = view;
    loadData();
}

// Load data for current view
async function loadData() {
    if (currentView === 'dashboard') {
        const data = await api('/api/collections?library=default&stats=true');
        if (data?.collections) {
            const docs = data.collections.reduce((sum, col) => sum + (col.document_count || 0), 0);
            set('statTotalCollections', data.collections.length);
            set('totalDocuments', docs);
            set('statDatabaseSize', '0 Bytes');
            set('systemStatus', 'Active');
        }
        
        const metrics = await api('/api/metrics/stats?format=json&library=default');
        if (metrics?.metrics) {
            const requests = metrics.metrics.find(m => m.name === 'server_requests_total');
            const operations = metrics.metrics.find(m => m.name === 'db_operations_total');
            if (requests) set('totalRequests', requests.value || 0);
            if (operations) set('totalOps', operations.value || 0);
        }
    }
    
    else if (currentView === 'browser') {
        const [collections, documents] = await Promise.all([
            api(`/api/collections?library=${currentLibrary}`),
            api(`/api/documents?library=${currentLibrary}`)
        ]);
        
        if (collections?.collections) {
            setHTML('collectionsList', collections.collections.map(col => 
                `<div class="list-group-item d-flex justify-content-between">
                    <strong>${col.name}</strong>
                    <span class="badge bg-primary">${col.document_count || 0}</span>
                </div>`
            ).join(''));
        }
        
        if (documents) {
            const docs = Array.isArray(documents) ? documents : (documents.documents || []);
            setHTML('documentsList', docs.map(doc => 
                `<div class="list-group-item">
                    <h6>${doc.type || 'Document'}</h6>
                    <small>${doc.uuid || doc.id}</small>
                </div>`
            ).join(''));
        }
    }
    
    else if (currentView === 'metrics') {
        const data = await api('/api/metrics/stats?format=json&library=default&range=1h');
        if (data?.metrics) {
            data.metrics.forEach(metric => {
                if (metric.name === 'db_operations_total') set('totalOps', metric.value || 0);
                if (metric.name === 'server_requests_total') set('totalRequests', metric.value || 0);
            });
        }
    }
}

// Switch library
async function switchLibrary(lib) {
    currentLibrary = lib;
    const selector = document.getElementById('globalLibrarySelector');
    if (selector) selector.value = lib;
    loadData();
}

// Authentication check
function checkAuth() {
    if (!localStorage.getItem('jdbx_auth_token') && !location.pathname.includes('login')) {
        location.href = '/login.html';
        return false;
    }
    return true;
}

// Initialize app
function init() {
    if (!checkAuth()) return;
    
    // Show page
    document.documentElement.classList.add('loaded');
    
    // Set up globals
    window.showView = showView;
    window.switchView = showView;
    window.loadDashboard = () => loadData();
    window.switchLibrary = switchLibrary;
    window.logout = () => { localStorage.removeItem('jdbx_auth_token'); location.href = '/login.html'; };
    
    // Set up library selector
    const selector = document.getElementById('globalLibrarySelector');
    if (selector) {
        selector.onchange = (e) => switchLibrary(e.target.value);
        api('/api/libraries').then(data => {
            if (data?.libraries) {
                setHTML('globalLibrarySelector', data.libraries.map(lib => 
                    `<option value="${lib.name}">${lib.name}</option>`
                ).join(''));
                selector.value = currentLibrary;
            }
        });
    }
    
    // Start with dashboard
    showView('dashboard');
    
    // Auto-refresh every 30 seconds
    setInterval(loadData, 30000);
}

// Start when ready
document.readyState === 'loading' ? document.addEventListener('DOMContentLoaded', init) : init();