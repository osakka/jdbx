/**
 * JDBX UI Navigation System
 * Centralized view switching and navigation
 */

import { VIEW_TYPES, POLLING_INTERVALS } from '../core/constants.js';
import { state, setState, subscribe } from '../core/state.js';
import { showNotification } from './notifications.js';

/**
 * Switch to specified view
 * @param {string} view - View to switch to
 */
export function switchView(view) {
    // Validate view type
    if (!Object.values(VIEW_TYPES).includes(view)) {
        showNotification(`Invalid view: ${view}`, 'error');
        return;
    }

    const currentView = state.get('currentView');
    
    // No change needed
    if (currentView === view) {
        return;
    }

    // Clear current polling interval
    const refreshInterval = state.get('refreshInterval');
    if (refreshInterval) {
        clearInterval(refreshInterval);
        setState('refreshInterval', null);
    }

    // Hide all views
    hideAllViews();

    // Show selected view
    showView(view);

    // Update state
    setState('currentView', view);

    // Update navigation UI
    updateNavigationUI(view);

    // Initialize view-specific functionality
    initializeView(view);

    // Set up polling for the new view
    setupPolling(view);
}

/**
 * Hide all view containers
 */
function hideAllViews() {
    const views = Object.values(VIEW_TYPES);
    
    views.forEach(view => {
        const viewElement = document.getElementById(`${view}-view`);
        if (viewElement) {
            viewElement.classList.remove('active');
        }
    });
}

/**
 * Show specific view container
 * @param {string} view - View to show
 */
function showView(view) {
    const viewElement = document.getElementById(`${view}-view`);
    if (viewElement) {
        viewElement.classList.add('active');
    } else {
        showNotification(`View element not found: ${view}-view`, 'error');
    }
}

/**
 * Update navigation UI to reflect current view
 * @param {string} view - Current view
 */
function updateNavigationUI(view) {
    // Update navigation pills
    const navLinks = document.querySelectorAll('.nav-pills .nav-link');
    
    navLinks.forEach(link => {
        const href = link.getAttribute('href');
        if (href === `#${view}`) {
            link.classList.add('active');
        } else {
            link.classList.remove('active');
        }
    });

    // Update page title
    updatePageTitle(view);
}

/**
 * Update page title based on current view
 * @param {string} view - Current view
 */
function updatePageTitle(view) {
    const titles = {
        [VIEW_TYPES.DASHBOARD]: 'JDBX - Dashboard',
        [VIEW_TYPES.BROWSER]: 'JDBX - Document Browser',
        [VIEW_TYPES.METRICS]: 'JDBX - Metrics',
        [VIEW_TYPES.RBAC]: 'JDBX - RBAC Management',
        [VIEW_TYPES.OPERATIONS]: 'JDBX - Operations'
    };

    document.title = titles[view] || 'JDBX';
}

/**
 * Initialize view-specific functionality
 * @param {string} view - View to initialize
 */
async function initializeView(view) {
    setState('isLoading', true);

    try {
        switch (view) {
            case VIEW_TYPES.DASHBOARD:
                await initializeDashboard();
                break;
            case VIEW_TYPES.BROWSER:
                await initializeBrowser();
                break;
            case VIEW_TYPES.METRICS:
                await initializeMetrics();
                break;
            case VIEW_TYPES.RBAC:
                await initializeRBAC();
                break;
            case VIEW_TYPES.OPERATIONS:
                await initializeOperations();
                break;
            default:
                showNotification(`Unknown view: ${view}`, 'warning');
        }
    } catch (error) {
        showNotification(`Error initializing ${view}: ${error.message}`, 'error');
    } finally {
        setState('isLoading', false);
    }
}

/**
 * Set up polling for specific view
 * @param {string} view - View to set up polling for
 */
function setupPolling(view) {
    const interval = POLLING_INTERVALS[view];
    
    if (!interval) {
        return;
    }

    const refreshInterval = setInterval(() => {
        refreshCurrentView();
    }, interval);

    setState('refreshInterval', refreshInterval);
}

/**
 * Refresh current view data
 */
async function refreshCurrentView() {
    const currentView = state.get('currentView');
    
    try {
        switch (currentView) {
            case VIEW_TYPES.DASHBOARD:
                await loadDashboard(true); // isPolling = true
                break;
            case VIEW_TYPES.BROWSER:
                await refreshBrowser();
                break;
            case VIEW_TYPES.METRICS:
                await refreshMetrics();
                break;
            case VIEW_TYPES.RBAC:
                await refreshRBAC();
                break;
            case VIEW_TYPES.OPERATIONS:
                await refreshOperations();
                break;
        }
    } catch (error) {
        console.warn(`Polling error for ${currentView}:`, error);
        // Don't show notifications for polling errors to avoid spam
    }
}

