// JSON Database Admin Interface

// Configuration
// Use current host (dynamically determined at runtime)
const API_BASE_URL = window.location.protocol + '//' + window.location.host;
const AUTH_TOKEN_KEY = 'jsondb_auth_token';
const DEFAULT_CREDENTIALS = {
    username: 'admin',
    password: 'admin'
};

// Enable debugging messages
const DEBUG = true;

// Helper function for logging when debug is enabled
function debugLog(...args) {
    if (DEBUG) {
        console.log('[JsonDB Debug]', ...args);
    }
}

// Log initial configuration
debugLog('API Base URL:', API_BASE_URL);

// Application State
let currentView = 'dashboard';
let authToken = localStorage.getItem(AUTH_TOKEN_KEY);
let currentCollection = null;
let jsonEditor = null;
let charts = {};

// Transaction visualization state
let transactionHistory = [];
let transactionMetrics = {};
let transactionRelationships = {};
let activeTransactionChart = null;
let activeMetricsChart = null;
let transactionVisType = 'timeline';

// DOM Elements
const viewContainer = document.getElementById('view-container');
const pageTitle = document.getElementById('page-title');
const actionBtn = document.getElementById('action-btn');
const actionBtnText = document.getElementById('action-btn-text');
const loginModal = new bootstrap.Modal(document.getElementById('login-modal'));
const actionModal = document.getElementById('actionModal');
const confirmationModal = document.getElementById('confirmationModal');
const documentViewModal = document.getElementById('documentViewModal');

// Initialize Application
document.addEventListener('DOMContentLoaded', function() {
    // Check Authentication
    if (!authToken) {
        showLoginModal();
    } else {
        // Verify token is valid
        validateToken()
            .then(valid => {
                if (!valid) {
                    showLoginModal();
                } else {
                    initializeApp();
                }
            })
            .catch(() => showLoginModal());
    }

    // Set up event listeners
    setupEventListeners();
});

// Authentication Functions
function showLoginModal() {
    // Reset form
    document.getElementById('login-form').reset();
    document.getElementById('login-error').classList.add('d-none');
    
    // Set default username in development mode
    if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') {
        document.getElementById('username').value = DEFAULT_CREDENTIALS.username;
        document.getElementById('password').value = DEFAULT_CREDENTIALS.password;
    }
    
    // Show modal
    loginModal.show();
}

function validateToken() {
    // Since we don't have a dedicated validation endpoint, we can use a simpler approach
    // Just check if the token exists and hasn't expired
    if (!authToken) {
        debugLog('No token found in localStorage');
        return Promise.resolve(false);
    }

    debugLog('Token exists, checking validity...');

    // In debug mode, always allow token for easier development
    if (DEBUG) {
        debugLog('DEBUG mode: Accepting token without validation');
        return Promise.resolve(true);
    }

    // Test by making a protected API call
    return fetch(`${API_BASE_URL}/api/admin/test`, {
        method: 'GET',
        headers: {
            'Authorization': `Bearer ${authToken}`,
            'Content-Type': 'application/json'
        }
    })
    .then(response => {
        const isValid = response.ok;
        debugLog('Token validation result:', isValid);
        return isValid;
    })
    .catch(error => {
        debugLog('Token validation error:', error);
        return false;
    });
}

function handleLogin(event) {
    event.preventDefault();

    const username = document.getElementById('username').value;
    const password = document.getElementById('password').value;
    const errorEl = document.getElementById('login-error');

    // Clear previous errors
    errorEl.classList.add('d-none');

    debugLog(`Attempting to login with username: ${username} to ${API_BASE_URL}/api/admin/login`);

    // First, try a test request to verify the server is responding
    fetch(`${API_BASE_URL}/api/admin/test`, {
        method: 'GET',
        headers: {
            'Content-Type': 'application/json'
        }
    })
    .then(response => {
        debugLog('Test endpoint response status:', response.status);

        if (!response.ok) {
            debugLog('Test endpoint error, proceeding with login attempt anyway');
        } else {
            return response.json().then(data => {
                debugLog('Test endpoint response:', data);
            });
        }
    })
    .catch(error => {
        debugLog('Test endpoint error:', error);
    })
    .finally(() => {
        // Proceed with login regardless of test result
        // Call login API using the correct endpoint
        debugLog('Proceeding with login request');

        fetch(`${API_BASE_URL}/api/admin/login`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ username, password })
        })
        .then(response => {
            debugLog('Login response status:', response.status);
            debugLog('Login response headers:', [...response.headers.entries()]);

            if (!response.ok) {
                throw new Error('Invalid credentials');
            }
            return response.json();
        })
        .then(data => {
            debugLog('Login successful, response data:', data);

            // Store token (if available in the response)
            authToken = data.token || 'mock-token';
            localStorage.setItem(AUTH_TOKEN_KEY, authToken);

            // Hide login modal
            loginModal.hide();

            // Initialize app
            initializeApp();
        })
        .catch(error => {
            console.error('Login error:', error);
            // Show error
            errorEl.textContent = error.message;
            errorEl.classList.remove('d-none');
        });
    });
}

function handleLogout() {
    // Clear token
    authToken = null;
    localStorage.removeItem(AUTH_TOKEN_KEY);
    
    // Show login modal
    showLoginModal();
}

// Application Initialization
function initializeApp() {
    // Load initial view
    loadView('dashboard');
    
    // Initialize JSON editor
    initializeJsonEditor();
    
    // Load data for dashboard
    loadDashboardData();
}

function setupEventListeners() {
    // Navigation
    document.querySelectorAll('.nav-link[data-view]').forEach(link => {
        link.addEventListener('click', function(event) {
            event.preventDefault();
            loadView(this.dataset.view);
        });
    });
    
    // Login form
    document.getElementById('login-form').addEventListener('submit', handleLogin);
    
    // Logout button
    document.getElementById('logout-btn').addEventListener('click', handleLogout);
    
    // Refresh button
    document.getElementById('refresh-btn').addEventListener('click', refreshCurrentView);
    
    // Collection selector for documents view
    document.getElementById('collection-selector').addEventListener('change', function() {
        currentCollection = this.value;
        if (currentCollection) {
            loadDocuments(currentCollection);
        } else {
            document.getElementById('documents-table-body').innerHTML = '<tr><td colspan="5" class="text-center">Select a collection</td></tr>';
        }
    });
    
    // Search functionality
    document.getElementById('search-btn').addEventListener('click', function() {
        if (currentCollection) {
            const query = document.getElementById('document-search').value;
            loadDocuments(currentCollection, query);
        }
    });
    
    // Document search on Enter key
    document.getElementById('document-search').addEventListener('keypress', function(e) {
        if (e.key === 'Enter') {
            document.getElementById('search-btn').click();
        }
    });
    
    // Settings form submissions
    document.querySelectorAll('#settings-view form').forEach(form => {
        form.addEventListener('submit', function(event) {
            event.preventDefault();
            const formData = new FormData(this);
            const formId = this.id;
            
            saveSettings(formId, formData)
                .then(() => showToast('Settings saved successfully'))
                .catch(error => showToast('Error saving settings: ' + error.message, 'error'));
        });
    });
    
    // Create backup button
    document.getElementById('create-backup-btn').addEventListener('click', createBackup);
    
    // Generate JWT secret button
    document.getElementById('generate-secret-btn').addEventListener('click', generateJwtSecret);
    
    // Save document button in document view modal
    document.getElementById('save-document-btn').addEventListener('click', saveDocument);
    
    // Action button (context-dependent)
    actionBtn.addEventListener('click', handleActionButton);
    
    // Action confirmation
    document.getElementById('confirm-action-btn').addEventListener('click', executeConfirmedAction);
}

// View Management
function loadView(viewName) {
    // Update current view
    currentView = viewName;
    
    // Hide all views
    document.querySelectorAll('.view-content').forEach(view => {
        view.classList.remove('active-view');
    });
    
    // Show selected view
    document.getElementById(`${viewName}-view`).classList.add('active-view');
    
    // Update page title
    pageTitle.textContent = viewName.charAt(0).toUpperCase() + viewName.slice(1);
    
    // Update active navigation item
    document.querySelectorAll('.nav-link').forEach(link => {
        link.classList.remove('active');
    });
    document.querySelector(`.nav-link[data-view="${viewName}"]`).classList.add('active');
    
    // Update action button based on view
    updateActionButton(viewName);
    
    // Load view-specific data
    loadViewData(viewName);
}

