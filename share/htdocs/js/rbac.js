// RBAC Management JavaScript
const API_BASE_URL = '';
let authToken = localStorage.getItem('jsondb_auth_token');
let allUsers = [];
let allRoles = [];
let allPermissions = [];

// Check authentication
if (!authToken) {
    window.location.href = '/login.html';
}

// Initialize
document.addEventListener('DOMContentLoaded', function() {
    loadUsers();
    loadRoles();
    loadPermissionMatrix();
    
    // Setup search functionality
    document.getElementById('userSearch').addEventListener('input', filterUsers);
    document.getElementById('roleSearch').addEventListener('input', filterRoles);
});

// Logout function
function logout() {
    localStorage.removeItem('jsondb_auth_token');
    window.location.href = '/login.html';
}

// API helper
async function apiRequest(endpoint, options = {}) {
    const defaultOptions = {
        headers: {
            'Authorization': `Bearer ${authToken}`,
            'Content-Type': 'application/json'
        }
    };
    
    const response = await fetch(`${API_BASE_URL}${endpoint}`, {
        ...defaultOptions,
        ...options,
        headers: {
            ...defaultOptions.headers,
            ...options.headers
        }
    });
    
    if (!response.ok) {
        if (response.status === 401) {
            // Token expired or invalid
            localStorage.removeItem('jsondb_auth_token');
            localStorage.removeItem('jsondb_refresh_token');
            window.location.href = '/login.html';
            return;
        }
        const error = await response.json();
        throw new Error(error.error || 'API request failed');
    }
    
    return response.json();
}

// Load users
async function loadUsers() {
    try {
        const data = await apiRequest('/api/rbac/users');
        allUsers = data.users || [];
        renderUsers();
    } catch (error) {
        console.error('Error loading users:', error);
        showAlert('Error loading users', 'danger');
    }
}

// Render users table
function renderUsers() {
    const tbody = document.getElementById('usersTable');
    const searchTerm = document.getElementById('userSearch').value.toLowerCase();
    
    const filteredUsers = allUsers.filter(user => 
        user.username.toLowerCase().includes(searchTerm) ||
        (user.email && user.email.toLowerCase().includes(searchTerm))
    );
    
    tbody.innerHTML = filteredUsers.map(user => `
        <tr>
            <td><span class="status-badge status-active"></span></td>
            <td><strong>${user.username}</strong></td>
            <td>${user.email || 'N/A'}</td>
            <td>
                ${(user.roles || []).map(role => 
                    `<span class="badge bg-primary me-1">${role}</span>`
                ).join('')}
            </td>
            <td>${formatDate(user.created_at)}</td>
            <td>
                <button class="btn btn-sm btn-outline-primary" onclick="editUser('${user.id}')">
                    <i class="bi bi-pencil"></i>
                </button>
                <button class="btn btn-sm btn-outline-danger" onclick="deleteUser('${user.id}')">
                    <i class="bi bi-trash"></i>
                </button>
            </td>
        </tr>
    `).join('');
}

// Load roles
async function loadRoles() {
    try {
        const data = await apiRequest('/api/rbac/roles');
        allRoles = data.roles || [];
        renderRoles();
        updateRoleSelects();
    } catch (error) {
        console.error('Error loading roles:', error);
        showAlert('Error loading roles', 'danger');
    }
}

// Render roles table
function renderRoles() {
    const tbody = document.getElementById('rolesTable');
    const searchTerm = document.getElementById('roleSearch').value.toLowerCase();
    
    const filteredRoles = allRoles.filter(role => 
        role.name.toLowerCase().includes(searchTerm) ||
        (role.description && role.description.toLowerCase().includes(searchTerm))
    );
    
    tbody.innerHTML = filteredRoles.map(role => `
        <tr>
            <td><strong>${role.name}</strong></td>
            <td>${role.description || 'N/A'}</td>
            <td>${role.user_count || 0}</td>
            <td>${Object.keys(role.permissions || {}).length} permissions</td>
            <td>
                <button class="btn btn-sm btn-outline-primary" onclick="editRole('${role.id}')">
                    <i class="bi bi-pencil"></i>
                </button>
                ${role.name !== 'admin' ? `
                    <button class="btn btn-sm btn-outline-danger" onclick="deleteRole('${role.id}')">
                        <i class="bi bi-trash"></i>
                    </button>
                ` : ''}
            </td>
        </tr>
    `).join('');
}

