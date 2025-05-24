// RBAC Management JavaScript
const API_BASE_URL = '';
let authToken = localStorage.getItem('jsondb_auth_token');
let allUsers = [];
let allRoles = [];
let permissionResources = ['users', 'roles', 'collections', 'documents', 'system', 'metrics'];
let permissionActions = ['create', 'read', 'update', 'delete'];

// Check authentication
if (!authToken) {
    window.location.href = '/login.html';
}

// Initialize
document.addEventListener('DOMContentLoaded', function() {
    loadUsers();
    loadRoles();
    
    // Setup search filters
    document.getElementById('userSearch').addEventListener('input', filterUsers);
    document.getElementById('roleSearch').addEventListener('input', filterRoles);
    
    // Setup tab change handlers
    document.getElementById('roles-tab').addEventListener('shown.bs.tab', loadRoles);
    document.getElementById('permissions-tab').addEventListener('shown.bs.tab', loadPermissionMatrix);
});

// Logout function
function logout() {
    localStorage.removeItem('jsondb_auth_token');
    window.location.href = '/login.html';
}

// Show toast notification
function showToast(message, type = 'success') {
    const toastHtml = `
        <div class="toast align-items-center text-white bg-${type === 'error' ? 'danger' : 'success'}" role="alert">
            <div class="d-flex">
                <div class="toast-body">
                    ${message}
                </div>
                <button type="button" class="btn-close btn-close-white me-2 m-auto" data-bs-dismiss="toast"></button>
            </div>
        </div>
    `;
    
    const toastElement = document.createElement('div');
    toastElement.innerHTML = toastHtml;
    document.querySelector('.toast-container').appendChild(toastElement.firstElementChild);
    
    const toast = new bootstrap.Toast(toastElement.firstElementChild);
    toast.show();
    
    setTimeout(() => {
        toastElement.firstElementChild.remove();
    }, 5000);
}

// API helper function
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
        const error = await response.json();
        throw new Error(error.error || 'API request failed');
    }
    
    return response.json();
}

// User Management Functions
async function loadUsers() {
    try {
        // First get users from _users collection
        const usersResponse = await apiRequest('/api/collections/_users/documents');
        allUsers = usersResponse.documents || [];
        
        // Also get users from RBAC API
        try {
            const rbacUsers = await apiRequest('/api/users');
            // Merge RBAC users with collection users
            if (rbacUsers.users) {
                rbacUsers.users.forEach(rbacUser => {
                    if (!allUsers.find(u => u.username === rbacUser.username)) {
                        allUsers.push(rbacUser);
                    }
                });
            }
        } catch (e) {
            console.log('RBAC users endpoint not available');
        }
        
        displayUsers(allUsers);
    } catch (error) {
        console.error('Error loading users:', error);
        showToast('Failed to load users', 'error');
    }
}

function displayUsers(users) {
    const tbody = document.getElementById('usersTable');
    tbody.innerHTML = '';
    
    users.forEach(user => {
        const row = document.createElement('tr');
        row.innerHTML = `
            <td><span class="status-indicator status-active"></span></td>
            <td>${user.username || ''}</td>
            <td>${user.email || '-'}</td>
            <td>${formatRoles(user.roles)}</td>
            <td>${formatDate(user.created_at)}</td>
            <td>
                <div class="action-buttons">
                    <button class="btn btn-sm btn-outline-primary" onclick="editUser('${user._id || user.id}')">
                        <i class="bi bi-pencil"></i>
                    </button>
                    <button class="btn btn-sm btn-outline-danger" onclick="deleteUser('${user._id || user.id}')">
                        <i class="bi bi-trash"></i>
                    </button>
                </div>
            </td>
        `;
        tbody.appendChild(row);
    });
}

function formatRoles(roles) {
    if (!roles || roles.length === 0) return '<span class="badge bg-secondary">No roles</span>';
    return roles.map(role => `<span class="badge bg-primary me-1">${role}</span>`).join('');
}

