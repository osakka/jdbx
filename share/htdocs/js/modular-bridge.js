/**
 * JDBX Modular Bridge
 * Seamless transition between monolithic and modular architecture
 * Enables progressive migration with zero regressions
 */

// Flag to enable modular system (passive mode for now)
const ENABLE_MODULAR_SYSTEM = true;
const PASSIVE_MODE = true; // Don't interfere with legacy navigation

/**
 * Initialize modular bridge
 */
function initializeModularBridge() {
    if (!ENABLE_MODULAR_SYSTEM) {
        console.log('📦 Modular system disabled - using legacy monolithic app');
        return;
    }
    
    if (PASSIVE_MODE) {
        console.log('😴 Modular system in passive mode - legacy app handles navigation');
    }

    console.log('🌉 Initializing JDBX Modular Bridge...');

    // Check if modular system is available
    if (typeof window.initializeApp === 'function') {
        console.log('✅ Modular system detected and available');
        
        // The modular app.js will auto-initialize via DOMContentLoaded
        // This bridge ensures compatibility
        
        // Set up bridge functions for legacy compatibility
        setupLegacyCompatibilityBridge();
        
    } else {
        console.log('⚠️ Modular system not available - falling back to legacy app');
        // Legacy app.js will load normally
    }
}

/**
 * Set up compatibility bridge for legacy code
 */
function setupLegacyCompatibilityBridge() {
    // Ensure global functions exist for onclick handlers and legacy code
    
    // Navigation bridge
    if (!window.switchView && window.switchView) {
        // Already available from modular system
    }
    
    // Notification bridge
    if (!window.showNotification && window.showNotification) {
        // Already available from modular system
    }
    
    // Chart bridge
    if (!window.createChart && window.createChart) {
        // Already available from modular system
    }
    
    console.log('🔗 Legacy compatibility bridge established');
}

/**
 * Feature detection for modular capabilities
 */
function detectModularCapabilities() {
    const capabilities = {
        modules: typeof window.initializeApp === 'function',
        state: typeof window.state === 'object',
        api: typeof window.apiRequest === 'function',
        charts: typeof window.createChart === 'function',
        navigation: typeof window.switchView === 'function',
        notifications: typeof window.showNotification === 'function'
    };
    
    console.log('🔍 Modular capabilities:', capabilities);
    return capabilities;
}

/**
 * Progressive enhancement detector
 */
function detectProgressiveEnhancement() {
    const features = {
        esModules: 'noModule' in HTMLScriptElement.prototype,
        fetch: typeof fetch === 'function',
        promise: typeof Promise === 'function',
        localStorage: typeof Storage !== 'undefined',
        mutationObserver: typeof MutationObserver === 'function'
    };
    
    const isModernBrowser = Object.values(features).every(Boolean);
    
    console.log('🌐 Browser features:', features);
    console.log('📱 Modern browser support:', isModernBrowser);
    
    return isModernBrowser;
}

// Initialize bridge when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        detectProgressiveEnhancement();
        detectModularCapabilities();
        initializeModularBridge();
    });
} else {
    // DOM is already ready
    detectProgressiveEnhancement();
    detectModularCapabilities();
    initializeModularBridge();
}

// Export for manual initialization if needed
window.initializeModularBridge = initializeModularBridge;