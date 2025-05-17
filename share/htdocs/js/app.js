// JSON Database Admin Interface

// Configuration
// Always use the current host (dynamically determined at runtime)
// This ensures API requests go to the same host that served the page
const API_BASE_URL = window.location.protocol + '//' + window.location.host;

// Feature flags to enable/disable certain functionality based on server capabilities
const CONFIG = {
    SKIP_AUTHENTICATION: false,  // Skip token auth
    DEBUG_MODE: true            // Enable more verbose logging
};
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

// Helper function to handle network/CORS errors
function handleNetworkError(error, operation) {
    if (DEBUG) {
        console.error(`[JsonDB Error] ${operation} failed:`, error);
    }
    
    // Format a user-friendly error message
    let message = error.message || 'Unknown error';
    
    if (error.name === 'TypeError' && message.includes('NetworkError')) {
        return `Network connection error when ${operation}. The server may be unreachable or CORS may be blocking the request.`;
    }
    
    if (error.name === 'AbortError') {
        return `Request timeout when ${operation}. The server took too long to respond.`;
    }
    
    if (message.includes('CORS')) {
        return `CORS policy error when ${operation}. This is likely a server configuration issue.`;
    }
    
    return `Error ${operation}: ${message}`;
}

// Log initial configuration
debugLog('Window location:', window.location.toString());
debugLog('Hostname:', window.location.hostname);
debugLog('Host:', window.location.host);
debugLog('Protocol:', window.location.protocol);
debugLog('Final API Base URL:', API_BASE_URL);

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

    // Show loading state
    const submitBtn = document.querySelector('#login-form button[type="submit"]');
    const originalText = submitBtn.textContent;
    submitBtn.disabled = true;
    submitBtn.textContent = 'Logging in...';

    // Proceed directly to login
    fetch(`${API_BASE_URL}/api/admin/login?_t=${Date.now()}`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Accept': 'application/json'
        },
        body: JSON.stringify({ username, password }),
        mode: 'cors',
        credentials: 'same-origin'
    })
    .then(response => {
        debugLog('Login response status:', response.status);
        debugLog('Login response headers:', [...response.headers.entries()]);

        if (!response.ok) {
            throw new Error('Invalid credentials or server error');
        }
        return response.json();
    })
    .then(data => {
        debugLog('Login successful, response data:', data);

        // Store token (if available in the response)
        if (data && data.token) {
            authToken = data.token;
            localStorage.setItem(AUTH_TOKEN_KEY, authToken);
            debugLog('Saved auth token:', authToken);
        } else {
            // For development/testing, use a mock token if not provided
            authToken = 'mock-token';
            localStorage.setItem(AUTH_TOKEN_KEY, authToken);
            debugLog('Using mock token for development');
        }

        // Hide login modal
        loginModal.hide();

        // Initialize app
        initializeApp();
    })
    .catch(error => {
        debugLog('Login error:', error);
        
        // Show more detailed error
        let errorMsg = error.message;
        if (error.name === 'TypeError' && error.message.includes('NetworkError')) {
            errorMsg = `Network error - unable to connect to server at ${API_BASE_URL}`;
        }
        
        // Show error
        errorEl.textContent = errorMsg;
        errorEl.classList.remove('d-none');
    })
    .finally(() => {
        // Re-enable login button
        submitBtn.disabled = false;
        submitBtn.textContent = originalText;
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
function checkServerConnection() {
    debugLog('Checking server connection to', API_BASE_URL);
    
    // First try standard CORS approach
    return fetch(`${API_BASE_URL}/health?_t=${Date.now()}`, {
        method: 'GET',
        headers: {
            'Accept': 'application/json',
            'X-Requested-With': 'XMLHttpRequest'
        },
        mode: 'cors',
        credentials: 'omit' // Changed to 'omit' to be consistent
    })
    .then(response => {
        if (response.ok) {
            debugLog('Server connection check successful');
            return true;
        } else {
            debugLog('Server connection check failed with status:', response.status);
            return false;
        }
    })
    .catch(error => {
        debugLog('Standard CORS connection check failed, trying fallback approach:', error);
        
        // Fallback - try with no-cors mode which is more permissive but offers limited functionality
        return fetch(`${API_BASE_URL}/health?_t=${Date.now()}`, {
            method: 'GET',
            mode: 'no-cors'
        })
        .then(() => {
            // If we get here, the server is probably running but with CORS issues
            debugLog('Fallback connection check succeeded - server is likely running but has CORS configuration issues');
            showToast('Connected to server, but CORS issues detected. Limited functionality may be available.', 'warning');
            return true;
        })
        .catch(fallbackError => {
            debugLog('Fallback connection check also failed:', fallbackError);
            return false;
        });
    });
}

function initializeApp() {
    // Check server connection
    checkServerConnection()
        .then(connected => {
            if (!connected) {
                showToast('Warning: Could not connect to server. Some features may not work.', 'warning');
            }
        });
    
    // Load initial view
    loadView('dashboard');
    
    // Initialize JSON editor
    initializeJsonEditor();
    
    // Load data for dashboard
    loadDashboardData();
    
    // Add seed data if needed (only in development environments)
    if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') {
        addSeedDataIfNeeded();
    }
}

// Function to add seed data to collections if they are empty
function addSeedDataIfNeeded() {
    // Wait for authentication to complete
    setTimeout(() => {
        debugLog('Checking for seed data needs...');
        
        // Check if collections exist and contain data
        fetch(`${API_BASE_URL}/api/collections`, {
            headers: { 
                'Authorization': CONFIG.SKIP_AUTHENTICATION ? {} : `Bearer ${authToken}`,
                'Accept': 'application/json',
                'X-Requested-With': 'XMLHttpRequest'
            },
            mode: 'cors',
            credentials: 'omit'
        })
        .then(response => response.json())
        .then(data => {
            if (data.collections && data.collections.length > 0) {
                // Get the first collection to add data to
                const collection = data.collections[0];
                
                // Check if this collection has documents
                return fetch(`${API_BASE_URL}/api/collections/${collection}/documents`, {
                    headers: { 
                        'Authorization': CONFIG.SKIP_AUTHENTICATION ? {} : `Bearer ${authToken}`,
                        'Accept': 'application/json',
                        'X-Requested-With': 'XMLHttpRequest'
                    },
                    mode: 'cors',
                    credentials: 'omit'
                })
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`Failed to check documents in ${collection}`);
                    }
                    return response.text();
                })
                .then(text => {
                    let documentsData;
                    try {
                        documentsData = text ? JSON.parse(text) : { documents: [] };
                    } catch (e) {
                        documentsData = { documents: [] };
                    }
                    
                    // If no documents, add seed data
                    if (!documentsData.documents || documentsData.documents.length === 0) {
                        debugLog(`Adding seed data to ${collection}...`);
                        
                        // Create an array of promises for creating documents
                        const seedPromises = [
                            // User profile document
                            createDocument(collection, {
                                title: "User Profile",
                                name: "John Doe",
                                email: "john@example.com",
                                age: 30,
                                roles: ["admin", "editor"],
                                active: true,
                                created: new Date().toISOString()
                            }),
                            
                            // Product document
                            createDocument(collection, {
                                title: "Product",
                                sku: "PROD-001",
                                name: "Premium Widget",
                                price: 49.99,
                                stock: 100,
                                categories: ["electronics", "gadgets"],
                                details: {
                                    weight: "1.2kg",
                                    dimensions: "10x5x2cm",
                                    color: "silver"
                                }
                            }),
                            
                            // Order document
                            createDocument(collection, {
                                title: "Order",
                                order_number: "ORD-12345",
                                customer_id: "CUST-789",
                                date: new Date().toISOString(),
                                items: [
                                    { product_id: "PROD-001", quantity: 2, price: 49.99 },
                                    { product_id: "PROD-002", quantity: 1, price: 29.99 }
                                ],
                                shipping: {
                                    address: "123 Main St, Anytown, USA",
                                    method: "express",
                                    cost: 12.50
                                },
                                total: 142.47,
                                status: "processing"
                            })
                        ];
                        
                        // Return a promise that resolves when all documents are created
                        return Promise.all(seedPromises.map(p => p.catch(e => {
                            // Handle individual failures but continue
                            debugLog('Seed data creation error:', e);
                            return null;
                        })));
                    }
                    
                    return null; // No seed data needed
                });
            }
            
            return null; // No collections available
        })
        .then(() => {
            debugLog('Seed data check complete.');
        })
        .catch(error => {
            debugLog('Error in addSeedDataIfNeeded:', error);
        });
    }, 2000); // Wait 2 seconds after initialization
}