function updateActionButton(viewName) {
    // Set action button text and visibility based on current view
    switch (viewName) {
        case 'collections':
            actionBtnText.textContent = 'New Collection';
            actionBtn.classList.remove('d-none');
            break;
        case 'documents':
            actionBtnText.textContent = 'New Document';
            actionBtn.classList.remove('d-none');
            break;
        case 'users':
            actionBtnText.textContent = 'New User';
            actionBtn.classList.remove('d-none');
            break;
        case 'roles':
            actionBtnText.textContent = 'New Role';
            actionBtn.classList.remove('d-none');
            break;
        default:
            actionBtn.classList.add('d-none');
            break;
    }
}

function loadViewData(viewName) {
    // Load data based on current view
    switch (viewName) {
        case 'dashboard':
            loadDashboardData();
            break;
        case 'collections':
            loadCollections();
            break;
        case 'documents':
            loadCollectionSelector();
            if (currentCollection) {
                loadDocuments(currentCollection);
            }
            break;
        case 'users':
            loadUsers();
            break;
        case 'roles':
            loadRoles();
            break;
        case 'metrics':
            loadMetrics();
            break;
        case 'settings':
            loadSettings();
            break;
        case 'transactions':
            loadTransactionVisualization();
            break;
    }
}

function refreshCurrentView() {
    loadViewData(currentView);
}

// Data Loading Functions
function loadDashboardData() {
    // Load dashboard statistics
    fetch(`${API_BASE_URL}/api/metrics`, {
        headers: { 'Authorization': `Bearer ${authToken}` }
    })
    .then(response => response.json())
    .then(data => {
        document.getElementById('collections-count').textContent = data.collections_count || 0;
        document.getElementById('documents-count').textContent = data.documents_count || 0;
        document.getElementById('users-count').textContent = data.users_count || 0;
        document.getElementById('connections-count').textContent = data.active_connections || 0;
        
        // Update charts
        updateHealthChart(data);
        
        // Also use the metrics data for activity since it's all in one endpoint
        loadActivityData(data);
    })
    .catch(error => console.error('Error loading dashboard data:', error));
}

// Function to load activity data from the metrics response
function loadActivityData(data) {
    const activityTable = document.getElementById('activity-table-body');
    if (!activityTable) return;
    
    activityTable.innerHTML = '';
    
    if (data.activities && data.activities.length > 0) {
        data.activities.forEach(activity => {
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${formatDateTime(activity.timestamp)}</td>
                <td>${activity.action}</td>
                <td>${activity.user}</td>
            `;
            activityTable.appendChild(row);
        });
    } else {
        activityTable.innerHTML = '<tr><td colspan="3" class="text-center">No recent activity</td></tr>';
    }
}

function loadCollections() {
    fetch(`${API_BASE_URL}/api/collections`, {
        headers: { 'Authorization': `Bearer ${authToken}` }
    })
    .then(response => response.json())
    .then(data => {
        const tableBody = document.getElementById('collections-table-body');
        tableBody.innerHTML = '';
        
        if (data.collections && data.collections.length > 0) {
            data.collections.forEach(collection => {
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${collection.name}</td>
                    <td>${collection.documents_count}</td>
                    <td>${formatSize(collection.size_bytes)}</td>
                    <td>${formatDateTime(collection.last_modified)}</td>
                    <td class="actions-column">
                        <button class="btn btn-sm btn-outline-primary view-documents-btn" data-collection="${collection.name}">
                            <i class="bi bi-eye"></i>
                        </button>
                        <button class="btn btn-sm btn-outline-danger delete-collection-btn" data-collection="${collection.name}">
                            <i class="bi bi-trash"></i>
                        </button>
                    </td>
                `;
                tableBody.appendChild(row);
                
                // Add event listeners
                row.querySelector('.view-documents-btn').addEventListener('click', function() {
                    currentCollection = this.dataset.collection;
                    loadView('documents');
                    // Pre-select the collection
                    document.getElementById('collection-selector').value = currentCollection;
                });
                
                row.querySelector('.delete-collection-btn').addEventListener('click', function() {
                    showConfirmationModal(
                        `Are you sure you want to delete the collection "${this.dataset.collection}"?`,
                        () => deleteCollection(this.dataset.collection)
                    );
                });
            });
        } else {
            tableBody.innerHTML = '<tr><td colspan="5" class="text-center">No collections found</td></tr>';
        }
    })
    .catch(error => console.error('Error loading collections:', error));
}

function loadCollectionSelector() {
    fetch(`${API_BASE_URL}/api/collections`, {
        headers: { 'Authorization': `Bearer ${authToken}` }
    })
    .then(response => response.json())
    .then(data => {
        const selector = document.getElementById('collection-selector');
        
        // Keep only the first option
        while (selector.options.length > 1) {
            selector.remove(1);
        }
        
        if (data.collections && data.collections.length > 0) {
            data.collections.forEach(collection => {
                const option = document.createElement('option');
                option.value = collection.name;
                option.textContent = collection.name;
                selector.appendChild(option);
            });
            
            // Set current collection if available
            if (currentCollection) {
                selector.value = currentCollection;
            }
        }
    })
    .catch(error => console.error('Error loading collections for selector:', error));
}

