/**
 * JavaScript-Enhanced Error Tracking Metrics for JDBX
 * Provides intelligent error classification, rate calculation, and alerting
 */

// Error classification and tracking
function classifyError(statusCode, error, context) {
    const errorMetric = {
        timestamp: new Date().toISOString(),
        status_code: statusCode,
        category: 'unknown',
        severity: 'low',
        endpoint: context.endpoint || 'unknown',
        user_id: context.user_id || 'anonymous',
        error_message: error.message || error,
        stack_trace: error.stack || null
    };

    // Classify by status code
    if (statusCode >= 200 && statusCode < 300) {
        errorMetric.category = 'success';
        errorMetric.severity = 'info';
    } else if (statusCode >= 300 && statusCode < 400) {
        errorMetric.category = 'redirect';
        errorMetric.severity = 'info';
    } else if (statusCode >= 400 && statusCode < 500) {
        errorMetric.category = 'client_error';
        errorMetric.severity = statusCode === 401 || statusCode === 403 ? 'medium' : 'low';
    } else if (statusCode >= 500) {
        errorMetric.category = 'server_error';
        errorMetric.severity = statusCode >= 503 ? 'critical' : 'high';
    }

    // Special classifications
    if (error.message && error.message.includes('timeout')) {
        errorMetric.category = 'timeout';
        errorMetric.severity = 'medium';
    }
    
    if (error.message && error.message.includes('memory')) {
        errorMetric.category = 'resource_exhaustion';
        errorMetric.severity = 'critical';
    }

    return errorMetric;
}

// Calculate error rates with JavaScript
function calculateErrorRates(errorHistory, timeWindowMinutes = 5) {
    const now = new Date();
    const windowStart = new Date(now.getTime() - timeWindowMinutes * 60 * 1000);
    
    const recentErrors = errorHistory.filter(error => 
        new Date(error.timestamp) >= windowStart
    );
    
    const totalRequests = recentErrors.length;
    const errorsByCategory = {};
    const errorsByEndpoint = {};
    
    recentErrors.forEach(error => {
        // Count by category
        errorsByCategory[error.category] = (errorsByCategory[error.category] || 0) + 1;
        
        // Count by endpoint
        errorsByEndpoint[error.endpoint] = (errorsByEndpoint[error.endpoint] || 0) + 1;
    });
    
    return {
        total_requests: totalRequests,
        error_rate: totalRequests / timeWindowMinutes, // errors per minute
        categories: errorsByCategory,
        endpoints: errorsByEndpoint,
        window_minutes: timeWindowMinutes,
        critical_errors: recentErrors.filter(e => e.severity === 'critical').length,
        high_errors: recentErrors.filter(e => e.severity === 'high').length
    };
}

// Alert threshold checker
function checkErrorThresholds(errorRates, config = {}) {
    const thresholds = {
        error_rate_critical: config.error_rate_critical || 10, // errors/min
        error_rate_warning: config.error_rate_warning || 5,
        critical_error_limit: config.critical_error_limit || 1,
        high_error_limit: config.high_error_limit || 3,
        ...config
    };
    
    const alerts = [];
    
    if (errorRates.error_rate >= thresholds.error_rate_critical) {
        alerts.push({
            level: 'CRITICAL',
            message: `Error rate (${errorRates.error_rate.toFixed(2)}/min) exceeds critical threshold (${thresholds.error_rate_critical}/min)`,
            metric: 'error_rate',
            value: errorRates.error_rate
        });
    } else if (errorRates.error_rate >= thresholds.error_rate_warning) {
        alerts.push({
            level: 'WARNING',
            message: `Error rate (${errorRates.error_rate.toFixed(2)}/min) exceeds warning threshold (${thresholds.error_rate_warning}/min)`,
            metric: 'error_rate',
            value: errorRates.error_rate
        });
    }
    
    if (errorRates.critical_errors >= thresholds.critical_error_limit) {
        alerts.push({
            level: 'CRITICAL',
            message: `Critical errors (${errorRates.critical_errors}) exceed limit (${thresholds.critical_error_limit})`,
            metric: 'critical_errors',
            value: errorRates.critical_errors
        });
    }
    
    return alerts;
}

// Export for JDBX integration
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { classifyError, calculateErrorRates, checkErrorThresholds };
}