// Load permission matrix
async function loadPermissionMatrix() {
    try {
        // Get all collections for resources
        const collectionsData = await apiRequest('/api/collections');
        const collections = collectionsData.collections || [];
        
        // Define permission types
        const permissionTypes = ['read', 'write', 'delete'];
        
        // Build matrix HTML
        let matrixHTML = `
            <div class="table-responsive">
                <table class="table table-bordered permission-matrix-table">
                    <thead>
                        <tr>
                            <th style="width: 150px;">Role</th>
                            ${collections.map(col => 
                                `<th colspan="${permissionTypes.length}" class="text-center">${col}</th>`
                            ).join('')}
                        </tr>
                        <tr>
                            <th></th>
                            ${collections.map(() => 
                                permissionTypes.map(type => 
                                    `<th class="text-center permission-type">${type}</th>`
                                ).join('')
                            ).join('')}
                        </tr>
                    </thead>
                    <tbody>
                        ${allRoles.map(role => `
                            <tr>
                                <td><strong>${role.name}</strong></td>
                                ${collections.map(collection => 
                                    permissionTypes.map(type => {
                                        const permKey = `${collection}:${type}`;
                                        const hasPermission = role.permissions && 
                                            (role.permissions[permKey] || 
                                             role.permissions[`${collection}:*`] || 
                                             role.permissions['*:*']);
                                        return `
                                            <td class="text-center">
                                                <input type="checkbox" 
                                                    class="form-check-input"
                                                    data-role="${role.id}"
                                                    data-collection="${collection}"
                                                    data-permission="${type}"
                                                    ${hasPermission ? 'checked' : ''}
                                                    ${role.name === 'admin' ? 'disabled' : ''}>
                                            </td>
                                        `;
                                    }).join('')
                                ).join('')}
                            </tr>
                        `).join('')}
                    </tbody>
                </table>
            </div>
        `;
        
        document.getElementById('permissionMatrix').innerHTML = matrixHTML;
    } catch (error) {
        console.error('Error loading permission matrix:', error);
        showAlert('Error loading permission matrix', 'danger');
    }
}

// Save permissions from matrix
async function savePermissions() {
    try {
        // Collect all permission changes
        const updates = [];
        const checkboxes = document.querySelectorAll('#permissionMatrix input[type="checkbox"]:not(:disabled)');
        
        // Group by role
        const rolePermissions = {};
        checkboxes.forEach(checkbox => {
            const roleId = checkbox.dataset.role;
            const collection = checkbox.dataset.collection;
            const permission = checkbox.dataset.permission;
            
            if (!rolePermissions[roleId]) {
                rolePermissions[roleId] = {};
            }
            
            if (checkbox.checked) {
                rolePermissions[roleId][`${collection}:${permission}`] = true;
            }
        });
        
        // Update each role
        for (const [roleId, permissions] of Object.entries(rolePermissions)) {
            await apiRequest(`/api/rbac/roles/${roleId}`, {
                method: 'PUT',
                body: JSON.stringify({ permissions })
            });
        }
        
        showAlert('Permissions updated successfully', 'success');
        loadRoles(); // Reload to refresh the data
    } catch (error) {
        console.error('Error saving permissions:', error);
        showAlert('Error saving permissions', 'danger');
    }
}

// Show create user modal
function showCreateUserModal() {
    document.getElementById('userModalTitle').textContent = 'Create User';
    document.getElementById('userForm').reset();
    document.getElementById('userId').value = '';
    document.getElementById('passwordGroup').style.display = 'block';
    
    const modal = new bootstrap.Modal(document.getElementById('userModal'));
    modal.show();
}

// Edit user
async function editUser(userId) {
    try {
        const user = allUsers.find(u => u.id === userId);
        if (!user) return;
        
        document.getElementById('userModalTitle').textContent = 'Edit User';
        document.getElementById('userId').value = user.id;
        document.getElementById('username').value = user.username;
        document.getElementById('email').value = user.email || '';
        
        // Select user roles
        const roleSelect = document.getElementById('userRoles');
        Array.from(roleSelect.options).forEach(option => {
            option.selected = user.roles && user.roles.includes(option.value);
        });
        
        const modal = new bootstrap.Modal(document.getElementById('userModal'));
        modal.show();
    } catch (error) {
        console.error('Error editing user:', error);
        showAlert('Error loading user details', 'danger');
    }
}

// Save user
async function saveUser() {
    try {
        const userId = document.getElementById('userId').value;
        const userData = {
            username: document.getElementById('username').value,
            email: document.getElementById('email').value,
            roles: Array.from(document.getElementById('userRoles').selectedOptions).map(opt => opt.value)
        };
        
        const password = document.getElementById('password').value;
        if (password) {
            userData.password = password;
        }
        
        if (userId) {
            // Update existing user
            await apiRequest(`/api/rbac/users/${userId}`, {
                method: 'PUT',
                body: JSON.stringify(userData)
            });
        } else {
            // Create new user
            if (!password) {
                showAlert('Password is required for new users', 'warning');
                return;
            }
            await apiRequest('/api/rbac/users', {
                method: 'POST',
                body: JSON.stringify(userData)
            });
        }
        
        bootstrap.Modal.getInstance(document.getElementById('userModal')).hide();
        showAlert(`User ${userId ? 'updated' : 'created'} successfully`, 'success');
        loadUsers();
    } catch (error) {
        console.error('Error saving user:', error);
        showAlert('Error saving user', 'danger');
    }
}

// Delete user
async function deleteUser(userId) {
    if (!confirm('Are you sure you want to delete this user?')) return;
    
    try {
        await apiRequest(`/api/rbac/users/${userId}`, {
            method: 'DELETE'
        });
        showAlert('User deleted successfully', 'success');
        loadUsers();
    } catch (error) {
        console.error('Error deleting user:', error);
        showAlert('Error deleting user', 'danger');
    }
}