/**
 * Initialize dashboard view
 */
async function initializeDashboard() {
    // Check if legacy app has loaded and is available
    if (typeof window.initializeDashboard === 'function' && window.initializeDashboard !== initializeDashboard) {
        try {
            console.log('🔗 Calling legacy initializeDashboard');
            await window.initializeDashboard();
        } catch (error) {
            console.warn('Error calling legacy initializeDashboard:', error);
        }
    }
    
    if (typeof window.loadDashboard === 'function') {
        try {
            console.log('🔗 Calling legacy loadDashboard');
            await window.loadDashboard(false);
        } catch (error) {
            console.warn('Error calling legacy loadDashboard:', error);
        }
    } else {
        console.warn('⚠️ loadDashboard function not available - data may not load');
        showNotification('Dashboard data loading may be delayed. Please refresh if data does not appear.', 'info');
    }
}

/**
 * Initialize browser view
 */
async function initializeBrowser() {
    // Will be implemented when browser module is extracted
    if (window.initializeBrowser) {
        await window.initializeBrowser();
    }
}

/**
 * Initialize metrics view
 */
async function initializeMetrics() {
    // Will be implemented when metrics module is extracted
    if (typeof window.initializeMetrics === 'function') {
        try {
            await window.initializeMetrics();
        } catch (error) {
            console.warn('Error initializing metrics view:', error);
            showNotification('Metrics view initialization failed. Some features may not work correctly.', 'warning');
        }
    } else {
        console.warn('initializeMetrics function not available');
        showNotification('Metrics view not fully initialized. Some features may not work.', 'info');
    }
}

/**
 * Initialize RBAC view
 */
async function initializeRBAC() {
    // Will be implemented when RBAC module is extracted
    if (window.initializeRBAC) {
        await window.initializeRBAC();
    }
}

/**
 * Initialize operations view
 */
async function initializeOperations() {
    // Will be implemented when operations module is extracted
    if (window.initializeOperations) {
        await window.initializeOperations();
    }
}

/**
 * Refresh browser view
 */
async function refreshBrowser() {
    if (window.refreshBrowser) {
        await window.refreshBrowser();
    }
}

/**
 * Refresh metrics view
 */
async function refreshMetrics() {
    if (window.refreshMetrics) {
        await window.refreshMetrics();
    }
}

/**
 * Refresh RBAC view
 */
async function refreshRBAC() {
    if (window.refreshRBAC) {
        await window.refreshRBAC();
    }
}

/**
 * Refresh operations view
 */
async function refreshOperations() {
    if (window.refreshOperations) {
        await window.refreshOperations();
    }
}

/**
 * Navigate to view from URL hash
 */
export function navigateFromHash() {
    const hash = window.location.hash.slice(1); // Remove #
    
    if (hash && Object.values(VIEW_TYPES).includes(hash)) {
        switchView(hash);
    } else {
        switchView(VIEW_TYPES.DASHBOARD);
    }
}

/**
 * Set up hash change listener
 */
export function setupHashNavigation() {
    window.addEventListener('hashchange', navigateFromHash);
    
    // Navigate on initial load
    navigateFromHash();
}

/**
 * Navigate to specific view and update URL
 * @param {string} view - View to navigate to
 */
export function navigateTo(view) {
    window.location.hash = view;
    // Hash change will trigger switchView
}

/**
 * Navigation Manager Class
 * Encapsulates navigation functionality
 */
export class NavigationManager {
    constructor() {
        this.isInitialized = false;
    }

    /**
     * Initialize navigation manager
     */
    initialize() {
        if (this.isInitialized) return;
        
        console.log('🧭 Initializing Navigation Manager');
        
        // Set up hash navigation
        setupHashNavigation();
        
        this.isInitialized = true;
    }

    /**
     * Switch to view
     * @param {string} view - View to switch to
     */
    switchView(view) {
        switchView(view);
    }

    /**
     * Navigate to view
     * @param {string} view - View to navigate to
     */
    navigateTo(view) {
        navigateTo(view);
    }

    /**
     * Cleanup resources
     */
    cleanup() {
        // Clear any intervals
        const refreshInterval = state.get('refreshInterval');
        if (refreshInterval) {
            clearInterval(refreshInterval);
            setState('refreshInterval', null);
        }
    }
}

// Make functions globally available for onclick handlers
if (typeof window !== 'undefined') {
    window.switchView = switchView;
    window.navigateTo = navigateTo;
}