// Utility Functions
function formatDateTime(dateString) {
    if (!dateString) return 'N/A';
    
    // Handle both ISO strings and timestamps
    const date = typeof dateString === 'number' ? new Date(dateString) : new Date(dateString);
    
    // Check if date is valid
    if (isNaN(date.getTime())) return 'Invalid Date';
    
    // Format the date: YYYY-MM-DD HH:MM:SS
    return date.toISOString().replace('T', ' ').substr(0, 19);
}

function formatSize(bytes) {
    if (bytes === undefined || bytes === null) return 'N/A';
    if (bytes === 0) return '0 Bytes';
    
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function formatJsonPreview(json) {
    if (!json) return '';
    
    try {
        // Create a simplified preview by showing key-value pairs
        const keys = Object.keys(json).filter(key => !key.startsWith('_'));
        if (keys.length === 0) return '{}';
        
        // Show at most 3 key-value pairs
        const preview = keys.slice(0, 3).map(key => {
            // Simplify the value representation
            let value = json[key];
            if (value === null) return `"${key}": null`;
            if (typeof value === 'object') value = Array.isArray(value) ? '[...]' : '{...}';
            else if (typeof value === 'string') {
                if (value.length > 20) value = value.substring(0, 20) + '...';
                value = `"${value}"`;
            }
            return `"${key}": ${value}`;
        }).join(', ');
        
        // Indicate if there are more keys
        return keys.length > 3 ? `{ ${preview}, ... }` : `{ ${preview} }`;
    } catch (e) {
        console.error('Error formatting JSON preview:', e);
        return 'Error formatting JSON';
    }
}

// Show toast notification
function showToast(message, type = 'success') {
    // Create toast container if it doesn't exist
    let toastContainer = document.getElementById('toast-container');
    if (!toastContainer) {
        toastContainer = document.createElement('div');
        toastContainer.id = 'toast-container';
        toastContainer.className = 'position-fixed bottom-0 end-0 p-3';
        toastContainer.style.zIndex = '11';
        document.body.appendChild(toastContainer);
    }
    
    // Create a unique ID for this toast
    const toastId = 'toast-' + Date.now();
    
    // Determine the bootstrap class based on the type
    let bgClass = 'bg-success';
    if (type === 'error') bgClass = 'bg-danger';
    if (type === 'warning') bgClass = 'bg-warning';
    if (type === 'info') bgClass = 'bg-info';
    
    // Create the toast element
    const toastEl = document.createElement('div');
    toastEl.id = toastId;
    toastEl.className = `toast ${bgClass} text-white`;
    toastEl.setAttribute('role', 'alert');
    toastEl.setAttribute('aria-live', 'assertive');
    toastEl.setAttribute('aria-atomic', 'true');
    
    toastEl.innerHTML = `
        <div class="toast-header">
            <strong class="me-auto">JSON Database</strong>
            <button type="button" class="btn-close" data-bs-dismiss="toast" aria-label="Close"></button>
        </div>
        <div class="toast-body">
            ${message}
        </div>
    `;
    
    // Add the toast to the container
    toastContainer.appendChild(toastEl);
    
    // Initialize the toast
    const toast = new bootstrap.Toast(toastEl, {
        autohide: true,
        delay: 5000
    });
    
    // Show the toast
    toast.show();
    
    // Remove toast from DOM after it's hidden
    toastEl.addEventListener('hidden.bs.toast', function() {
        this.remove();
    });
    
    // If this is an error toast about modal forms, trigger automatic recovery
    if (type === 'error' && message.includes('form') && message.includes('not')) {
        debugLog('Triggering automatic modal recovery');
        
        // Try to recover the modal if it's visible
        setTimeout(() => {
            const actionModal = document.getElementById('actionModal');
            if (actionModal && actionModal.classList.contains('show')) {
                // Force-redraw the modal content
                const modalBody = actionModal.querySelector('.modal-body');
                if (modalBody) {
                    if (currentView === 'collections') {
                        showNewCollectionModal();
                    } else if (currentView === 'documents') {
                        showNewDocumentModal();
                    }
                }
            }
        }, 1000);
    }
}

// Initialize JSON editor
function initializeJsonEditor() {
    const container = document.getElementById('json-editor');
    if (!container) return;
    
    const options = {
        mode: 'tree',
        modes: ['tree', 'view', 'form', 'code', 'text'],
        onChange: function() {
            // Enable validation
        }
    };
    
    jsonEditor = new JSONEditor(container, options);
    jsonEditor.set({});
}

// Confirmation helpers
let confirmedAction = null;

function showConfirmationModal(message, callback) {
    // Set message
    document.getElementById('confirmation-message').textContent = message;
    
    // Store the callback
    confirmedAction = callback;
    
    // Show modal
    new bootstrap.Modal(document.getElementById('confirmationModal')).show();
}

function executeConfirmedAction() {
    // Hide modal
    const modalElement = document.getElementById('confirmationModal');
    const modal = bootstrap.Modal.getInstance(modalElement);
    modal.hide();
    
    // Execute callback if available
    if (typeof confirmedAction === 'function') {
        confirmedAction();
        confirmedAction = null;
    }
}

// Stub implementations for dashboard-related functions
function updateHealthChart(data) {
    const ctx = document.getElementById('health-chart');
    if (!ctx) return;
    
    // If Chart.js is available, create a simple chart
    if (window.Chart) {
        if (window.healthChart) {
            window.healthChart.destroy();
        }
        
        window.healthChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: ['System Health'],
                datasets: [{
                    label: 'CPU',
                    data: [data.cpu_usage || 0],
                    borderColor: 'rgba(75, 192, 192, 1)',
                    backgroundColor: 'rgba(75, 192, 192, 0.2)',
                }, {
                    label: 'Memory',
                    data: [data.memory_usage || 0],
                    borderColor: 'rgba(153, 102, 255, 1)',
                    backgroundColor: 'rgba(153, 102, 255, 0.2)',
                }, {
                    label: 'Disk',
                    data: [data.disk_usage || 0],
                    borderColor: 'rgba(255, 159, 64, 1)',
                    backgroundColor: 'rgba(255, 159, 64, 0.2)',
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: {
                        beginAtZero: true,
                        max: 100
                    }
                }
            }
        });
    }
}

function loadMetrics() {
    debugLog('Loading metrics (stub implementation)');
    // Implementation will be added in future
}