function loadDocuments(collection, query = '') {
    // The API supports both ?query= parameter and full JSON query structure
    // For now, implement the simpler query parameter approach
    const url = query 
        ? `${API_BASE_URL}/api/collections/${collection}/documents?query=${encodeURIComponent(query)}`
        : `${API_BASE_URL}/api/collections/${collection}/documents`;
        
    // Add a timestamp to prevent caching issues
    const finalUrl = `${url}${url.includes('?') ? '&' : '?'}_t=${Date.now()}`;
    
    // Add debug logging
    debugLog(`Loading documents from: ${finalUrl}`);
    
    // Set a timeout to avoid hanging indefinitely
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 10000);
    
    fetch(finalUrl, {
        headers: { 'Authorization': `Bearer ${authToken}` },
        signal: controller.signal
    })
    .catch(error => {
        // Clear the timeout 
        clearTimeout(timeoutId);
        
        if (error.name === 'AbortError') {
            debugLog('Request timed out after 10 seconds');
            return Promise.reject(new Error('Request timed out after 10 seconds'));
        }
        return Promise.reject(error);
    })
    .then(response => {
        // Clear the timeout on response
        clearTimeout(timeoutId);
        
        if (!response.ok) {
            debugLog(`Error response: ${response.status} ${response.statusText}`);
            return response.json().then(errData => {
                throw new Error(errData.error || 'Failed to load documents');
            });
        }
        return response.json();
    })
    .then(data => {
        debugLog('Documents response:', data);
        
        const tableBody = document.getElementById('documents-table-body');
        tableBody.innerHTML = '';
        
        // Check both response formats - some endpoints return documents directly, 
        // others return them in a "documents" property
        const documents = data.documents || (Array.isArray(data) ? data : []);
        
        if (documents && documents.length > 0) {
            documents.forEach(document => {
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${document._id}</td>
                    <td class="text-truncate-2">${formatJsonPreview(document)}</td>
                    <td>${formatDateTime(document._created || '')}</td>
                    <td>${formatDateTime(document._updated || '')}</td>
                    <td class="actions-column">
                        <button class="btn btn-sm btn-outline-primary view-document-btn" data-id="${document._id}">
                            <i class="bi bi-pencil-square"></i>
                        </button>
                        <button class="btn btn-sm btn-outline-danger delete-document-btn" data-id="${document._id}">
                            <i class="bi bi-trash"></i>
                        </button>
                    </td>
                `;
                tableBody.appendChild(row);
                
                // Add event listeners
                row.querySelector('.view-document-btn').addEventListener('click', function() {
                    loadDocument(collection, this.dataset.id);
                });
                
                row.querySelector('.delete-document-btn').addEventListener('click', function() {
                    showConfirmationModal(
                        `Are you sure you want to delete document with ID "${this.dataset.id}"?`,
                        () => deleteDocument(collection, this.dataset.id)
                    );
                });
            });
            
            // Update pagination info
            document.getElementById('documents-pagination-info').textContent = 
                `Showing ${documents.length} of ${data.total || documents.length} documents`;
        } else {
            tableBody.innerHTML = '<tr><td colspan="5" class="text-center">No documents found</td></tr>';
            document.getElementById('documents-pagination-info').textContent = 'Showing 0 of 0 documents';
        }
        
        // Update pagination controls (simplified for now)
        updatePagination(data.total || 0, data.page || 1, data.limit || 20);
    })
    .catch(error => console.error(`Error loading documents for ${collection}:`, error));
}

function loadDocument(collection, id) {
    fetch(`${API_BASE_URL}/api/collections/${collection}/documents/${id}`, {
        headers: { 'Authorization': `Bearer ${authToken}` }
    })
    .then(response => response.json())
    .then(document => {
        // Set document in JSON editor
        jsonEditor.set(document);
        
        // Update modal title
        document.getElementById('documentViewModalLabel').textContent = `Edit Document (ID: ${id})`;
        
        // Store collection and id for saving
        jsonEditor.collection = collection;
        jsonEditor.documentId = id;
        
        // Show modal
        new bootstrap.Modal(documentViewModal).show();
    })
    .catch(error => console.error(`Error loading document ${id}:`, error));
}

function loadUsers() {
    fetch(`${API_BASE_URL}/api/users`, {
        headers: { 'Authorization': `Bearer ${authToken}` }
    })
    .then(response => response.json())
    .then(data => {
        const tableBody = document.getElementById('users-table-body');
        tableBody.innerHTML = '';
        
        if (data.users && data.users.length > 0) {
            data.users.forEach(user => {
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${user.id}</td>
                    <td>${user.username}</td>
                    <td>${formatRoles(user.roles)}</td>
                    <td>${formatDateTime(user.last_login || '')}</td>
                    <td class="actions-column">
                        <button class="btn btn-sm btn-outline-primary edit-user-btn" data-id="${user.id}">
                            <i class="bi bi-pencil"></i>
                        </button>
                        <button class="btn btn-sm btn-outline-danger delete-user-btn" data-id="${user.id}">
                            <i class="bi bi-trash"></i>
                        </button>
                    </td>
                `;
                tableBody.appendChild(row);
                
                // Add event listeners
                row.querySelector('.edit-user-btn').addEventListener('click', function() {
                    showEditUserModal(user);
                });
                
                row.querySelector('.delete-user-btn').addEventListener('click', function() {
                    showConfirmationModal(
                        `Are you sure you want to delete user "${user.username}"?`,
                        () => deleteUser(user.id)
                    );
                });
            });
        } else {
            tableBody.innerHTML = '<tr><td colspan="5" class="text-center">No users found</td></tr>';
        }
    })
    .catch(error => console.error('Error loading users:', error));
}

function loadRoles() {
    fetch(`${API_BASE_URL}/api/roles`, {
        headers: { 'Authorization': `Bearer ${authToken}` }
    })
    .then(response => response.json())
    .then(data => {
        const tableBody = document.getElementById('roles-table-body');
        tableBody.innerHTML = '';
        
        if (data.roles && data.roles.length > 0) {
            data.roles.forEach(role => {
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${role.id}</td>
                    <td>${role.name}</td>
                    <td>${role.users_count || 0}</td>
                    <td>${formatPermissions(role.permissions)}</td>
                    <td class="actions-column">
                        <button class="btn btn-sm btn-outline-primary edit-role-btn" data-id="${role.id}">
                            <i class="bi bi-pencil"></i>
                        </button>
                        <button class="btn btn-sm btn-outline-danger delete-role-btn" data-id="${role.id}">
                            <i class="bi bi-trash"></i>
                        </button>
                    </td>
                `;
                tableBody.appendChild(row);
                
                // Add event listeners
                row.querySelector('.edit-role-btn').addEventListener('click', function() {
                    showEditRoleModal(role);
                });
                
                row.querySelector('.delete-role-btn').addEventListener('click', function() {
                    showConfirmationModal(
                        `Are you sure you want to delete role "${role.name}"?`,
                        () => deleteRole(role.id)
                    );
                });
            });
        } else {
            tableBody.innerHTML = '<tr><td colspan="5" class="text-center">No roles found</td></tr>';
        }
    })
    .catch(error => console.error('Error loading roles:', error));
}

function loadMetrics() {
    fetch(`${API_BASE_URL}/api/metrics`, {
        headers: { 'Authorization': `Bearer ${authToken}` }
    })
    .then(response => response.json())
    .then(data => {
        // Update metrics table
        const tableBody = document.getElementById('metrics-table-body');
        tableBody.innerHTML = '';
        
        if (data.metrics && data.metrics.length > 0) {
            data.metrics.forEach(metric => {
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${metric.name}</td>
                    <td>${formatMetricValue(metric)}</td>
                    <td>${metric.description || ''}</td>
                `;
                tableBody.appendChild(row);
            });
        } else {
            tableBody.innerHTML = '<tr><td colspan="3" class="text-center">No metrics available</td></tr>';
        }
        
        // Update charts
        updateRequestRateChart(data.request_rate || []);
        updateResponseTimeChart(data.response_times || []);
    })
    .catch(error => console.error('Error loading metrics:', error));
}

function loadSettings() {
    fetch(`${API_BASE_URL}/api/config`, {
        headers: { 'Authorization': `Bearer ${authToken}` }
    })
    .then(response => response.json())
    .then(data => {
        // General settings
        document.getElementById('server-port').value = data.server?.port || 5000;
        document.getElementById('max-connections').value = data.server?.max_connections || 100;
        document.getElementById('timeout').value = data.server?.timeout || 30;
        document.getElementById('db-path').value = data.database?.path || 'db.json';
        
        // Security settings
        document.getElementById('ssl-enabled').checked = data.ssl?.enabled || false;
        document.getElementById('ssl-cert').value = data.ssl?.cert_path || '';
        document.getElementById('ssl-key').value = data.ssl?.key_path || '';
        document.getElementById('jwt-expiration').value = data.jwt?.expiration || 86400;
        
        // Backup settings
        document.getElementById('auto-backup').checked = data.backup?.enabled || false;
        document.getElementById('backup-interval').value = data.backup?.interval ? data.backup.interval / 3600 : 24;
        document.getElementById('backup-dir').value = data.backup?.path || './backups';
        document.getElementById('max-backups').value = data.backup?.max_files || 10;
        
        // Advanced settings
        document.getElementById('metrics-enabled').checked = data.metrics?.enabled !== false;
        document.getElementById('metrics-interval').value = data.metrics?.interval || 60;
        document.getElementById('log-level').value = data.logging?.level || 'info';
        document.getElementById('throttling-enabled').checked = data.throttling?.enabled || false;
        document.getElementById('rate-limit').value = data.throttling?.rate || 100;
    })
    .catch(error => console.error('Error loading settings:', error));
}

// Action Handlers
function handleActionButton() {
    // Show appropriate modal based on current view
    switch (currentView) {
        case 'collections':
            showNewCollectionModal();
            break;
        case 'documents':
            showNewDocumentModal();
            break;
        case 'users':
            showNewUserModal();
            break;
        case 'roles':
            showNewRoleModal();
            break;
    }
}