function formatDate(dateString) {
    if (!dateString) return '-';
    const date = new Date(dateString);
    return date.toLocaleDateString();
}

function filterUsers() {
    const search = document.getElementById('userSearch').value.toLowerCase();
    const filtered = allUsers.filter(user => 
        user.username.toLowerCase().includes(search) ||
        (user.email && user.email.toLowerCase().includes(search))
    );
    displayUsers(filtered);
}

function showCreateUserModal() {
    document.getElementById('userModalTitle').textContent = 'Create User';
    document.getElementById('userForm').reset();
    document.getElementById('userId').value = '';
    document.getElementById('passwordGroup').style.display = 'block';
    loadRolesForSelect();
    new bootstrap.Modal(document.getElementById('userModal')).show();
}

async function editUser(userId) {
    try {
        const user = allUsers.find(u => u._id === userId || u.id === userId);
        if (!user) return;
        
        document.getElementById('userModalTitle').textContent = 'Edit User';
        document.getElementById('userId').value = userId;
        document.getElementById('username').value = user.username;
        document.getElementById('email').value = user.email || '';
        document.getElementById('password').value = '';
        
        await loadRolesForSelect();
        
        // Set selected roles
        const select = document.getElementById('userRoles');
        Array.from(select.options).forEach(option => {
            option.selected = user.roles && user.roles.includes(option.value);
        });
        
        new bootstrap.Modal(document.getElementById('userModal')).show();
    } catch (error) {
        console.error('Error editing user:', error);
        showToast('Failed to load user details', 'error');
    }
}

async function loadRolesForSelect() {
    try {
        const select = document.getElementById('userRoles');
        select.innerHTML = '';
        
        allRoles.forEach(role => {
            const option = document.createElement('option');
            option.value = role.name || role.id;
            option.textContent = role.name || role.id;
            select.appendChild(option);
        });
    } catch (error) {
        console.error('Error loading roles for select:', error);
    }
}

async function saveUser() {
    try {
        const userId = document.getElementById('userId').value;
        const username = document.getElementById('username').value;
        const email = document.getElementById('email').value;
        const password = document.getElementById('password').value;
        const selectedRoles = Array.from(document.getElementById('userRoles').selectedOptions)
            .map(opt => opt.value);
        
        const userData = {
            username,
            email,
            roles: selectedRoles
        };
        
        if (password) {
            userData.password = password;
        }
        
        if (userId) {
            // Update existing user
            await apiRequest(`/api/users/${userId}`, {
                method: 'PUT',
                body: JSON.stringify(userData)
            });
            showToast('User updated successfully');
        } else {
            // Create new user
            if (!password) {
                showToast('Password is required for new users', 'error');
                return;
            }
            await apiRequest('/api/users', {
                method: 'POST',
                body: JSON.stringify(userData)
            });
            showToast('User created successfully');
        }
        
        bootstrap.Modal.getInstance(document.getElementById('userModal')).hide();
        loadUsers();
    } catch (error) {
        console.error('Error saving user:', error);
        showToast(error.message || 'Failed to save user', 'error');
    }
}

async function deleteUser(userId) {
    if (!confirm('Are you sure you want to delete this user?')) return;
    
    try {
        await apiRequest(`/api/users/${userId}`, {
            method: 'DELETE'
        });
        showToast('User deleted successfully');
        loadUsers();
    } catch (error) {
        console.error('Error deleting user:', error);
        showToast('Failed to delete user', 'error');
    }
}

// Role Management Functions
async function loadRoles() {
    try {
        // First get roles from _roles collection
        const rolesResponse = await apiRequest('/api/collections/_roles/documents');
        allRoles = rolesResponse.documents || [];
        
        // Also try RBAC API
        try {
            const rbacRoles = await apiRequest('/api/roles');
            if (rbacRoles.roles) {
                rbacRoles.roles.forEach(rbacRole => {
                    if (!allRoles.find(r => r.name === rbacRole.name)) {
                        allRoles.push(rbacRole);
                    }
                });
            }
        } catch (e) {
            console.log('RBAC roles endpoint not available');
        }
        
        displayRoles(allRoles);
    } catch (error) {
        console.error('Error loading roles:', error);
        showToast('Failed to load roles', 'error');
    }
}