function loadUsers() {
    const tableBody = document.getElementById('users-table-body');
    if (tableBody) {
        tableBody.innerHTML = '<tr><td colspan="5" class="text-center">User management not implemented in this version</td></tr>';
    }
}

function loadRoles() {
    const tableBody = document.getElementById('roles-table-body');
    if (tableBody) {
        tableBody.innerHTML = '<tr><td colspan="5" class="text-center">Role management not implemented in this version</td></tr>';
    }
}

function loadSettings() {
    debugLog('Loading settings (stub implementation)');
    // Implementation will be added in future
}

function loadTransactionVisualization() {
    debugLog('Loading transaction visualization (stub implementation)');
    // Update transaction stats with placeholder data
    document.getElementById('transactions-total').textContent = '0';
    document.getElementById('transactions-committed').textContent = '0';
    document.getElementById('transactions-aborted').textContent = '0';
    document.getElementById('transactions-active').textContent = '0';
    document.getElementById('transactions-avg-duration').textContent = '0.00 sec';
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

// Fallback function to manually create collection without modal
function createCollectionDirectly() {
    debugLog('Using direct collection creation without modal');
    
    // Create an overlay div
    const overlay = document.createElement('div');
    overlay.style.position = 'fixed';
    overlay.style.top = '0';
    overlay.style.left = '0';
    overlay.style.width = '100%';
    overlay.style.height = '100%';
    overlay.style.backgroundColor = 'rgba(0, 0, 0, 0.5)';
    overlay.style.zIndex = '9999';
    overlay.style.display = 'flex';
    overlay.style.justifyContent = 'center';
    overlay.style.alignItems = 'center';
    
    // Create the form container
    const formContainer = document.createElement('div');
    formContainer.style.backgroundColor = 'white';
    formContainer.style.padding = '20px';
    formContainer.style.borderRadius = '5px';
    formContainer.style.maxWidth = '500px';
    formContainer.style.width = '80%';
    
    // Create form content
    formContainer.innerHTML = `
        <h3 style="margin-bottom: 20px;">Create New Collection</h3>
        <form id="direct-collection-form">
            <div style="margin-bottom: 15px;">
                <label style="display: block; margin-bottom: 5px; font-weight: bold;">Collection Name</label>
                <input type="text" id="direct-collection-name" style="width: 100%; padding: 8px; border: 1px solid #ced4da; border-radius: 4px;" required>
                <div style="font-size: 0.875em; color: #6c757d; margin-top: 5px;">
                    Names should contain only letters, numbers, and underscores.
                </div>
            </div>
            <div style="display: flex; justify-content: flex-end; gap: 10px; margin-top: 20px;">
                <button type="button" id="direct-cancel-btn" style="padding: 8px 16px; background-color: #6c757d; color: white; border: none; border-radius: 4px; cursor: pointer;">Cancel</button>
                <button type="submit" style="padding: 8px 16px; background-color: #0d6efd; color: white; border: none; border-radius: 4px; cursor: pointer;">Create Collection</button>
            </div>
        </form>
    `;
    
    // Add to DOM
    overlay.appendChild(formContainer);
    document.body.appendChild(overlay);
    
    // Focus the input
    setTimeout(() => {
        document.getElementById('direct-collection-name')?.focus();
    }, 100);
    
    // Cancel button handler
    document.getElementById('direct-cancel-btn').addEventListener('click', () => {
        document.body.removeChild(overlay);
    });
    
    // Form submission handler
    document.getElementById('direct-collection-form').addEventListener('submit', (event) => {
        event.preventDefault();
        
        const nameInput = document.getElementById('direct-collection-name');
        const name = nameInput.value.trim();
        
        if (!name) {
            alert('Collection name is required');
            return;
        }
        
        // Validate the name
        const namePattern = /^[a-zA-Z0-9_]+$/;
        if (!namePattern.test(name)) {
            alert('Collection name can only contain letters, numbers, and underscores');
            return;
        }
        
        // Submit button
        const submitBtn = event.target.querySelector('button[type="submit"]');
        submitBtn.disabled = true;
        submitBtn.textContent = 'Creating...';
        
        createCollection(name)
            .then(() => {
                document.body.removeChild(overlay);
                loadCollections();
                showToast(`Collection "${name}" created successfully`, 'success');
            })
            .catch(error => {
                alert(`Error creating collection: ${error.message}`);
            })
            .finally(() => {
                submitBtn.disabled = false;
                submitBtn.textContent = 'Create Collection';
            });
    });
}

// Handle action button clicks based on current view
function handleActionButton() {
    // Set modal title based on current view
    const actionModalLabel = document.getElementById('actionModalLabel');
    const modalBody = document.querySelector('#actionModal .modal-body');
    
    // Clear any previous content
    modalBody.innerHTML = '';
    
    // Prepare the modal content based on the current view before it's shown
    // Check if the URL has a 'direct=true' parameter to force direct mode
    const useDirectMode = window.location.search.includes('direct=true');
    
    // Special handling for collection creation
    if (currentView === 'collections' && useDirectMode) {
        // Skip modal and use direct creation
        createCollectionDirectly();
        return;
    }
    
    switch (currentView) {
        case 'collections':
            actionModalLabel.textContent = 'Create New Collection';
            showNewCollectionModal();
            
            // Add a fallback timeout - if the modal fails to show the form, provide a direct link
            setTimeout(() => {
                const form = document.getElementById('new-collection-form');
                if (!form && actionModal && actionModal.classList.contains('show')) {
                    const modalBody = actionModal.querySelector('.modal-body');
                    if (modalBody) {
                        modalBody.innerHTML = `
                            <div class="alert alert-warning">
                                <p>The collection form did not load properly.</p>
                                <button class="btn btn-primary mt-2" id="direct-creation-btn">
                                    Try Direct Creation Instead
                                </button>
                            </div>
                        `;
                        
                        document.getElementById('direct-creation-btn')?.addEventListener('click', () => {
                            const modalInstance = bootstrap.Modal.getInstance(actionModal);
                            if (modalInstance) {
                                modalInstance.hide();
                            }
                            createCollectionDirectly();
                        });
                    }
                }
            }, 2000);
            break;
        case 'documents':
            actionModalLabel.textContent = 'Create New Document';
            showNewDocumentModal();
            break;
        case 'users':
            actionModalLabel.textContent = 'Create New User';
            showNewUserModal();
            break;
        case 'roles':
            actionModalLabel.textContent = 'Create New Role';
            showNewRoleModal();
            break;
        default:
            // Default empty modal
            modalBody.innerHTML = '<div class="alert alert-warning">No action available for this view.</div>';
    }
    
    // Make sure the modal is properly initialized and shown
    const actionModal = document.getElementById('actionModal');
    
    // Delay showing the modal slightly to ensure content is ready
    setTimeout(() => {
        const bsModal = bootstrap.Modal.getInstance(actionModal) || new bootstrap.Modal(actionModal);
        bsModal.show();
        
        debugLog('Modal shown programmatically');
    }, 50);
}

// Show modal for creating a new collection
function showNewCollectionModal() {
    debugLog('Showing new collection modal');
    
    // First verify the modal is in the DOM
    const actionModal = document.getElementById('actionModal');
    if (!actionModal) {
        console.error('Action modal not found in DOM');
        alert('Error: Modal dialog not found in DOM. This might be a browser issue.');
        return;
    }
    
    const modalBody = document.querySelector('#actionModal .modal-body');
    if (!modalBody) {
        debugLog('Error: Modal body element not found');
        showToast('Error: UI element not found', 'error');
        return;
    }
    
    // Use a simple form with direct styling to ensure it shows correctly
    modalBody.innerHTML = `
        <form id="new-collection-form" class="p-2">
            <div class="form-group mb-3">
                <label for="collection-name" class="form-label fw-bold">Collection Name</label>
                <input type="text" class="form-control form-control-lg" id="collection-name" 
                       placeholder="Enter collection name" required autofocus>
                <div class="form-text text-muted mt-1">
                    Names should contain only letters, numbers, and underscores.
                </div>
            </div>
            
            <div class="d-flex justify-content-end mt-4">
                <button type="button" class="btn btn-secondary me-2" data-bs-dismiss="modal">Cancel</button>
                <button type="submit" class="btn btn-primary px-4">Create Collection</button>
            </div>
        </form>
    `;
    
    debugLog('Collection form HTML added to modal');
    
    // Set up a mutation observer to watch for any DOM changes to the modal body
    const observer = new MutationObserver((mutations) => {
        for (const mutation of mutations) {
            if (mutation.type === 'childList') {
                debugLog('Modal content changed by external code, re-adding our form');
                
                // If our form was removed, re-add it
                if (!document.getElementById('new-collection-form')) {
                    // Add form HTML again
                    modalBody.innerHTML = `
                        <form id="new-collection-form" class="p-2">
                            <div class="form-group mb-3">
                                <label for="collection-name" class="form-label fw-bold">Collection Name</label>
                                <input type="text" class="form-control form-control-lg" id="collection-name" 
                                    placeholder="Enter collection name" required autofocus>
                                <div class="form-text text-muted mt-1">
                                    Names should contain only letters, numbers, and underscores.
                                </div>
                            </div>
                            
                            <div class="d-flex justify-content-end mt-4">
                                <button type="button" class="btn btn-secondary me-2" data-bs-dismiss="modal">Cancel</button>
                                <button type="submit" class="btn btn-primary px-4">Create Collection</button>
                            </div>
                        </form>
                    `;
                    
                    // Re-setup the form
                    setupCollectionForm();
                }
            }
        }
    });
    
    // Start observing
    observer.observe(modalBody, { childList: true, subtree: true });
    
    // Function to set up the form event listeners
    function setupCollectionForm() {
        const form = document.getElementById('new-collection-form');
        const input = document.getElementById('collection-name');
        if (!form) {
            debugLog('Error: Form was not rendered');
            modalBody.innerHTML = `
                <div class="alert alert-danger">
                    Error: The form failed to render properly. Please try again.
                </div>
                <div class="d-flex justify-content-end mt-3">
                    <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Close</button>
                </div>
            `;
            return;
        }
        
        if (input) {
            // Automatically focus the input field
            input.focus();
        }
        
        // Add event listener for form submission
        form.addEventListener('submit', function(event) {
            event.preventDefault();
            
            const nameInput = document.getElementById('collection-name');
            if (!nameInput) {
                showToast('Error: Could not find the collection name input', 'error');
                return;
            }
            
            const name = nameInput.value.trim();
            if (!name) {
                showToast('Collection name is required', 'error');
                return;
            }
            
            // Validate the collection name format
            const namePattern = /^[a-zA-Z0-9_]+$/;
            if (!namePattern.test(name)) {
                showToast('Collection name can only contain letters, numbers, and underscores', 'error');
                return;
            }
            
            // Disable the submit button during creation
            const submitButton = this.querySelector('button[type="submit"]');
            const originalText = submitButton.textContent;
            submitButton.disabled = true;
            submitButton.textContent = 'Creating...';
            
            // Show a toast to indicate progress
            showToast(`Creating collection "${name}"...`, 'info');
            
            createCollection(name)
                .then(() => {
                    // Hide modal and refresh collections
                    const actionModal = document.getElementById('actionModal');
                    const modalInstance = bootstrap.Modal.getInstance(actionModal);
                    if (modalInstance) {
                        modalInstance.hide();
                    } else {
                        // If the modal instance is not found, hide manually
                        const bsModal = new bootstrap.Modal(actionModal);
                        bsModal.hide();
                    }
                    
                    loadCollections();
                    showToast(`Collection "${name}" created successfully`, 'success');
                })
                .catch(error => {
                    debugLog('Error creating collection:', error);
                    showToast(`Error creating collection: ${error.message}`, 'error');
                })
                .finally(() => {
                    // Re-enable the button
                    submitButton.disabled = false;
                    submitButton.textContent = originalText;
                });
        });
        
        debugLog('New collection form initialized');
    }
    
    // Set up the form initially
    setupCollectionForm();
    
    // Disconnect the observer when the modal is hidden
    const actionModalEl = document.getElementById('actionModal');
    actionModalEl.addEventListener('hidden.bs.modal', () => {
        observer.disconnect();
        debugLog('Mutation observer disconnected');
    });
}

// Show modal for creating a new document
function showNewDocumentModal() {
    // Make sure we have a selected collection
    if (!currentCollection) {
        const collectionSelector = document.getElementById('collection-selector');
        // If we're on the documents view but no collection is selected
        if (collectionSelector && collectionSelector.options.length > 1) {
            // Automatically select the first available collection
            collectionSelector.selectedIndex = 1;
            currentCollection = collectionSelector.value;
            debugLog(`Auto-selected collection: ${currentCollection}`);
        } else {
            showToast('Please select a collection first', 'error');
            const modalInstance = bootstrap.Modal.getInstance(document.getElementById('actionModal'));
            modalInstance.hide();
            return;
        }
    }
    
    const modalBody = document.querySelector('#actionModal .modal-body');
    modalBody.innerHTML = `
        <div id="new-document-editor" style="height: 400px;"></div>
        <div class="d-flex justify-content-end mt-3">
            <button type="button" class="btn btn-secondary me-2" data-bs-dismiss="modal">Cancel</button>
            <button type="button" class="btn btn-primary" id="create-document-btn">Create Document</button>
        </div>
    `;
    
    // Initialize JSON editor for new document
    const container = document.getElementById('new-document-editor');
    const options = {
        mode: 'tree',
        modes: ['tree', 'view', 'form', 'code', 'text'],
        onChange: function() {
            // Enable validation
        }
    };
    const editor = new JSONEditor(container, options, {});
    
    // Set default document with sample data
    const sampleData = {
        "title": "Sample Document",
        "description": "This is a sample document to help get you started.",
        "created": new Date().toISOString(),
        "tags": ["sample", "new", "template"],
        "details": {
            "priority": "medium",
            "status": "active"
        }
    };
    editor.set(sampleData);
    
    // Add event listener for form submission
    document.getElementById('create-document-btn').addEventListener('click', function() {
        try {
            // Get the document from the editor
            const docObj = editor.get();
            
            // Simple validation - check if document is empty
            if (docObj && Object.keys(docObj).length === 0) {
                showToast('Document cannot be empty', 'error');
                return;
            }
            
            // Disable create button during submission
            this.disabled = true;
            const originalText = this.textContent;
            this.textContent = 'Creating...';
            
            // First show a preview toast to give feedback
            showToast(`Creating document in ${currentCollection}...`, 'info');
            
            // Check if this is a simulation environment (occurs when server is down)
            const isSimulation = window.location.search.includes('simulate=true');
            
            if (isSimulation) {
                // Simulate server response with a slight delay
                setTimeout(() => {
                    showToast(`Document created with ID: sim_${Date.now()}`, 'success');
                    
                    // Hide modal
                    const modalInstance = bootstrap.Modal.getInstance(document.getElementById('actionModal'));
                    modalInstance.hide();
                    
                    // Add the document to the table manually
                    const tableBody = document.getElementById('documents-table-body');
                    if (tableBody) {
                        const row = document.createElement('tr');
                        const now = new Date().toISOString();
                        row.innerHTML = `
                            <td>sim_${Date.now()}</td>
                            <td class="text-truncate-2">${formatJsonPreview(docObj)}</td>
                            <td>${formatDateTime(now)}</td>
                            <td>${formatDateTime(now)}</td>
                            <td class="actions-column">
                                <button class="btn btn-sm btn-outline-primary view-document-btn" data-id="sim_${Date.now()}">
                                    <i class="bi bi-pencil-square"></i>
                                </button>
                                <button class="btn btn-sm btn-outline-danger delete-document-btn" data-id="sim_${Date.now()}">
                                    <i class="bi bi-trash"></i>
                                </button>
                            </td>
                        `;
                        
                        // If table shows "No documents" message, clear it first
                        if (tableBody.querySelector('td[colspan="5"]')) {
                            tableBody.innerHTML = '';
                        }
                        
                        tableBody.appendChild(row);
                        
                        // Update the document count
                        document.getElementById('documents-pagination-info').textContent = 
                            `Showing ${tableBody.querySelectorAll('tr').length} document(s)`;
                    }
                    
                    // Re-enable create button
                    this.disabled = false;
                    this.textContent = originalText;
                }, 1000);
                
                return;
            }
            
            createDocument(currentCollection, docObj)
                .then(data => {
                    debugLog('Document creation successful:', data);
                    
                    // Hide modal and refresh documents
                    const modalInstance = bootstrap.Modal.getInstance(document.getElementById('actionModal'));
                    modalInstance.hide();
                    
                    // Try to reload documents, but handle failures gracefully
                    loadDocuments(currentCollection)
                        .catch(loadError => {
                            debugLog('Error reloading documents after creation:', loadError);
                            showToast(`Document created, but couldn't refresh document list`, 'warning');
                            
                            // Add the document to the table manually
                            const tableBody = document.getElementById('documents-table-body');
                            if (tableBody) {
                                const row = document.createElement('tr');
                                const now = new Date().toISOString();
                                row.innerHTML = `
                                    <td>${data._id || 'new_doc_' + Date.now()}</td>
                                    <td class="text-truncate-2">${formatJsonPreview(docObj)}</td>
                                    <td>${formatDateTime(now)}</td>
                                    <td>${formatDateTime(now)}</td>
                                    <td class="actions-column">
                                        <button class="btn btn-sm btn-outline-primary view-document-btn" data-id="${data._id || 'new_doc_' + Date.now()}">
                                            <i class="bi bi-pencil-square"></i>
                                        </button>
                                        <button class="btn btn-sm btn-outline-danger delete-document-btn" data-id="${data._id || 'new_doc_' + Date.now()}">
                                            <i class="bi bi-trash"></i>
                                        </button>
                                    </td>
                                `;
                                
                                // If table shows "No documents" message, clear it first
                                if (tableBody.querySelector('td[colspan="5"]')) {
                                    tableBody.innerHTML = '';
                                }
                                
                                tableBody.appendChild(row);
                                
                                // Update the document count
                                document.getElementById('documents-pagination-info').textContent = 
                                    `Showing ${tableBody.querySelectorAll('tr').length} document(s)`;
                            }
                        });
                })
                .catch(error => {
                    debugLog('Error creating document:', error);
                    showToast(`Error creating document: ${error.message}`, 'error');
                    
                    // If the server is unresponsive, add the document to the UI anyway
                    // so the user can see something happened
                    if (error.message.includes('timed out') || error.message.includes('NetworkError')) {
                        showToast('Server timeout - document may have been created but confirmation failed', 'warning');
                        
                        // Hide modal
                        const modalInstance = bootstrap.Modal.getInstance(document.getElementById('actionModal'));
                        modalInstance.hide();
                        
                        // Add the document to the table manually with a temporary ID
                        const tableBody = document.getElementById('documents-table-body');
                        if (tableBody) {
                            const row = document.createElement('tr');
                            const now = new Date().toISOString();
                            row.innerHTML = `
                                <td>temp_${Date.now()}</td>
                                <td class="text-truncate-2">${formatJsonPreview(docObj)}</td>
                                <td>${formatDateTime(now)}</td>
                                <td>${formatDateTime(now)}</td>
                                <td class="actions-column">
                                    <button class="btn btn-sm btn-outline-primary view-document-btn" data-id="temp_${Date.now()}">
                                        <i class="bi bi-pencil-square"></i>
                                    </button>
                                    <button class="btn btn-sm btn-outline-danger delete-document-btn" data-id="temp_${Date.now()}">
                                        <i class="bi bi-trash"></i>
                                    </button>
                                </td>
                            `;
                            
                            // If table shows "No documents" message, clear it first
                            if (tableBody.querySelector('td[colspan="5"]')) {
                                tableBody.innerHTML = '';
                            }
                            
                            tableBody.appendChild(row);
                            
                            // Update the document count
                            document.getElementById('documents-pagination-info').textContent = 
                                `Showing ${tableBody.querySelectorAll('tr').length} document(s)`;
                        }
                    }
                })
                .finally(() => {
                    // Re-enable create button
                    this.disabled = false;
                    this.textContent = originalText;
                });
        } catch (e) {
            showToast('Invalid JSON: ' + e.message, 'error');
        }
    });
}

// Show modal for creating a new user (placeholder)
function showNewUserModal() {
    const modalBody = document.querySelector('#actionModal .modal-body');
    modalBody.innerHTML = `
        <form id="new-user-form">
            <div class="mb-3">
                <label for="username" class="form-label">Username</label>
                <input type="text" class="form-control" id="username" required>
            </div>
            <div class="mb-3">
                <label for="password" class="form-label">Password</label>
                <input type="password" class="form-control" id="password" required>
            </div>
            <div class="mb-3">
                <label for="role" class="form-label">Role</label>
                <select class="form-select" id="role">
                    <option value="admin">Admin</option>
                    <option value="user">User</option>
                    <option value="readonly">Read Only</option>
                </select>
            </div>
            <div class="d-flex justify-content-end">
                <button type="button" class="btn btn-secondary me-2" data-bs-dismiss="modal">Cancel</button>
                <button type="submit" class="btn btn-primary">Create User</button>
            </div>
        </form>
    `;
    
    // Add event listener for form submission (placeholder)
    document.getElementById('new-user-form').addEventListener('submit', function(event) {
        event.preventDefault();
        showToast('User management is not implemented in this version', 'info');
        const modalInstance = bootstrap.Modal.getInstance(document.getElementById('actionModal'));
        modalInstance.hide();
    });
}

// Show modal for creating a new role (placeholder)
function showNewRoleModal() {
    const modalBody = document.querySelector('#actionModal .modal-body');
    modalBody.innerHTML = `
        <form id="new-role-form">
            <div class="mb-3">
                <label for="role-name" class="form-label">Role Name</label>
                <input type="text" class="form-control" id="role-name" required>
            </div>
            <div class="mb-3">
                <label class="form-label">Permissions</label>
                <div class="form-check">
                    <input class="form-check-input" type="checkbox" id="perm-read">
                    <label class="form-check-label" for="perm-read">Read</label>
                </div>
                <div class="form-check">
                    <input class="form-check-input" type="checkbox" id="perm-write">
                    <label class="form-check-label" for="perm-write">Write</label>
                </div>
                <div class="form-check">
                    <input class="form-check-input" type="checkbox" id="perm-delete">
                    <label class="form-check-label" for="perm-delete">Delete</label>
                </div>
                <div class="form-check">
                    <input class="form-check-input" type="checkbox" id="perm-admin">
                    <label class="form-check-label" for="perm-admin">Admin</label>
                </div>
            </div>
            <div class="d-flex justify-content-end">
                <button type="button" class="btn btn-secondary me-2" data-bs-dismiss="modal">Cancel</button>
                <button type="submit" class="btn btn-primary">Create Role</button>
            </div>
        </form>
    `;
    
    // Add event listener for form submission (placeholder)
    document.getElementById('new-role-form').addEventListener('submit', function(event) {
        event.preventDefault();
        showToast('Role management is not implemented in this version', 'info');
        const modalInstance = bootstrap.Modal.getInstance(document.getElementById('actionModal'));
        modalInstance.hide();
    });
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
    debugLog('Loading collections...');
    
    // Show loading indicator
    const tableBody = document.getElementById('collections-table-body');
    tableBody.innerHTML = '<tr><td colspan="5" class="text-center">Loading collections...</td></tr>';
    
    // Add cache-busting and response timeout (increase to 30 seconds)
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 30000);
    
    fetch(`${API_BASE_URL}/api/collections?_t=${Date.now()}`, {
        method: 'GET',
        headers: { 
            'Authorization': CONFIG.SKIP_AUTHENTICATION ? {} : `Bearer ${authToken}`,
            'Accept': 'application/json',
            'X-Requested-With': 'XMLHttpRequest'
        },
        signal: controller.signal,
        mode: 'cors',
        credentials: 'omit' // Changed to 'omit' to be consistent
    })
    .catch(error => {
        clearTimeout(timeoutId);
        
        if (error.name === 'AbortError') {
            debugLog('Collection loading request timed out');
            tableBody.innerHTML = '<tr><td colspan="5" class="text-center">Request timed out. <button class="btn btn-link p-0" onclick="loadCollections()">Try again</button></td></tr>';
            showToast('Loading collections timed out after 30 seconds', 'error');
            throw new Error('Request timed out after 30 seconds');
        }
        throw error;
    })
    .then(response => {
        clearTimeout(timeoutId);
        
        debugLog(`Collections response status: ${response.status}`);
        
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to load collections');
            }).catch(e => {
                // Handle non-JSON responses
                throw new Error(`Failed to load collections (${response.status})`);
            });
        }
        return response.json();
    })
    .then(data => {
        displayCollections(data);
    })
    .catch(error => {
        debugLog('Error loading collections:', error);
        
        // Show error in table
        if (tableBody.innerHTML.indexOf('Loading') >= 0) {
            tableBody.innerHTML = `<tr><td colspan="5" class="text-center text-danger">
                Error loading collections: ${error.message}
                <button class="btn btn-link p-0 ms-2" onclick="loadCollections()">Try again</button>
            </td></tr>`;
        }
        
        showToast(`Error loading collections: ${error.message}`, 'error');
    });
}