function showNewCollectionModal() {
    // Prepare modal
    const modalTitle = document.querySelector('#actionModal .modal-title');
    const modalBody = document.querySelector('#actionModal .modal-body');
    
    modalTitle.textContent = 'Create New Collection';
    modalBody.innerHTML = `
        <form id="new-collection-form">
            <div class="mb-3">
                <label for="collection-name" class="form-label">Collection Name</label>
                <input type="text" class="form-control" id="collection-name" required>
            </div>
            <div class="d-grid gap-2">
                <button type="submit" class="btn btn-primary">Create Collection</button>
            </div>
        </form>
    `;
    
    // Add form submit handler
    document.getElementById('new-collection-form').addEventListener('submit', function(event) {
        event.preventDefault();
        
        const name = document.getElementById('collection-name').value;
        createCollection(name)
            .then(() => {
                bootstrap.Modal.getInstance(actionModal).hide();
                refreshCurrentView();
            })
            .catch(error => showToast(`Error creating collection: ${error.message}`, 'error'));
    });
}

function showNewDocumentModal() {
    // Check if a collection is selected
    if (!currentCollection) {
        showToast('Please select a collection first', 'warning');
        bootstrap.Modal.getInstance(actionModal).hide();
        return;
    }
    
    // Prepare modal
    const modalTitle = document.querySelector('#actionModal .modal-title');
    const modalBody = document.querySelector('#actionModal .modal-body');
    
    modalTitle.textContent = 'Create New Document';
    modalBody.innerHTML = `
        <div id="new-document-editor" style="height: 400px;"></div>
        <div class="d-grid gap-2 mt-3">
            <button id="create-document-btn" class="btn btn-primary">Create Document</button>
        </div>
    `;
    
    // Initialize JSON editor
    const container = document.getElementById('new-document-editor');
    const newDocEditor = new JSONEditor(container, {
        mode: 'tree',
        modes: ['tree', 'text', 'form'],
        mainMenuBar: true
    });
    
    // Set initial empty object
    newDocEditor.set({});
    
    // Add button click handler
    document.getElementById('create-document-btn').addEventListener('click', function() {
        try {
            const document = newDocEditor.get();
            createDocument(currentCollection, document)
                .then(() => {
                    bootstrap.Modal.getInstance(actionModal).hide();
                    refreshCurrentView();
                })
                .catch(error => showToast(`Error creating document: ${error.message}`, 'error'));
        } catch (e) {
            showToast('Invalid JSON: ' + e.message, 'error');
        }
    });
}

function showNewUserModal() {
    // First, get available roles
    fetch(`${API_BASE_URL}/api/roles`, {
        headers: { 'Authorization': `Bearer ${authToken}` }
    })
    .then(response => response.json())
    .then(data => {
        // Prepare modal
        const modalTitle = document.querySelector('#actionModal .modal-title');
        const modalBody = document.querySelector('#actionModal .modal-body');
        
        modalTitle.textContent = 'Create New User';
        
        // Generate role checkboxes
        let roleOptions = '';
        if (data.roles && data.roles.length > 0) {
            data.roles.forEach(role => {
                roleOptions += `
                    <div class="form-check">
                        <input class="form-check-input" type="checkbox" value="${role.id}" id="role-${role.id}">
                        <label class="form-check-label" for="role-${role.id}">
                            ${role.name}
                        </label>
                    </div>
                `;
            });
        } else {
            roleOptions = '<div class="text-muted">No roles available</div>';
        }
        
        modalBody.innerHTML = `
            <form id="new-user-form">
                <div class="mb-3">
                    <label for="user-username" class="form-label">Username</label>
                    <input type="text" class="form-control" id="user-username" required>
                </div>
                <div class="mb-3">
                    <label for="user-password" class="form-label">Password</label>
                    <input type="password" class="form-control" id="user-password" required>
                </div>
                <div class="mb-3">
                    <label class="form-label">Roles</label>
                    ${roleOptions}
                </div>
                <div class="d-grid gap-2">
                    <button type="submit" class="btn btn-primary">Create User</button>
                </div>
            </form>
        `;
        
        // Add form submit handler
        document.getElementById('new-user-form').addEventListener('submit', function(event) {
            event.preventDefault();
            
            const username = document.getElementById('user-username').value;
            const password = document.getElementById('user-password').value;
            
            // Get selected roles
            const roleCheckboxes = document.querySelectorAll('#new-user-form input[type="checkbox"]:checked');
            const roles = Array.from(roleCheckboxes).map(cb => cb.value);
            
            createUser(username, password, roles)
                .then(() => {
                    bootstrap.Modal.getInstance(actionModal).hide();
                    refreshCurrentView();
                })
                .catch(error => showToast(`Error creating user: ${error.message}`, 'error'));
        });
    })
    .catch(error => {
        console.error('Error loading roles for user creation:', error);
        showToast('Error loading roles: ' + error.message, 'error');
    });
}

function showNewRoleModal() {
    // Prepare modal
    const modalTitle = document.querySelector('#actionModal .modal-title');
    const modalBody = document.querySelector('#actionModal .modal-body');
    
    modalTitle.textContent = 'Create New Role';
    modalBody.innerHTML = `
        <form id="new-role-form">
            <div class="mb-3">
                <label for="role-name" class="form-label">Role Name</label>
                <input type="text" class="form-control" id="role-name" required>
            </div>
            <div class="mb-3">
                <label class="form-label">Default Permissions</label>
                <div class="form-check">
                    <input class="form-check-input" type="checkbox" value="read" id="perm-read" checked>
                    <label class="form-check-label" for="perm-read">Read</label>
                </div>
                <div class="form-check">
                    <input class="form-check-input" type="checkbox" value="write" id="perm-write">
                    <label class="form-check-label" for="perm-write">Write</label>
                </div>
                <div class="form-check">
                    <input class="form-check-input" type="checkbox" value="delete" id="perm-delete">
                    <label class="form-check-label" for="perm-delete">Delete</label>
                </div>
                <div class="form-check">
                    <input class="form-check-input" type="checkbox" value="admin" id="perm-admin">
                    <label class="form-check-label" for="perm-admin">Admin</label>
                </div>
            </div>
            <div class="d-grid gap-2">
                <button type="submit" class="btn btn-primary">Create Role</button>
            </div>
        </form>
    `;
    
    // Add form submit handler
    document.getElementById('new-role-form').addEventListener('submit', function(event) {
        event.preventDefault();
        
        const name = document.getElementById('role-name').value;
        
        // Get selected permissions
        const permissions = {
            read: document.getElementById('perm-read').checked,
            write: document.getElementById('perm-write').checked,
            delete: document.getElementById('perm-delete').checked,
            admin: document.getElementById('perm-admin').checked
        };
        
        createRole(name, permissions)
            .then(() => {
                bootstrap.Modal.getInstance(actionModal).hide();
                refreshCurrentView();
            })
            .catch(error => showToast(`Error creating role: ${error.message}`, 'error'));
    });
}

function showConfirmationModal(message, confirmCallback) {
    // Set message
    document.getElementById('confirmation-message').textContent = message;
    
    // Store callback
    document.getElementById('confirm-action-btn').onclick = () => {
        confirmCallback();
        bootstrap.Modal.getInstance(confirmationModal).hide();
    };
    
    // Show modal
    new bootstrap.Modal(confirmationModal).show();
}

let confirmedAction = null;

function showConfirmationModal(message, confirmCallback) {
    document.getElementById('confirmation-message').textContent = message;
    confirmedAction = confirmCallback;
    new bootstrap.Modal(document.getElementById('confirmationModal')).show();
}

function executeConfirmedAction() {
    if (confirmedAction) {
        confirmedAction();
        confirmedAction = null;
        bootstrap.Modal.getInstance(document.getElementById('confirmationModal')).hide();
    }
}

// API Functions
function createCollection(name) {
    return fetch(`${API_BASE_URL}/api/collections`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${authToken}`
        },
        body: JSON.stringify({ name })
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to create collection');
            });
        }
        return response.json();
    });
}

function deleteCollection(name) {
    return fetch(`${API_BASE_URL}/api/collections/${name}`, {
        method: 'DELETE',
        headers: {
            'Authorization': `Bearer ${authToken}`
        }
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to delete collection');
            });
        }
        loadCollections();
        showToast(`Collection "${name}" deleted successfully`);
    })
    .catch(error => {
        console.error(`Error deleting collection ${name}:`, error);
        showToast(`Error deleting collection: ${error.message}`, 'error');
    });
}

function createDocument(collection, document) {
    return fetch(`${API_BASE_URL}/api/collections/${collection}/documents`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${authToken}`
        },
        body: JSON.stringify(document)
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to create document');
            });
        }
        return response.json();
    })
    .then(data => {
        showToast(`Document created with ID: ${data._id}`);
        return data;
    });
}

