/**
 * JDBX UI Notification System
 * Centralized notification management
 */

import { NOTIFICATION_TYPES } from '../core/constants.js';
import { generateId } from '../core/utils.js';

/**
 * Show notification to user
 * @param {string} message - Notification message
 * @param {string} type - Notification type (success, error, warning, info)
 * @param {number} duration - Auto-dismiss duration in ms (0 = no auto-dismiss)
 */
export function showNotification(message, type = NOTIFICATION_TYPES.INFO, duration = 5000) {
    // Ensure notification container exists
    let container = document.getElementById('notification-container');
    if (!container) {
        container = createNotificationContainer();
    }

    // Create notification element
    const notification = document.createElement('div');
    const notificationId = generateId();
    notification.id = notificationId;
    notification.className = `alert alert-${type} alert-dismissible fade show notification-item`;
    notification.style.cssText = `
        margin-bottom: 0.5rem;
        animation: slideInDown 0.3s ease-out;
    `;
    
    notification.innerHTML = `
        <span class="notification-message">${escapeHtml(message)}</span>
        <button type="button" class="btn-close" onclick="dismissNotification('${notificationId}')" aria-label="Close"></button>
    `;

    // Add to container
    container.appendChild(notification);

    // Auto-dismiss if duration specified
    if (duration > 0) {
        setTimeout(() => {
            dismissNotification(notificationId);
        }, duration);
    }

    // Limit number of notifications
    limitNotifications(container);
}

/**
 * Dismiss specific notification
 * @param {string} notificationId - ID of notification to dismiss
 */
export function dismissNotification(notificationId) {
    const notification = document.getElementById(notificationId);
    if (notification) {
        notification.style.animation = 'slideOutUp 0.3s ease-in';
        setTimeout(() => {
            if (notification.parentNode) {
                notification.parentNode.removeChild(notification);
            }
        }, 300);
    }
}

/**
 * Clear all notifications
 */
export function clearAllNotifications() {
    const container = document.getElementById('notification-container');
    if (container) {
        container.innerHTML = '';
    }
}

/**
 * Create notification container if it doesn't exist
 * @returns {HTMLElement} Notification container
 */
function createNotificationContainer() {
    let container = document.getElementById('notification-container');
    
    if (!container) {
        container = document.createElement('div');
        container.id = 'notification-container';
        container.style.cssText = `
            position: fixed;
            top: 80px;
            right: 20px;
            z-index: 9999;
            max-width: 400px;
            pointer-events: none;
        `;
        
        // Make notifications interactive
        container.addEventListener('click', (e) => {
            e.stopPropagation();
        });
        
        // Enable pointer events for notifications
        container.style.pointerEvents = 'auto';
        
        document.body.appendChild(container);
    }
    
    return container;
}

/**
 * Limit number of notifications displayed
 * @param {HTMLElement} container - Notification container
 * @param {number} maxNotifications - Maximum notifications to show
 */
function limitNotifications(container, maxNotifications = 5) {
    const notifications = container.querySelectorAll('.notification-item');
    
    if (notifications.length > maxNotifications) {
        // Remove oldest notifications
        for (let i = 0; i < notifications.length - maxNotifications; i++) {
            notifications[i].remove();
        }
    }
}

/**
 * Escape HTML to prevent XSS
 * @param {string} text - Text to escape
 * @returns {string} Escaped text
 */
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

/**
 * Show success notification
 * @param {string} message - Success message
 */
export function showSuccess(message) {
    showNotification(message, NOTIFICATION_TYPES.SUCCESS);
}

/**
 * Show error notification
 * @param {string} message - Error message
 */
export function showError(message) {
    showNotification(message, NOTIFICATION_TYPES.ERROR, 8000); // Longer duration for errors
}

/**
 * Show warning notification
 * @param {string} message - Warning message
 */
export function showWarning(message) {
    showNotification(message, NOTIFICATION_TYPES.WARNING, 6000);
}

/**
 * Show info notification
 * @param {string} message - Info message
 */
export function showInfo(message) {
    showNotification(message, NOTIFICATION_TYPES.INFO);
}

/**
 * Notification Manager Class
 * Provides a class-based interface for the notification system
 */
export class NotificationManager {
    constructor() {
        this.isInitialized = false;
    }

    /**
     * Initialize notification manager
     */
    initialize() {
        if (this.isInitialized) return;
        
        console.log('🔔 Initializing Notification Manager');
        
        // Ensure container exists
        if (!document.getElementById('notification-container')) {
            createNotificationContainer();
        }
        
        this.isInitialized = true;
    }

    /**
     * Show notification
     */
    show(message, type, duration) {
        return showNotification(message, type, duration);
    }

    /**
     * Show success notification
     */
    showSuccess(message) {
        return showSuccess(message);
    }

    /**
     * Show error notification
     */
    showError(message) {
        return showError(message);
    }

    /**
     * Show warning notification
     */
    showWarning(message) {
        return showWarning(message);
    }

    /**
     * Show info notification
     */
    showInfo(message) {
        return showInfo(message);
    }

    /**
     * Clear all notifications
     */
    clearAll() {
        return clearAllNotifications();
    }

    /**
     * Cleanup resources
     */
    cleanup() {
        clearAllNotifications();
    }
}

// Make functions globally available for onclick handlers
if (typeof window !== 'undefined') {
    window.dismissNotification = dismissNotification;
    window.clearAllNotifications = clearAllNotifications;
}