// Display collections in the table
function displayCollections(data) {
    const tableBody = document.getElementById('collections-table-body');
    tableBody.innerHTML = '';
    
    debugLog('Collections to display:', data);
    
    if (data.collections && data.collections.length > 0) {
        data.collections.forEach(collection => {
            // Handle both string-only and object collection formats
            const collName = typeof collection === 'string' ? collection : collection.name;
            
            // Parse the collection object for display
            const displayData = {
                name: collName,
                documents_count: typeof collection === 'object' ? collection.documents_count || 0 : 0,
                size_bytes: typeof collection === 'object' ? collection.size_bytes || 0 : 0,
                last_modified: typeof collection === 'object' ? collection.last_modified || '' : ''
            };
            
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${displayData.name}</td>
                <td>${displayData.documents_count}</td>
                <td>${formatSize(displayData.size_bytes)}</td>
                <td>${formatDateTime(displayData.last_modified)}</td>
                <td class="actions-column">
                    <button class="btn btn-sm btn-outline-primary view-documents-btn" data-collection="${displayData.name}">
                        <i class="bi bi-eye"></i>
                    </button>
                    <button class="btn btn-sm btn-outline-danger delete-collection-btn" data-collection="${displayData.name}">
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
}

function loadCollectionSelector() {
    debugLog('Loading collection selector...');
    
    const selector = document.getElementById('collection-selector');
    
    // Keep only the first option and add a loading option
    while (selector.options.length > 1) {
        selector.remove(1);
    }
    
    const loadingOption = document.createElement('option');
    loadingOption.value = "";
    loadingOption.textContent = "Loading collections...";
    loadingOption.disabled = true;
    selector.appendChild(loadingOption);
    
    // Add cache-busting and response timeout (increase to 30 seconds)
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 30000);
    
    fetch(`${API_BASE_URL}/api/collections?_t=${Date.now()}`, {
        method: 'GET',
        headers: { 
            'Authorization': CONFIG.SKIP_AUTHENTICATION ? {} : `Bearer ${authToken}`,
            'Accept': 'application/json',
            'X-Requested-With': 'XMLHttpRequest'
        },
        signal: controller.signal,
        mode: 'cors',
        credentials: 'omit' // Changed to 'omit' to be consistent
    })
    .catch(error => {
        clearTimeout(timeoutId);
        
        if (error.name === 'AbortError') {
            debugLog('Collection selector loading request timed out');
            
            // Remove loading option
            if (selector.options.length > 1) {
                selector.remove(1);
            }
            
            // Add error option
            const errorOption = document.createElement('option');
            errorOption.value = "";
            errorOption.textContent = "Error loading collections (timeout)";
            errorOption.disabled = true;
            selector.appendChild(errorOption);
            
            showToast('Loading collections timed out after 30 seconds', 'error');
            throw new Error('Request timed out after 30 seconds');
        }
        throw error;
    })
    .then(response => {
        clearTimeout(timeoutId);
        
        debugLog(`Collection selector response status: ${response.status}`);
        
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to load collections');
            }).catch(e => {
                // Handle non-JSON responses
                throw new Error(`Failed to load collections (${response.status})`);
            });
        }
        return response.json();
    })
    .then(data => {
        debugLog('Collection selector data:', data);
        
        // Remove the loading option
        if (selector.options.length > 1) {
            selector.remove(1);
        }
        
        if (data.collections && data.collections.length > 0) {
            data.collections.forEach(collection => {
                // Handle both string-only and object collection formats
                const collName = typeof collection === 'string' ? collection : collection.name;
                
                const option = document.createElement('option');
                option.value = collName;
                option.textContent = collName;
                selector.appendChild(option);
            });
            
            // Set current collection if available
            if (currentCollection) {
                selector.value = currentCollection;
            }
        } else {
            // Add empty state option
            const emptyOption = document.createElement('option');
            emptyOption.value = "";
            emptyOption.textContent = "No collections available";
            emptyOption.disabled = true;
            selector.appendChild(emptyOption);
        }
    })
    .catch(error => {
        debugLog('Error loading collections for selector:', error);
        
        // Remove loading option
        if (selector.options.length > 1) {
            selector.remove(1);
        }
        
        // Add error option
        const errorOption = document.createElement('option');
        errorOption.value = "";
        errorOption.textContent = `Error: ${error.message}`;
        errorOption.disabled = true;
        selector.appendChild(errorOption);
        
        showToast(`Error loading collections: ${error.message}`, 'error');
    });
}