function saveDocument() {
    // Get document from editor
    try {
        const document = jsonEditor.get();
        const collection = jsonEditor.collection;
        const id = jsonEditor.documentId;
        
        if (!collection || !id) {
            throw new Error('Missing collection or document ID');
        }
        
        // Update document
        fetch(`${API_BASE_URL}/api/collections/${collection}/documents/${id}`, {
            method: 'PUT',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${authToken}`
            },
            body: JSON.stringify(document)
        })
        .then(response => {
            if (!response.ok) {
                return response.json().then(data => {
                    throw new Error(data.error || 'Failed to update document');
                });
            }
            return response.json();
        })
        .then(() => {
            showToast('Document updated successfully');
            bootstrap.Modal.getInstance(documentViewModal).hide();
            loadDocuments(collection);
        })
        .catch(error => {
            console.error('Error saving document:', error);
            showToast(`Error saving document: ${error.message}`, 'error');
        });
    } catch (e) {
        showToast('Invalid JSON: ' + e.message, 'error');
    }
}

function deleteDocument(collection, id) {
    return fetch(`${API_BASE_URL}/api/collections/${collection}/documents/${id}`, {
        method: 'DELETE',
        headers: {
            'Authorization': `Bearer ${authToken}`
        }
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to delete document');
            });
        }
        loadDocuments(collection);
        showToast('Document deleted successfully');
    })
    .catch(error => {
        console.error(`Error deleting document ${id}:`, error);
        showToast(`Error deleting document: ${error.message}`, 'error');
    });
}

function createUser(username, password, roles) {
    return fetch(`${API_BASE_URL}/api/users`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${authToken}`
        },
        body: JSON.stringify({ username, password, roles })
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to create user');
            });
        }
        return response.json();
    })
    .then(data => {
        showToast(`User "${username}" created successfully`);
        return data;
    });
}

function deleteUser(id) {
    return fetch(`${API_BASE_URL}/api/users/${id}`, {
        method: 'DELETE',
        headers: {
            'Authorization': `Bearer ${authToken}`
        }
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to delete user');
            });
        }
        loadUsers();
        showToast('User deleted successfully');
    })
    .catch(error => {
        console.error(`Error deleting user ${id}:`, error);
        showToast(`Error deleting user: ${error.message}`, 'error');
    });
}

function createRole(name, permissions) {
    return fetch(`${API_BASE_URL}/api/roles`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${authToken}`
        },
        body: JSON.stringify({ name, permissions })
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to create role');
            });
        }
        return response.json();
    })
    .then(data => {
        showToast(`Role "${name}" created successfully`);
        return data;
    });
}

function deleteRole(id) {
    return fetch(`${API_BASE_URL}/api/roles/${id}`, {
        method: 'DELETE',
        headers: {
            'Authorization': `Bearer ${authToken}`
        }
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to delete role');
            });
        }
        loadRoles();
        showToast('Role deleted successfully');
    })
    .catch(error => {
        console.error(`Error deleting role ${id}:`, error);
        showToast(`Error deleting role: ${error.message}`, 'error');
    });
}

function saveSettings(formId, formData) {
    // Convert form data to object
    const settings = {};
    formData.forEach((value, key) => {
        const parts = key.split('-');
        if (parts.length === 2) {
            const section = parts[0];
            const field = parts[1];
            
            if (!settings[section]) {
                settings[section] = {};
            }
            
            // Convert checkboxes
            if (value === 'on') {
                settings[section][field] = true;
            } else if (value === 'off') {
                settings[section][field] = false;
            } else {
                // Try to convert numbers
                const numValue = Number(value);
                settings[section][field] = isNaN(numValue) ? value : numValue;
            }
        }
    });
    
    // Send settings to API
    return fetch(`${API_BASE_URL}/api/config`, {
        method: 'PUT',
        headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${authToken}`
        },
        body: JSON.stringify(settings)
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to save settings');
            });
        }
        return response.json();
    });
}

function createBackup() {
    return fetch(`${API_BASE_URL}/api/backup`, {
        method: 'POST',
        headers: {
            'Authorization': `Bearer ${authToken}`
        }
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to create backup');
            });
        }
        return response.json();
    })
    .then(data => {
        showToast(`Backup created: ${data.path}`);
        return data;
    })
    .catch(error => {
        console.error('Error creating backup:', error);
        showToast(`Error creating backup: ${error.message}`, 'error');
    });
}

function generateJwtSecret() {
    // Generate a random string for JWT secret
    const array = new Uint8Array(32);
    window.crypto.getRandomValues(array);
    const secret = Array.from(array, byte => byte.toString(16).padStart(2, '0')).join('');
    
    // Set value
    document.getElementById('jwt-secret').value = secret;
}

// Chart Functions
function initializeJsonEditor() {
    const container = document.getElementById('json-editor');
    jsonEditor = new JSONEditor(container, {
        mode: 'tree',
        modes: ['tree', 'text', 'form', 'view'],
        mainMenuBar: true
    });
}

function updateHealthChart(data) {
    const ctx = document.getElementById('health-chart');
    
    if (!ctx) return;
    
    // Destroy existing chart
    if (charts.health) {
        charts.health.destroy();
    }
    
    // Create new chart
    charts.health = new Chart(ctx, {
        type: 'line',
        data: {
            labels: data.health_timestamps || [],
            datasets: [
                {
                    label: 'CPU Usage (%)',
                    data: data.cpu_usage || [],
                    borderColor: 'rgba(75, 192, 192, 1)',
                    backgroundColor: 'rgba(75, 192, 192, 0.2)',
                    tension: 0.4
                },
                {
                    label: 'Memory Usage (MB)',
                    data: data.memory_usage || [],
                    borderColor: 'rgba(153, 102, 255, 1)',
                    backgroundColor: 'rgba(153, 102, 255, 0.2)',
                    tension: 0.4
                }
            ]
        },
        options: {
            responsive: true,
            plugins: {
                title: {
                    display: false
                },
                tooltip: {
                    mode: 'index',
                    intersect: false,
                }
            },
            scales: {
                y: {
                    beginAtZero: true
                }
            }
        }
    });
}

function updateRequestRateChart(data) {
    const ctx = document.getElementById('request-rate-chart');
    
    if (!ctx) return;
    
    // Prepare data
    const labels = data.map(item => item.timestamp);
    const values = data.map(item => item.rate);
    
    // Destroy existing chart
    if (charts.requestRate) {
        charts.requestRate.destroy();
    }
    
    // Create new chart
    charts.requestRate = new Chart(ctx, {
        type: 'line',
        data: {
            labels: labels,
            datasets: [{
                label: 'Requests per Minute',
                data: values,
                borderColor: 'rgba(54, 162, 235, 1)',
                backgroundColor: 'rgba(54, 162, 235, 0.2)',
                tension: 0.4,
                fill: true
            }]
        },
        options: {
            responsive: true,
            plugins: {
                title: {
                    display: false
                }
            },
            scales: {
                y: {
                    beginAtZero: true
                }
            }
        }
    });
}

function updateResponseTimeChart(data) {
    const ctx = document.getElementById('response-time-chart');
    
    if (!ctx) return;
    
    // Prepare data
    const labels = data.map(item => item.timestamp);
    const averages = data.map(item => item.avg_ms);
    const p95Values = data.map(item => item.p95_ms);
    const maxValues = data.map(item => item.max_ms);
    
    // Destroy existing chart
    if (charts.responseTime) {
        charts.responseTime.destroy();
    }
    
    // Create new chart
    charts.responseTime = new Chart(ctx, {
        type: 'line',
        data: {
            labels: labels,
            datasets: [
                {
                    label: 'Average (ms)',
                    data: averages,
                    borderColor: 'rgba(75, 192, 192, 1)',
                    backgroundColor: 'rgba(75, 192, 192, 0.2)',
                    tension: 0.4
                },
                {
                    label: '95th Percentile (ms)',
                    data: p95Values,
                    borderColor: 'rgba(255, 159, 64, 1)',
                    backgroundColor: 'rgba(255, 159, 64, 0.2)',
                    tension: 0.4
                },
                {
                    label: 'Max (ms)',
                    data: maxValues,
                    borderColor: 'rgba(255, 99, 132, 1)',
                    backgroundColor: 'rgba(255, 99, 132, 0.2)',
                    tension: 0.4
                }
            ]
        },
        options: {
            responsive: true,
            plugins: {
                title: {
                    display: false
                }
            },
            scales: {
                y: {
                    beginAtZero: true
                }
            }
        }
    });
}