function displayRoles(roles) {
    const tbody = document.getElementById('rolesTable');
    tbody.innerHTML = '';
    
    roles.forEach(role => {
        const userCount = role.users ? role.users.length : 0;
        const permissionCount = role.permissions ? Object.keys(role.permissions).length : 0;
        
        const row = document.createElement('tr');
        row.innerHTML = `
            <td><strong>${role.name || role.id}</strong></td>
            <td>${role.description || '-'}</td>
            <td><span class="badge bg-info">${userCount} users</span></td>
            <td><span class="badge bg-success">${permissionCount} permissions</span></td>
            <td>
                <div class="action-buttons">
                    <button class="btn btn-sm btn-outline-primary" onclick="editRole('${role._id || role.id}')">
                        <i class="bi bi-pencil"></i>
                    </button>
                    <button class="btn btn-sm btn-outline-danger" onclick="deleteRole('${role._id || role.id}')">
                        <i class="bi bi-trash"></i>
                    </button>
                </div>
            </td>
        `;
        tbody.appendChild(row);
    });
}

function filterRoles() {
    const search = document.getElementById('roleSearch').value.toLowerCase();
    const filtered = allRoles.filter(role => 
        (role.name && role.name.toLowerCase().includes(search)) ||
        (role.description && role.description.toLowerCase().includes(search))
    );
    displayRoles(filtered);
}

function showCreateRoleModal() {
    document.getElementById('roleModalTitle').textContent = 'Create Role';
    document.getElementById('roleForm').reset();
    document.getElementById('roleId').value = '';
    loadPermissionsForRole({});
    new bootstrap.Modal(document.getElementById('roleModal')).show();
}

async function editRole(roleId) {
    try {
        const role = allRoles.find(r => r._id === roleId || r.id === roleId);
        if (!role) return;
        
        document.getElementById('roleModalTitle').textContent = 'Edit Role';
        document.getElementById('roleId').value = roleId;
        document.getElementById('roleName').value = role.name || '';
        document.getElementById('roleDescription').value = role.description || '';
        
        loadPermissionsForRole(role);
        
        new bootstrap.Modal(document.getElementById('roleModal')).show();
    } catch (error) {
        console.error('Error editing role:', error);
        showToast('Failed to load role details', 'error');
    }
}

function loadPermissionsForRole(role) {
    const container = document.getElementById('rolePermissions');
    container.innerHTML = '';
    
    permissionResources.forEach(resource => {
        const card = document.createElement('div');
        card.className = 'permission-card';
        card.innerHTML = `
            <h6>${resource.charAt(0).toUpperCase() + resource.slice(1)}</h6>
            ${permissionActions.map(action => `
                <div class="form-check">
                    <input class="form-check-input" type="checkbox" 
                           id="perm_${resource}_${action}" 
                           data-resource="${resource}" 
                           data-action="${action}"
                           ${hasPermission(role, resource, action) ? 'checked' : ''}>
                    <label class="form-check-label" for="perm_${resource}_${action}">
                        ${action.charAt(0).toUpperCase() + action.slice(1)}
                    </label>
                </div>
            `).join('')}
        `;
        container.appendChild(card);
    });
}

function hasPermission(role, resource, action) {
    if (!role.permissions) return false;
    
    // Check for wildcard permissions
    if (role.permissions['*:*']) return true;
    if (role.permissions[`${resource}:*`]) return true;
    if (role.permissions[`${resource}:${action}`]) return true;
    
    return false;
}