function loadDocuments(collection, query = '') {
    debugLog(`Loading documents from collection: ${collection}, query: ${query}`);
    
    const tableBody = document.getElementById('documents-table-body');
    tableBody.innerHTML = '<tr><td colspan="5" class="text-center">Loading documents...</td></tr>';

    // The API supports both ?query= parameter and full JSON query structure
    // For now, implement the simpler query parameter approach
    const url = query 
        ? `${API_BASE_URL}/api/collections/${collection}/documents?query=${encodeURIComponent(query)}`
        : `${API_BASE_URL}/api/collections/${collection}/documents`;
        
    // Add a timestamp to prevent caching issues
    const finalUrl = `${url}${url.includes('?') ? '&' : '?'}_t=${Date.now()}`;
    
    // Add debug logging
    debugLog(`Loading documents from: ${finalUrl}`);
    
    // Set a timeout to avoid hanging indefinitely (increased to 30 seconds)
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 30000);
    
    fetch(finalUrl, {
        method: 'GET',
        headers: { 
            'Authorization': CONFIG.SKIP_AUTHENTICATION ? {} : `Bearer ${authToken}`,
            'Accept': 'application/json',
            'X-Requested-With': 'XMLHttpRequest'
        },
        signal: controller.signal,
        mode: 'cors',
        credentials: 'omit' // Changed to 'omit' to be consistent
    })
    .catch(error => {
        // Clear the timeout 
        clearTimeout(timeoutId);
        
        if (error.name === 'AbortError') {
            debugLog('Request timed out after 30 seconds');
            return Promise.reject(new Error('Request timed out after 30 seconds'));
        }
        return Promise.reject(error);
    })
    .then(response => {
        // Clear the timeout on response
        clearTimeout(timeoutId);
        
        debugLog(`Load documents response status: ${response.status}`);
        
        if (!response.ok) {
            debugLog(`Error response: ${response.status} ${response.statusText}`);
            return response.json().then(errData => {
                throw new Error(errData.error || 'Failed to load documents');
            }).catch(e => {
                // If JSON parsing fails, throw a more specific error
                if (e instanceof SyntaxError) {
                    throw new Error(`Invalid response from server (${response.status}): Not a valid JSON response`);
                }
                throw new Error(`Failed to load documents (${response.status}): ${e.message}`);
            });
        }
        
        // Add extra handling for empty responses
        return response.text().then(text => {
            if (!text || text.trim() === '') {
                debugLog('Response body is empty, returning empty array');
                return { documents: [] };
            }
            
            try {
                return JSON.parse(text);
            } catch (err) {
                debugLog('Error parsing JSON response:', err, 'Text was:', text);
                throw new Error('Invalid JSON response from server: ' + err.message);
            }
        });
    })
    .then(data => {
        debugLog('Documents response:', data);
        displayDocuments(data, collection);
    })
    .catch(error => {
        debugLog(`Error loading documents for ${collection}:`, error);
        
        // Show error message
        const tableBody = document.getElementById('documents-table-body');
        tableBody.innerHTML = `<tr><td colspan="5" class="text-center text-danger">
            Error loading documents: ${error.message}
            <button class="btn btn-link p-0 ms-2" onclick="loadDocuments('${collection}')">Try again</button>
        </td></tr>`;
        
        document.getElementById('documents-pagination-info').textContent = 'Showing 0 of 0 documents';
        
        showToast(`Error loading documents: ${error.message}`, 'error');
    });
}