// Utility Functions
function updatePagination(total, currentPage, limit) {
    const paginationEl = document.getElementById('documents-pagination');
    paginationEl.innerHTML = '';
    
    // Simple case: few documents
    if (total <= limit) {
        return;
    }
    
    const totalPages = Math.ceil(total / limit);
    
    // Previous button
    const prevLi = document.createElement('li');
    prevLi.className = `page-item ${currentPage === 1 ? 'disabled' : ''}`;
    prevLi.innerHTML = `<a class="page-link" href="#" data-page="${currentPage - 1}">&laquo;</a>`;
    paginationEl.appendChild(prevLi);
    
    // Page numbers
    const maxPages = 5; // Show up to 5 page numbers
    let startPage = Math.max(1, currentPage - Math.floor(maxPages / 2));
    let endPage = Math.min(totalPages, startPage + maxPages - 1);
    
    if (endPage - startPage + 1 < maxPages) {
        startPage = Math.max(1, endPage - maxPages + 1);
    }
    
    for (let i = startPage; i <= endPage; i++) {
        const pageLi = document.createElement('li');
        pageLi.className = `page-item ${i === currentPage ? 'active' : ''}`;
        pageLi.innerHTML = `<a class="page-link" href="#" data-page="${i}">${i}</a>`;
        paginationEl.appendChild(pageLi);
    }
    
    // Next button
    const nextLi = document.createElement('li');
    nextLi.className = `page-item ${currentPage === totalPages ? 'disabled' : ''}`;
    nextLi.innerHTML = `<a class="page-link" href="#" data-page="${currentPage + 1}">&raquo;</a>`;
    paginationEl.appendChild(nextLi);
    
    // Add event listeners
    paginationEl.querySelectorAll('.page-link').forEach(link => {
        link.addEventListener('click', function(event) {
            event.preventDefault();
            const page = parseInt(this.dataset.page);
            loadDocuments(currentCollection, document.getElementById('document-search').value, page);
        });
    });
}

function formatDateTime(timestamp) {
    if (!timestamp) return 'N/A';
    
    const date = new Date(timestamp);
    return date.toLocaleString();
}

function formatSize(bytes) {
    if (bytes === 0 || !bytes) return '0 B';
    
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(1024));
    return parseFloat((bytes / Math.pow(1024, i)).toFixed(2)) + ' ' + sizes[i];
}

function formatJsonPreview(json) {
    // Remove internal fields for preview
    const preview = { ...json };
    delete preview._id;
    delete preview._created;
    delete preview._updated;
    
    // Stringify and truncate
    let str = JSON.stringify(preview);
    if (str.length > 50) {
        str = str.substring(0, 47) + '...';
    }
    return str;
}

function formatRoles(roles) {
    if (!roles || !Array.isArray(roles) || roles.length === 0) {
        return '<span class="text-muted">None</span>';
    }
    
    return roles.map(role => `<span class="badge bg-secondary">${role}</span>`).join(' ');
}

function formatPermissions(permissions) {
    if (!permissions || Object.keys(permissions).length === 0) {
        return '<span class="text-muted">None</span>';
    }
    
    // Format as badges
    let result = '';
    if (permissions.read) {
        result += '<span class="badge bg-info">Read</span> ';
    }
    if (permissions.write) {
        result += '<span class="badge bg-success">Write</span> ';
    }
    if (permissions.delete) {
        result += '<span class="badge bg-warning">Delete</span> ';
    }
    if (permissions.admin) {
        result += '<span class="badge bg-danger">Admin</span>';
    }
    
    return result || '<span class="text-muted">None</span>';
}

function formatMetricValue(metric) {
    if (!metric) return '';
    
    if (metric.type === 'counter') {
        return metric.value.toString();
    } else if (metric.type === 'gauge') {
        return metric.value.toFixed(2);
    } else if (metric.type === 'timer') {
        return `${metric.avg_ms.toFixed(2)} ms avg (${metric.count} samples)`;
    } else if (metric.type === 'histogram') {
        return `min: ${metric.min.toFixed(2)}, max: ${metric.max.toFixed(2)}, avg: ${metric.avg.toFixed(2)}`;
    }
    
    return String(metric.value);
}

// Transaction Visualization Functions
function loadTransactionVisualization() {
    // Reset visualization type if needed
    if (!transactionVisType) {
        transactionVisType = 'timeline';
    }
    
    // Update active visualization tab
    document.querySelectorAll('.transaction-vis-tab').forEach(tab => {
        tab.classList.remove('active');
    });
    document.getElementById(`vis-tab-${transactionVisType}`).classList.add('active');
    
    // Show appropriate visualization content
    document.querySelectorAll('.transaction-vis-content').forEach(content => {
        content.classList.add('d-none');
    });
    document.getElementById(`vis-content-${transactionVisType}`).classList.remove('d-none');
    
    // Load data based on selected visualization type
    switch (transactionVisType) {
        case 'timeline':
            loadTransactionTimeline();
            break;
        case 'metrics':
            loadTransactionMetrics();
            break;
        case 'relationships':
            loadTransactionRelationships();
            break;
    }
}

function loadTransactionTimeline() {
    // Get date range values (default to last 24 hours if not set)
    const endTime = Math.floor(Date.now() / 1000);
    const startTime = endTime - (24 * 60 * 60); // 24 hours ago
    
    // Load transaction history data
    fetch(`${API_BASE_URL}/api/visualization/transaction-history?start_time=${startTime}&end_time=${endTime}&limit=100`, {
        headers: { 'Authorization': `Bearer ${authToken}` }
    })
    .then(response => response.json())
    .then(data => {
        // Store transaction history data
        transactionHistory = data.history || [];
        
        // Update transaction stats
        updateTransactionStats(data);
        
        // Render timeline visualization
        renderTransactionTimeline(transactionHistory);
    })
    .catch(error => {
        console.error('Error loading transaction history:', error);
        showToast(`Error loading transaction history: ${error.message}`, 'error');
    });
}

function loadTransactionMetrics() {
    // Get date range and dimension values
    const endTime = Math.floor(Date.now() / 1000);
    const startTime = endTime - (7 * 24 * 60 * 60); // 7 days ago
    const dimension = document.getElementById('metrics-dimension-selector').value || 'time';
    
    // Load transaction metrics data
    fetch(`${API_BASE_URL}/api/visualization/transaction-metrics?dimension=${dimension}&start_time=${startTime}&end_time=${endTime}`, {
        headers: { 'Authorization': `Bearer ${authToken}` }
    })
    .then(response => response.json())
    .then(data => {
        // Store transaction metrics data
        transactionMetrics = data;
        
        // Render metrics visualization
        renderTransactionMetrics(transactionMetrics);
    })
    .catch(error => {
        console.error('Error loading transaction metrics:', error);
        showToast(`Error loading transaction metrics: ${error.message}`, 'error');
    });
}

