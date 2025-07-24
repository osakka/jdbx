/**
 * Authentication Manager
 * Handles all authentication logic with proper encapsulation
 */

import { state } from '../core/state.js';
import { api } from '../core/api.js';
import { STORAGE_KEYS } from '../core/constants.js';

export class AuthManager {
    constructor() {
        this.isInitialized = false;
    }

    /**
     * Initialize authentication manager
     */
    async initialize() {
        if (this.isInitialized) return;

        console.log('🔐 Initializing Authentication Manager');

        // Check for existing token
        const token = localStorage.getItem(STORAGE_KEYS.AUTH_TOKEN);
        console.log('🔍 Initial token check:', !!token);
        console.log('🔍 Token value:', token ? token.substring(0, 20) + '...' : 'null');
        
        if (token) {
            console.log('✅ Token found, setting state and validating session');
            state.setState('authToken', token);
            
            // Add small delay to ensure token is properly set
            await new Promise(resolve => setTimeout(resolve, 100));
            
            const isValid = await this.validateSession();
            if (!isValid) {
                console.log('❌ Session validation failed, staying on login redirect');
                return;
            }
        } else {
            console.log('❌ No token found, redirecting to login');
            this.redirectToLogin();
        }

        this.isInitialized = true;
    }

    /**
     * Validate current session
     */
    async validateSession() {
        const token = localStorage.getItem(STORAGE_KEYS.AUTH_TOKEN);
        console.log('🔍 validateSession called, token exists:', !!token);
        console.log('🔍 validateSession token value:', token ? token.substring(0, 20) + '...' : 'null');
        
        if (!token) {
            console.log('❌ No token found in validateSession, redirecting to login');
            this.redirectToLogin();
            return false;
        }
        
        try {
            console.log('🌐 Calling /api/auth/session...');
            const response = await api('/api/auth/session');
            console.log('✅ Session response:', response);
            
            // Handle data envelope from server
            const session = response.data || response;
            console.log('✅ Extracted session data:', session);
            
            if (session && session.username) {
                console.log('✅ Session valid, setting auth state');
                state.setState({
                    isAuthenticated: true,
                    user: { username: session.username, user_uuid: session.user_uuid }
                });
                return true;
            } else {
                console.log('❌ Session invalid or no user, logging out');
                this.logout();
                return false;
            }
        } catch (error) {
            console.error('❌ Session validation failed:', error);
            console.error('❌ Error details:', error.message);
            // Don't automatically logout on API errors - could be network issues
            console.log('❌ Session validation failed, but keeping token for retry');
            return false;
        }
    }

    /**
     * Log out user
     */
    logout() {
        console.log('🚪 Logging out user');
        
        // Clear tokens
        localStorage.removeItem(STORAGE_KEYS.AUTH_TOKEN);
        localStorage.removeItem(STORAGE_KEYS.REFRESH_TOKEN);
        
        // Clear state
        state.setState({
            isAuthenticated: false,
            user: null,
            authToken: null
        });
        
        // Redirect to login
        this.redirectToLogin();
    }

    /**
     * Redirect to login page
     */
    redirectToLogin() {
        if (window.location.pathname !== '/login.html') {
            window.location.href = '/login.html';
        }
    }

    /**
     * Get current user
     * @returns {object|null} Current user object
     */
    getCurrentUser() {
        return state.get('user');
    }

    /**
     * Check if user is authenticated
     * @returns {boolean} Authentication status
     */
    isAuthenticated() {
        return state.get('isAuthenticated') || false;
    }

    /**
     * Get auth token
     * @returns {string|null} Auth token
     */
    getAuthToken() {
        return state.get('authToken') || localStorage.getItem(STORAGE_KEYS.AUTH_TOKEN);
    }

    /**
     * Cleanup resources
     */
    cleanup() {
        // No cleanup needed for auth manager
    }
}