// Display documents in the table
function displayDocuments(data, collection) {
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
        document.getElementById('documents-pagination-info').textContent = `Showing ${documents.length} document(s)`;
    } else {
        tableBody.innerHTML = '<tr><td colspan="5" class="text-center">No documents found</td></tr>';
        document.getElementById('documents-pagination-info').textContent = 'Showing 0 documents';
    }
}

function loadDocument(collection, id) {
    // Add cache-busting
    const url = `${API_BASE_URL}/api/collections/${collection}/documents/${id}?_t=${Date.now()}`;
    
    // Add a controller for timeout
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 30000); // 30 second timeout
    
    fetch(url, {
        headers: { 
            'Authorization': CONFIG.SKIP_AUTHENTICATION ? {} : `Bearer ${authToken}`,
            'Accept': 'application/json',
            'X-Requested-With': 'XMLHttpRequest'
        },
        mode: 'cors',
        credentials: 'omit',
        signal: controller.signal
    })
    .catch(error => {
        clearTimeout(timeoutId);
        
        if (error.name === 'AbortError') {
            debugLog('Document load request timed out');
            throw new Error('Request timed out after 30 seconds');
        }
        throw error;
    })
    .then(response => {
        clearTimeout(timeoutId);
        
        if (!response.ok) {
            return response.json().then(errData => {
                throw new Error(errData.error || `Failed to load document (${response.status})`);
            }).catch(e => {
                throw new Error(`Server error when loading document: ${e.message}`);
            });
        }
        return response.json();
    })
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
    .catch(error => {
        console.error(`Error loading document ${id}:`, error);
        showToast(`Error loading document: ${error.message}`, 'error');
    });
}