function loadTransactionRelationships() {
    // Get filter values
    const endTime = Math.floor(Date.now() / 1000);
    const startTime = endTime - (24 * 60 * 60); // 24 hours ago
    const collection = document.getElementById('relationship-collection-filter').value || '';
    const documentId = document.getElementById('relationship-document-filter').value || '';
    
    // Build query string
    let queryString = `start_time=${startTime}&end_time=${endTime}`;
    if (collection) queryString += `&collection=${encodeURIComponent(collection)}`;
    if (documentId) queryString += `&document_id=${encodeURIComponent(documentId)}`;
    
    // Load transaction relationships data
    fetch(`${API_BASE_URL}/api/visualization/transaction-relationships?${queryString}`, {
        headers: { 'Authorization': `Bearer ${authToken}` }
    })
    .then(response => response.json())
    .then(data => {
        // Store transaction relationships data
        transactionRelationships = data;
        
        // Render relationships visualization
        renderTransactionRelationships(transactionRelationships);
    })
    .catch(error => {
        console.error('Error loading transaction relationships:', error);
        showToast(`Error loading transaction relationships: ${error.message}`, 'error');
    });
}

function updateTransactionStats(data) {
    // Calculate basic statistics
    const totalTransactions = data.count || 0;
    let committed = 0;
    let aborted = 0;
    let active = 0;
    let avgDuration = 0;
    let totalDuration = 0;
    let durationCount = 0;
    
    // Process transaction history
    transactionHistory.forEach(tx => {
        if (tx.state === 'committed') {
            committed++;
            if (tx.duration) {
                totalDuration += tx.duration;
                durationCount++;
            }
        } else if (tx.state === 'aborted') {
            aborted++;
        } else if (tx.state === 'active') {
            active++;
        }
    });
    
    // Calculate average duration
    if (durationCount > 0) {
        avgDuration = totalDuration / durationCount;
    }
    
    // Update stats display
    document.getElementById('transactions-total').textContent = totalTransactions;
    document.getElementById('transactions-committed').textContent = committed;
    document.getElementById('transactions-aborted').textContent = aborted;
    document.getElementById('transactions-active').textContent = active;
    document.getElementById('transactions-avg-duration').textContent = avgDuration.toFixed(2) + ' sec';
}

function renderTransactionTimeline(transactions) {
    const ctx = document.getElementById('transaction-timeline-chart');
    
    if (!ctx) return;
    
    // Destroy existing chart
    if (activeTransactionChart) {
        activeTransactionChart.destroy();
    }
    
    // Prepare data for timeline
    const datasets = [];
    const transactionIds = new Set();
    const timeLabels = [];
    const timeData = {};
    
    // Process transactions to group by transaction id
    transactions.forEach(tx => {
        if (tx.type === 'STATE') {
            // Add to set of transaction ids
            transactionIds.add(tx.transaction_id);
            
            // Format timestamp
            const date = new Date(tx.timestamp * 1000);
            const timeLabel = date.toLocaleTimeString();
            
            if (!timeLabels.includes(timeLabel)) {
                timeLabels.push(timeLabel);
            }
            
            // Initialize data for this transaction id if needed
            if (!timeData[tx.transaction_id]) {
                timeData[tx.transaction_id] = {};
            }
            
            // Store state at this timestamp
            timeData[tx.transaction_id][timeLabel] = tx.state;
        }
    });
    
    // Sort time labels
    timeLabels.sort((a, b) => {
        const dateA = new Date(a);
        const dateB = new Date(b);
        return dateA - dateB;
    });
    
    // Create datasets for each transaction
    const colors = [
        'rgba(75, 192, 192, 1)',
        'rgba(153, 102, 255, 1)',
        'rgba(255, 159, 64, 1)',
        'rgba(255, 99, 132, 1)',
        'rgba(54, 162, 235, 1)'
    ];
    
    let colorIndex = 0;
    transactionIds.forEach(id => {
        // Get state values for each time label
        const data = timeLabels.map(label => {
            if (!timeData[id][label]) return null;
            
            // Convert state to numeric value for chart
            switch (timeData[id][label]) {
                case 'active': return 1;
                case 'committing': return 2;
                case 'committed': return 3;
                case 'aborting': return -1;
                case 'aborted': return -2;
                default: return 0;
            }
        });
        
        // Add dataset
        datasets.push({
            label: `Transaction ${id.substring(0, 8)}...`,
            data: data,
            borderColor: colors[colorIndex % colors.length],
            backgroundColor: colors[colorIndex % colors.length].replace('1)', '0.2)'),
            tension: 0.4,
            spanGaps: true
        });
        
        colorIndex++;
    });
    
    // Create the chart
    activeTransactionChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: timeLabels,
            datasets: datasets
        },
        options: {
            responsive: true,
            plugins: {
                title: {
                    display: true,
                    text: 'Transaction States Over Time'
                },
                tooltip: {
                    callbacks: {
                        label: function(context) {
                            const yValue = context.parsed.y;
                            let state = 'Unknown';
                            
                            switch (yValue) {
                                case 1: state = 'Active'; break;
                                case 2: state = 'Committing'; break;
                                case 3: state = 'Committed'; break;
                                case -1: state = 'Aborting'; break;
                                case -2: state = 'Aborted'; break;
                                default: state = 'Unknown';
                            }
                            
                            return `${context.dataset.label}: ${state}`;
                        }
                    }
                }
            },
            scales: {
                y: {
                    ticks: {
                        callback: function(value) {
                            switch (value) {
                                case 1: return 'Active';
                                case 2: return 'Committing';
                                case 3: return 'Committed';
                                case -1: return 'Aborting';
                                case -2: return 'Aborted';
                                default: return '';
                            }
                        }
                    }
                }
            }
        }
    });
    
    // Update operations table
    renderTransactionOperationsTable(transactions);
}

