/**
 * JDBX Modular Application Bootstrap
 * Main entry point for the modular UI system
 */

// Core modules
import { API_BASE_URL, POLLING_INTERVALS, UNIFIED_API, VIEW_TYPES } from './core/constants.js';
import { formatBytes, formatDate, debounce, generateId, isEmpty } from './core/utils.js';
import { 
    apiRequest, 
    validateSession, 
    getActualDatabaseSize,
    isAuthenticated,
    logout,
    documentsAPI,
    collectionsAPI,
    librariesAPI 
} from './core/api.js';
import { state, getState, setState, subscribe } from './core/state.js';

// UI modules
import { 
    showNotification, 
    showSuccess, 
    showError, 
    showWarning, 
    showInfo,
    clearAllNotifications 
} from './ui/notifications.js';
import { 
    switchView, 
    navigateTo, 
    navigateFromHash, 
    setupHashNavigation 
} from './ui/navigation.js';

// Chart modules
import { 
    createChart, 
    updateChart, 
    destroyChart, 
    destroyAllCharts, 
    updateAllChartsTheme,
    resizeAllCharts 
} from './charts/chartManager.js';

/**
 * Initialize modular application
 */
async function initializeApp() {
    console.log('🚀 Initializing JDBX Modular Application...');

    try {
        // Mark HTML as loaded to prevent FOUC with minimal layout impact
        document.documentElement.classList.add('loaded');
        
        // Check authentication
        if (!isAuthenticated()) {
            window.location.href = '/login.html';
            return;
        }

        // Initialize global database size tracking
        window.currentDatabaseSizeBytes = 0;

        // DISABLED: Legacy app already handles session validation
        // setupSessionValidation();

        // DISABLED: Let legacy app handle navigation for now
        // setupHashNavigation();

        // Set up theme change listener for charts
        setupThemeChangeListener();

        // Set up window resize listener
        setupResizeListener();

        // DISABLED: Let legacy app handle everything - modular system is purely passive
        // setupIDConflictDetection();
        // navigateFromHash();

        console.log('✅ JDBX Modular Application initialized successfully');

    } catch (error) {
        console.error('❌ Failed to initialize application:', error);
        showError(`Application initialization failed: ${error.message}`);
    }
}

/**
 * Set up session validation
 */
function setupSessionValidation() {
    // Validate session every 30 seconds
    const sessionInterval = setInterval(async () => {
        const isValid = await validateSession();
        
        if (!isValid) {
            clearInterval(sessionInterval);
            showWarning('Session expired. Redirecting to login...');
            setTimeout(() => {
                logout();
            }, 2000);
        }
    }, 30000);

    // Store interval for cleanup
    setState('sessionInterval', sessionInterval);
}

/**
 * Set up theme change listener for charts
 */
function setupThemeChangeListener() {
    // Listen for theme changes
    const observer = new MutationObserver((mutations) => {
        mutations.forEach((mutation) => {
            if (mutation.type === 'attributes' && mutation.attributeName === 'data-theme') {
                // Update all charts with new theme
                updateAllChartsTheme();
            }
        });
    });

    // Observe theme changes on body element
    observer.observe(document.body, {
        attributes: true,
        attributeFilter: ['data-theme']
    });
}

/**
 * Set up window resize listener
 */
function setupResizeListener() {
    const debouncedResize = debounce(() => {
        resizeAllCharts();
    }, 250);

    window.addEventListener('resize', debouncedResize);
}

/**
 * Set up ID conflict detection (legacy compatibility)
 */
function setupIDConflictDetection() {
    if (typeof window.detectIDConflicts === 'function') {
        // Start ID conflict detection if original function exists
        window.detectIDConflicts();
    }
}

/**
 * Clean up application resources
 */
function cleanupApp() {
    console.log('🧹 Cleaning up JDBX application...');

    // Clear session interval
    const sessionInterval = state.get('sessionInterval');
    if (sessionInterval) {
        clearInterval(sessionInterval);
    }

    // Clear view refresh interval
    const refreshInterval = state.get('refreshInterval');
    if (refreshInterval) {
        clearInterval(refreshInterval);
    }

    // Destroy all charts
    destroyAllCharts();

    // Clear notifications
    clearAllNotifications();

    // Reset state
    state.reset();

    console.log('✅ Application cleanup completed');
}

/**
 * Global error handler
 */
function setupGlobalErrorHandler() {
    window.addEventListener('error', (event) => {
        console.error('Global error:', event.error);
        showError('An unexpected error occurred. Please refresh the page.');
    });

    window.addEventListener('unhandledrejection', (event) => {
        console.error('Unhandled promise rejection:', event.reason);
        showError('An unexpected error occurred. Please refresh the page.');
    });
}

// Export key functions for global access
const globalExports = {
    // Core
    API_BASE_URL,
    POLLING_INTERVALS,
    UNIFIED_API,
    VIEW_TYPES,
    
    // Utils
    formatBytes,
    formatDate,
    debounce,
    generateId,
    isEmpty,
    
    // API
    apiRequest,
    validateSession,
    getActualDatabaseSize,
    isAuthenticated,
    logout,
    documentsAPI,
    collectionsAPI,
    librariesAPI,
    
    // State
    state,
    getState,
    setState,
    subscribe,
    
    // UI
    showNotification,
    showSuccess,
    showError,
    showWarning,
    showInfo,
    clearAllNotifications,
    switchView,
    navigateTo,
    navigateFromHash,
    
    // Charts
    createChart,
    updateChart,
    destroyChart,
    destroyAllCharts,
    updateAllChartsTheme,
    
    // App
    initializeApp,
    cleanupApp
};

// Make functions globally available for legacy compatibility
if (typeof window !== 'undefined') {
    Object.assign(window, globalExports);
}

// Set up global error handling
setupGlobalErrorHandler();

// Simplified initialization - just provide utilities
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        // Wait a bit for legacy app to initialize first
        setTimeout(initializeApp, 500);
    });
} else {
    // DOM is already ready, wait for legacy app
    setTimeout(initializeApp, 500);
}

// Clean up on page unload
window.addEventListener('beforeunload', cleanupApp);

export default globalExports;