// API Functions
function createCollection(name) {
    debugLog(`Creating collection with name: ${name}`);
    
    // Add cache-busting and response timeout (increased to 30 seconds)
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 30000);
    
    const url = `${API_BASE_URL}/api/collections?_t=${Date.now()}`;
    debugLog(`Making POST request to: ${url}`);
    
    return fetch(url, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Authorization': CONFIG.SKIP_AUTHENTICATION ? {} : `Bearer ${authToken}`,
            'Accept': 'application/json',
            'X-Requested-With': 'XMLHttpRequest' 
        },
        body: JSON.stringify({ name }),
        signal: controller.signal,
        // Try with simple cors mode and no credentials which may help with CORS issues
        mode: 'cors',
        credentials: 'omit' // Changed from 'include' to 'omit' to avoid credentials issues
    })
    .catch(error => {
        clearTimeout(timeoutId);
        debugLog('Collection creation fetch error:', error);
        
        // Try a fallback approach with no-cors mode for diagnostic purposes
        debugLog('Trying diagnostic connection test...');
        return fetch(`${API_BASE_URL}/health?_t=${Date.now()}`, { 
            method: 'GET',
            mode: 'no-cors'
        })
        .then(() => {
            // If health check works but actual request failed, it's likely a CORS/auth issue
            throw new Error('Server is reachable but collection creation failed - possible CORS or authentication issue');
        })
        .catch(() => {
            // If even health check fails, server is likely unreachable
            throw new Error(`Unable to connect to server at ${API_BASE_URL} - please check if it's running`);
        });
    })
    .then(response => {
        clearTimeout(timeoutId);
        
        debugLog(`Collection creation response status: ${response.status}`);
        debugLog(`Collection creation response headers:`, [...response.headers.entries()]);
        
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || `Failed to create collection (${response.status}: ${response.statusText})`);
            }).catch(e => {
                // If we can't parse the error as JSON, provide a generic error with the status
                throw new Error(`Server error: ${response.status} ${response.statusText}`);
            });
        }
        
        try {
            return response.json();
        } catch (e) {
            debugLog('Error parsing JSON response:', e);
            // If response cannot be parsed as JSON, return empty success
            return { name };
        }
    })
    .then(data => {
        debugLog('Collection creation response:', data);
        return data;
    });
}