function renderTransactionOperationsTable(transactions) {
    const tableBody = document.getElementById('transaction-operations-table-body');
    if (!tableBody) return;
    
    tableBody.innerHTML = '';
    
    // Filter for operation entries
    const operations = transactions.filter(tx => tx.type === 'OPERATION');
    
    if (operations.length > 0) {
        operations.forEach(op => {
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${formatDateTime(op.timestamp * 1000)}</td>
                <td>${op.transaction_id}</td>
                <td>${op.collection || 'N/A'}</td>
                <td>${op.document_id || 'N/A'}</td>
                <td>
                    <span class="badge ${getBadgeClass(op.operation)}">
                        ${op.operation || 'N/A'}
                    </span>
                </td>
                <td>${op.complexity || 'N/A'}</td>
            `;
            tableBody.appendChild(row);
        });
    } else {
        tableBody.innerHTML = '<tr><td colspan="6" class="text-center">No operations found</td></tr>';
    }
}

function renderTransactionMetrics(metrics) {
    const ctx = document.getElementById('transaction-metrics-chart');
    if (!ctx) return;
    
    // Destroy existing chart
    if (activeMetricsChart) {
        activeMetricsChart.destroy();
    }
    
    // Get dimension and data
    const dimension = metrics.dimension || 'time';
    const data = metrics.data || {};
    
    // Prepare chart data based on dimension
    let labels = [];
    let datasets = [];
    
    if (dimension === 'time') {
        // Time-based metrics (hours)
        labels = Object.keys(data).sort();
        
        // Transaction count dataset
        const transactionCounts = labels.map(hour => data[hour].transaction_count || 0);
        datasets.push({
            label: 'Transaction Count',
            data: transactionCounts,
            borderColor: 'rgba(75, 192, 192, 1)',
            backgroundColor: 'rgba(75, 192, 192, 0.2)',
            type: 'bar'
        });
        
        // Commit/abort counts
        const commitCounts = labels.map(hour => data[hour].commit_count || 0);
        const abortCounts = labels.map(hour => data[hour].abort_count || 0);
        
        datasets.push({
            label: 'Commits',
            data: commitCounts,
            borderColor: 'rgba(54, 162, 235, 1)',
            backgroundColor: 'rgba(54, 162, 235, 0.2)',
            type: 'bar'
        });
        
        datasets.push({
            label: 'Aborts',
            data: abortCounts,
            borderColor: 'rgba(255, 99, 132, 1)',
            backgroundColor: 'rgba(255, 99, 132, 0.2)',
            type: 'bar'
        });
        
        // Average duration line
        const avgDurations = labels.map(hour => data[hour].avg_duration_ms || 0);
        datasets.push({
            label: 'Avg Duration (ms)',
            data: avgDurations,
            borderColor: 'rgba(255, 159, 64, 1)',
            backgroundColor: 'rgba(255, 159, 64, 0.2)',
            type: 'line',
            yAxisID: 'y1'
        });
    } 
    else if (dimension === 'user') {
        // User-based metrics
        labels = Object.keys(data);
        
        // Transaction, commit, abort counts
        const transactionCounts = labels.map(user => data[user].transaction_count || 0);
        const commitCounts = labels.map(user => data[user].commit_count || 0);
        const abortCounts = labels.map(user => data[user].abort_count || 0);
        
        datasets.push({
            label: 'Transactions',
            data: transactionCounts,
            backgroundColor: 'rgba(75, 192, 192, 0.7)'
        });
        
        datasets.push({
            label: 'Commits',
            data: commitCounts,
            backgroundColor: 'rgba(54, 162, 235, 0.7)'
        });
        
        datasets.push({
            label: 'Aborts',
            data: abortCounts,
            backgroundColor: 'rgba(255, 99, 132, 0.7)'
        });
    }
    else if (dimension === 'isolation') {
        // Isolation level metrics
        labels = Object.keys(data);
        
        // Success rate (commits / transactions)
        const successRates = labels.map(level => {
            const transactions = data[level].transaction_count || 0;
            const commits = data[level].commit_count || 0;
            return transactions > 0 ? (commits / transactions) * 100 : 0;
        });
        
        datasets.push({
            label: 'Success Rate (%)',
            data: successRates,
            backgroundColor: [
                'rgba(75, 192, 192, 0.7)',
                'rgba(54, 162, 235, 0.7)',
                'rgba(255, 99, 132, 0.7)'
            ]
        });
    }
    else if (dimension === 'collection') {
        // Collection-based metrics
        labels = Object.keys(data);
        
        // Operation types
        const insertCounts = labels.map(collection => data[collection].insert_count || 0);
        const updateCounts = labels.map(collection => data[collection].update_count || 0);
        const deleteCounts = labels.map(collection => data[collection].delete_count || 0);
        
        datasets.push({
            label: 'Inserts',
            data: insertCounts,
            backgroundColor: 'rgba(75, 192, 192, 0.7)'
        });
        
        datasets.push({
            label: 'Updates',
            data: updateCounts,
            backgroundColor: 'rgba(54, 162, 235, 0.7)'
        });
        
        datasets.push({
            label: 'Deletes',
            data: deleteCounts,
            backgroundColor: 'rgba(255, 99, 132, 0.7)'
        });
    }
    
    // Create chart based on dimension
    const chartType = dimension === 'time' ? 'bar' : 
                      dimension === 'isolation' ? 'pie' : 'bar';
    
    activeMetricsChart = new Chart(ctx, {
        type: chartType,
        data: {
            labels: labels,
            datasets: datasets
        },
        options: {
            responsive: true,
            plugins: {
                title: {
                    display: true,
                    text: `Transaction Metrics by ${dimension.charAt(0).toUpperCase() + dimension.slice(1)}`
                },
                tooltip: {
                    mode: 'index',
                    intersect: false
                }
            },
            scales: dimension !== 'isolation' && dimension !== 'pie' ? {
                x: {
                    stacked: dimension !== 'time'
                },
                y: {
                    beginAtZero: true,
                    stacked: dimension !== 'time',
                    title: {
                        display: true,
                        text: 'Count'
                    }
                },
                y1: dimension === 'time' ? {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    beginAtZero: true,
                    title: {
                        display: true,
                        text: 'Duration (ms)'
                    },
                    grid: {
                        drawOnChartArea: false
                    }
                } : undefined
            } : {}
        }
    });
}

function renderTransactionRelationships(relationships) {
    const container = document.getElementById('transaction-relationships-container');
    if (!container) return;
    
    // Check if we have data
    if (!relationships.nodes || relationships.nodes.length === 0) {
        container.innerHTML = '<div class="alert alert-info">No transaction relationships found.</div>';
        return;
    }
    
    // Clear previous visualization
    container.innerHTML = '';
    
    // Create a new visualization if we have a visualization library
    if (typeof vis !== 'undefined') {
        // Create a new vis.js Network
        const nodes = new vis.DataSet();
        const edges = new vis.DataSet();
        
        // Add nodes
        relationships.nodes.forEach(node => {
            let color, shape, label;
            
            if (node.type === 'transaction') {
                color = node.state === 'committed' ? '#4CAF50' : 
                        node.state === 'aborted' ? '#F44336' : '#2196F3';
                shape = 'dot';
                label = `Tx: ${node.id.substring(0, 8)}`;
            } else {
                color = '#FF9800';
                shape = 'square';
                label = `${node.collection}: ${node.document_id.substring(0, 8)}`;
            }
            
            nodes.add({
                id: node.id,
                label: label,
                color: color,
                shape: shape,
                title: JSON.stringify(node, null, 2)
            });
        });
        
        // Add edges
        relationships.edges.forEach(edge => {
            let color, width, label;
            
            switch (edge.operation) {
                case 'insert':
                    color = '#4CAF50';
                    label = 'INSERT';
                    break;
                case 'update':
                    color = '#2196F3';
                    label = 'UPDATE';
                    break;
                case 'delete':
                    color = '#F44336';
                    label = 'DELETE';
                    break;
                default:
                    color = '#9E9E9E';
                    label = 'ACTION';
            }
            
            edges.add({
                id: edge.id,
                from: edge.source,
                to: edge.target,
                label: label,
                color: color,
                arrows: 'to'
            });
        });
        
        // Create the network
        const options = {
            layout: {
                hierarchical: false
            },
            physics: {
                stabilization: true,
                barnesHut: {
                    gravitationalConstant: -2000,
                    centralGravity: 0.3,
                    springLength: 150,
                    springConstant: 0.04
                }
            },
            interaction: {
                tooltipDelay: 200,
                hover: true
            }
        };
        
        const network = new vis.Network(
            container, 
            { nodes: nodes, edges: edges }, 
            options
        );
    } else {
        // Fallback to basic display
        container.innerHTML = `
            <div class="card">
                <div class="card-body">
                    <h5 class="card-title">Transaction Graph</h5>
                    <p class="card-text">${relationships.nodes.length} nodes and ${relationships.edges.length} connections found.</p>
                    <p class="text-muted">Install vis.js for interactive visualization.</p>
                </div>
            </div>
        `;
    }
}

function switchTransactionVisType(type) {
    transactionVisType = type;
    loadTransactionVisualization();
}

function getBadgeClass(operation) {
    switch (operation) {
        case 'insert': return 'bg-success';
        case 'update': return 'bg-primary';
        case 'delete': return 'bg-danger';
        default: return 'bg-secondary';
    }
}

// Toast notifications
function showToast(message, type = 'success') {
    // Create toast container if it doesn't exist
    let toastContainer = document.querySelector('.toast-container');
    if (!toastContainer) {
        toastContainer = document.createElement('div');
        toastContainer.className = 'toast-container position-fixed bottom-0 end-0 p-3';
        document.body.appendChild(toastContainer);
    }
    
    // Create toast element
    const toastId = 'toast-' + Date.now();
    const toastEl = document.createElement('div');
    toastEl.className = `toast align-items-center text-white bg-${type === 'error' ? 'danger' : type}`;
    toastEl.setAttribute('role', 'alert');
    toastEl.setAttribute('aria-live', 'assertive');
    toastEl.setAttribute('aria-atomic', 'true');
    toastEl.setAttribute('id', toastId);
    
    toastEl.innerHTML = `
        <div class="d-flex">
            <div class="toast-body">
                ${message}
            </div>
            <button type="button" class="btn-close btn-close-white me-2 m-auto" data-bs-dismiss="toast" aria-label="Close"></button>
        </div>
    `;
    
    toastContainer.appendChild(toastEl);
    
    // Initialize and show toast
    const toast = new bootstrap.Toast(toastEl, {
        autohide: true,
        delay: 5000
    });
    
    toast.show();
    
    // Remove from DOM after hiding
    toastEl.addEventListener('hidden.bs.toast', function() {
        this.remove();
    });
}