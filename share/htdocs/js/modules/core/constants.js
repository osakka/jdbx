/**
 * JDBX UI Core Constants
 * Single source of truth for all application constants
 */

export const API_BASE_URL = '';

export const POLLING_INTERVALS = {
    dashboard: 60000,     // 60 seconds for dashboard
    browser: 300000,      // 5 minutes for browser (very conservative)
    metrics: 60000,       // 60 seconds for metrics
    rbac: 60000,          // 60 seconds for RBAC
    operations: 120000    // 2 minutes for operations
};

export const UNIFIED_API = {
    documents: '/api/documents',        // Main unified documents endpoint
    collections: '/api/collections',    // Virtual collections (document types)
    libraries: '/api/libraries'         // Library management
};

export const VIEW_TYPES = {
    DASHBOARD: 'dashboard',
    BROWSER: 'browser',
    METRICS: 'metrics',
    RBAC: 'rbac',
    OPERATIONS: 'operations'
};

export const NOTIFICATION_TYPES = {
    SUCCESS: 'success',
    ERROR: 'danger',
    WARNING: 'warning',
    INFO: 'info'
};

export const STORAGE_KEYS = {
    AUTH_TOKEN: 'jdbx_auth_token',
    THEME: 'jdbx_theme',
    CURRENT_LIBRARY: 'jdbx_current_library'
};