async function saveRole() {
    try {
        const roleId = document.getElementById('roleId').value;
        const roleName = document.getElementById('roleName').value;
        const roleDescription = document.getElementById('roleDescription').value;
        
        // Collect permissions
        const permissions = {};
        document.querySelectorAll('#rolePermissions input[type="checkbox"]:checked').forEach(checkbox => {
            const resource = checkbox.dataset.resource;
            const action = checkbox.dataset.action;
            permissions[`${resource}:${action}`] = true;
        });
        
        const roleData = {
            name: roleName,
            description: roleDescription,
            permissions
        };
        
        if (roleId) {
            // Update existing role
            await apiRequest(`/api/roles/${roleId}`, {
                method: 'PUT',
                body: JSON.stringify(roleData)
            });
            showToast('Role updated successfully');
        } else {
            // Create new role
            await apiRequest('/api/roles', {
                method: 'POST',
                body: JSON.stringify(roleData)
            });
            showToast('Role created successfully');
        }
        
        bootstrap.Modal.getInstance(document.getElementById('roleModal')).hide();
        loadRoles();
    } catch (error) {
        console.error('Error saving role:', error);
        showToast(error.message || 'Failed to save role', 'error');
    }
}

async function deleteRole(roleId) {
    if (!confirm('Are you sure you want to delete this role?')) return;
    
    try {
        await apiRequest(`/api/roles/${roleId}`, {
            method: 'DELETE'
        });
        showToast('Role deleted successfully');
        loadRoles();
    } catch (error) {
        console.error('Error deleting role:', error);
        showToast('Failed to delete role', 'error');
    }
}

// Permission Matrix Functions
async function loadPermissionMatrix() {
    try {
        await loadRoles(); // Ensure roles are loaded
        
        const container = document.getElementById('permissionMatrix');
        container.innerHTML = '';
        
        // Header row
        container.innerHTML = `
            <div class="permission-matrix-header">Resource</div>
            ${allRoles.map(role => 
                `<div class="permission-matrix-header">${role.name || role.id}</div>`
            ).join('')}
        `;
        
        // Permission rows
        permissionResources.forEach(resource => {
            permissionActions.forEach(action => {
                // Resource/Action label
                const labelDiv = document.createElement('div');
                labelDiv.className = 'permission-matrix-header';
                labelDiv.style.textAlign = 'left';
                labelDiv.textContent = `${resource}:${action}`;
                container.appendChild(labelDiv);
                
                // Permission toggles for each role
                allRoles.forEach(role => {
                    const cellDiv = document.createElement('div');
                    cellDiv.className = 'permission-matrix-cell';
                    
                    const toggleId = `matrix_${role._id || role.id}_${resource}_${action}`;
                    cellDiv.innerHTML = `
                        <label class="permission-toggle">
                            <input type="checkbox" id="${toggleId}"
                                   data-role-id="${role._id || role.id}"
                                   data-resource="${resource}"
                                   data-action="${action}"
                                   ${hasPermission(role, resource, action) ? 'checked' : ''}>
                            <span class="permission-slider"></span>
                        </label>
                    `;
                    container.appendChild(cellDiv);
                });
            });
        });
    } catch (error) {
        console.error('Error loading permission matrix:', error);
        showToast('Failed to load permission matrix', 'error');
    }
}

async function savePermissions() {
    try {
        const updates = [];
        
        // Collect all permission changes
        allRoles.forEach(role => {
            const permissions = {};
            
            document.querySelectorAll(`input[data-role-id="${role._id || role.id}"]`).forEach(checkbox => {
                if (checkbox.checked) {
                    const resource = checkbox.dataset.resource;
                    const action = checkbox.dataset.action;
                    permissions[`${resource}:${action}`] = true;
                }
            });
            
            updates.push({
                roleId: role._id || role.id,
                permissions
            });
        });
        
        // Update each role
        for (const update of updates) {
            const role = allRoles.find(r => r._id === update.roleId || r.id === update.roleId);
            await apiRequest(`/api/roles/${update.roleId}`, {
                method: 'PUT',
                body: JSON.stringify({
                    name: role.name,
                    description: role.description,
                    permissions: update.permissions
                })
            });
        }
        
        showToast('Permissions updated successfully');
        loadRoles(); // Reload to ensure consistency
    } catch (error) {
        console.error('Error saving permissions:', error);
        showToast('Failed to save permissions', 'error');
    }
}