function deleteCollection(name) {
    debugLog(`Deleting collection: ${name}`);
    
    // Add cache-busting and response timeout (increase to 30 seconds)
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 30000);
    
    return fetch(`${API_BASE_URL}/api/collections/${name}?_t=${Date.now()}`, {
        method: 'DELETE',
        headers: {
            'Authorization': CONFIG.SKIP_AUTHENTICATION ? {} : `Bearer ${authToken}`,
            'Accept': 'application/json',
            'X-Requested-With': 'XMLHttpRequest'
        },
        signal: controller.signal,
        mode: 'cors',
        credentials: 'omit'
    })
    .catch(error => {
        clearTimeout(timeoutId);
        
        if (error.name === 'AbortError') {
            debugLog('Collection deletion request timed out');
            showToast('Request timed out after 30 seconds', 'error');
            throw new Error('Request timed out after 30 seconds');
        }
        throw error;
    })
    .then(response => {
        clearTimeout(timeoutId);
        
        debugLog(`Collection deletion response status: ${response.status}`);
        
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to delete collection');
            }).catch(e => {
                // Handle non-JSON responses
                throw new Error(`Failed to delete collection (${response.status})`);
            });
        }
        
        // Refresh the collections list
        loadCollections();
        showToast(`Collection "${name}" deleted successfully`);
    })
    .catch(error => {
        console.error(`Error deleting collection ${name}:`, error);
        showToast(`Error deleting collection: ${error.message}`, 'error');
    });
}

function createDocument(collection, document) {
    debugLog(`Creating document in collection: ${collection}`);
    
    // Add cache-busting and response timeout (increase to 30 seconds)
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 30000);
    
    return fetch(`${API_BASE_URL}/api/collections/${collection}/documents?_t=${Date.now()}`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Authorization': CONFIG.SKIP_AUTHENTICATION ? {} : `Bearer ${authToken}`,
            'Accept': 'application/json',
            'X-Requested-With': 'XMLHttpRequest'
        },
        body: JSON.stringify(document),
        signal: controller.signal,
        mode: 'cors',
        credentials: 'omit'
    })
    .catch(error => {
        clearTimeout(timeoutId);
        
        if (error.name === 'AbortError') {
            debugLog('Document creation request timed out');
            throw new Error('Request timed out after 30 seconds');
        }
        throw error;
    })
    .then(response => {
        clearTimeout(timeoutId);
        
        debugLog(`Document creation response status: ${response.status}`);
        
        if (!response.ok) {
            return response.text().then(text => {
                if (text && text.trim() !== '') {
                    try {
                        const data = JSON.parse(text);
                        throw new Error(data.error || 'Failed to create document');
                    } catch (e) {
                        // If JSON parsing fails, use the raw text
                        throw new Error(`Failed to create document (${response.status}): ${text || response.statusText}`);
                    }
                } else {
                    throw new Error(`Failed to create document (${response.status}): ${response.statusText}`);
                }
            });
        }
        
        // Handle empty or invalid JSON responses
        return response.text().then(text => {
            if (!text || text.trim() === '') {
                // If server returns empty response but status is OK, assume success
                debugLog('Empty but successful response, creating default success object');
                return { 
                    _id: 'temp_' + Date.now(),
                    success: true,
                    message: 'Document created successfully'
                };
            }
            
            try {
                return JSON.parse(text);
            } catch (err) {
                debugLog('Error parsing JSON response:', err, 'Text was:', text);
                // Return a default object if we can't parse the response
                return { 
                    _id: 'temp_' + Date.now(),
                    success: true,
                    message: 'Document created successfully (response parsing error)'
                };
            }
        });
    })
    .then(data => {
        debugLog('Document creation response:', data);
        showToast(`Document created with ID: ${data._id || 'unknown'}`);
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
        
        // Simple validation - check if document is empty
        if (document && Object.keys(document).length === 0) {
            showToast('Document cannot be empty', 'error');
            return;
        }
        
        // Disable save button during submission
        const saveBtn = document.getElementById('save-document-btn');
        const originalText = saveBtn.textContent;
        saveBtn.disabled = true;
        saveBtn.textContent = 'Saving...';
        
        // Add cache-busting and response timeout (increase to 30 seconds for consistency)
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 30000);
        
        // Update document
        fetch(`${API_BASE_URL}/api/collections/${collection}/documents/${id}?_t=${Date.now()}`, {
            method: 'PUT',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': CONFIG.SKIP_AUTHENTICATION ? {} : `Bearer ${authToken}`,
                'Accept': 'application/json',
                'X-Requested-With': 'XMLHttpRequest'
            },
            body: JSON.stringify(document),
            signal: controller.signal,
            mode: 'cors',
            credentials: 'omit'
        })
        .catch(error => {
            clearTimeout(timeoutId);
            
            if (error.name === 'AbortError') {
                debugLog('Document update request timed out');
                throw new Error('Request timed out after 30 seconds');
            }
            throw error;
        })
        .then(response => {
            clearTimeout(timeoutId);
            
            debugLog(`Document update response status: ${response.status}`);
            
            if (!response.ok) {
                return response.json().then(data => {
                    throw new Error(data.error || 'Failed to update document');
                }).catch(e => {
                    // Handle non-JSON responses
                    throw new Error(`Failed to update document (${response.status})`);
                });
            }
            return response.json();
        })
        .then(() => {
            showToast('Document updated successfully');
            const modalInstance = bootstrap.Modal.getInstance(documentViewModal) || new bootstrap.Modal(documentViewModal);
            modalInstance.hide();
            loadDocuments(collection);
        })
        .catch(error => {
            debugLog('Error saving document:', error);
            showToast(`Error saving document: ${error.message}`, 'error');
        })
        .finally(() => {
            // Re-enable save button
            saveBtn.disabled = false;
            saveBtn.textContent = originalText;
        });
    } catch (e) {
        showToast('Invalid JSON: ' + e.message, 'error');
    }
}

function deleteDocument(collection, id) {
    debugLog(`Deleting document ${id} from collection ${collection}`);
    
    // Add cache-busting and response timeout (increase to 30 seconds)
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 30000);
    
    return fetch(`${API_BASE_URL}/api/collections/${collection}/documents/${id}?_t=${Date.now()}`, {
        method: 'DELETE',
        headers: {
            'Authorization': CONFIG.SKIP_AUTHENTICATION ? {} : `Bearer ${authToken}`,
            'Accept': 'application/json',
            'X-Requested-With': 'XMLHttpRequest'
        },
        signal: controller.signal,
        mode: 'cors',
        credentials: 'omit'
    })
    .catch(error => {
        clearTimeout(timeoutId);
        
        if (error.name === 'AbortError') {
            debugLog('Document deletion request timed out');
            showToast('Request timed out after 30 seconds', 'error');
            throw new Error('Request timed out after 30 seconds');
        }
        throw error;
    })
    .then(response => {
        clearTimeout(timeoutId);
        
        debugLog(`Document deletion response status: ${response.status}`);
        
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.error || 'Failed to delete document');
            }).catch(e => {
                // Handle non-JSON responses
                throw new Error(`Failed to delete document (${response.status})`);
            });
        }
        
        loadDocuments(collection);
        showToast('Document deleted successfully');
    })
    .catch(error => {
        debugLog(`Error deleting document ${id}:`, error);
        showToast(`Error deleting document: ${error.message}`, 'error');
    });
}