// Show create role modal
function showCreateRoleModal() {
    document.getElementById('roleModalTitle').textContent = 'Create Role';
    document.getElementById('roleForm').reset();
    document.getElementById('roleId').value = '';
    loadRolePermissions();
    
    const modal = new bootstrap.Modal(document.getElementById('roleModal'));
    modal.show();
}

// Edit role
async function editRole(roleId) {
    try {
        const role = allRoles.find(r => r.id === roleId);
        if (!role) return;
        
        document.getElementById('roleModalTitle').textContent = 'Edit Role';
        document.getElementById('roleId').value = role.id;
        document.getElementById('roleName').value = role.name;
        document.getElementById('roleDescription').value = role.description || '';
        
        loadRolePermissions(role.permissions);
        
        const modal = new bootstrap.Modal(document.getElementById('roleModal'));
        modal.show();
    } catch (error) {
        console.error('Error editing role:', error);
        showAlert('Error loading role details', 'danger');
    }
}

// Load permissions for role modal
async function loadRolePermissions(existingPermissions = {}) {
    try {
        const collectionsData = await apiRequest('/api/collections');
        const collections = collectionsData.collections || [];
        
        const permissionsHtml = collections.map(collection => `
            <div class="permission-item mb-2">
                <h6>${collection}</h6>
                <div class="form-check form-check-inline">
                    <input class="form-check-input" type="checkbox" 
                        id="perm_${collection}_read" 
                        value="${collection}:read"
                        ${existingPermissions[`${collection}:read`] ? 'checked' : ''}>
                    <label class="form-check-label" for="perm_${collection}_read">Read</label>
                </div>
                <div class="form-check form-check-inline">
                    <input class="form-check-input" type="checkbox" 
                        id="perm_${collection}_write" 
                        value="${collection}:write"
                        ${existingPermissions[`${collection}:write`] ? 'checked' : ''}>
                    <label class="form-check-label" for="perm_${collection}_write">Write</label>
                </div>
                <div class="form-check form-check-inline">
                    <input class="form-check-input" type="checkbox" 
                        id="perm_${collection}_delete" 
                        value="${collection}:delete"
                        ${existingPermissions[`${collection}:delete`] ? 'checked' : ''}>
                    <label class="form-check-label" for="perm_${collection}_delete">Delete</label>
                </div>
            </div>
        `).join('');
        
        document.getElementById('rolePermissions').innerHTML = permissionsHtml;
    } catch (error) {
        console.error('Error loading permissions:', error);
    }
}

// Save role
async function saveRole() {
    try {
        const roleId = document.getElementById('roleId').value;
        const permissions = {};
        
        // Collect selected permissions
        document.querySelectorAll('#rolePermissions input:checked').forEach(checkbox => {
            permissions[checkbox.value] = true;
        });
        
        const roleData = {
            name: document.getElementById('roleName').value,
            description: document.getElementById('roleDescription').value,
            permissions: permissions
        };
        
        if (roleId) {
            // Update existing role
            await apiRequest(`/api/rbac/roles/${roleId}`, {
                method: 'PUT',
                body: JSON.stringify(roleData)
            });
        } else {
            // Create new role
            await apiRequest('/api/rbac/roles', {
                method: 'POST',
                body: JSON.stringify(roleData)
            });
        }
        
        bootstrap.Modal.getInstance(document.getElementById('roleModal')).hide();
        showAlert(`Role ${roleId ? 'updated' : 'created'} successfully`, 'success');
        loadRoles();
        loadPermissionMatrix();
    } catch (error) {
        console.error('Error saving role:', error);
        showAlert('Error saving role', 'danger');
    }
}

// Delete role
async function deleteRole(roleId) {
    if (!confirm('Are you sure you want to delete this role?')) return;
    
    try {
        await apiRequest(`/api/rbac/roles/${roleId}`, {
            method: 'DELETE'
        });
        showAlert('Role deleted successfully', 'success');
        loadRoles();
        loadPermissionMatrix();
    } catch (error) {
        console.error('Error deleting role:', error);
        showAlert('Error deleting role', 'danger');
    }
}

// Update role selects
function updateRoleSelects() {
    const select = document.getElementById('userRoles');
    select.innerHTML = allRoles.map(role => 
        `<option value="${role.name}">${role.name}</option>`
    ).join('');
}

// Filter functions
function filterUsers() {
    renderUsers();
}

function filterRoles() {
    renderRoles();
}

// Show alert
function showAlert(message, type = 'info') {
    const alertDiv = document.createElement('div');
    alertDiv.className = `alert alert-${type} alert-dismissible fade show position-fixed top-0 start-50 translate-middle-x mt-3`;
    alertDiv.style.zIndex = '9999';
    alertDiv.innerHTML = `
        ${message}
        <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
    `;
    document.body.appendChild(alertDiv);
    
    // Auto-dismiss after 3 seconds
    setTimeout(() => {
        alertDiv.remove();
    }, 3000);
}

// Format date
function formatDate(dateString) {
    if (!dateString) return 'N/A';
    const date = new Date(dateString);
    return date.toLocaleDateString();
}