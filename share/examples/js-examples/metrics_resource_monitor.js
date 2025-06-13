/**
 * JavaScript Resource Utilization Monitor for JDBX
 * Provides system resource tracking, threshold monitoring, and predictive analysis
 */

// Resource utilization tracker
function trackResourceUtilization(systemInfo) {
    const timestamp = new Date().toISOString();
    
    return {
        timestamp: timestamp,
        cpu: {
            usage_percent: systemInfo.cpu_usage || 0,
            load_average: systemInfo.load_average || 0,
            cores: systemInfo.cpu_cores || 1
        },
        memory: {
            total_bytes: systemInfo.memory_total || 0,
            used_bytes: systemInfo.memory_used || 0,
            free_bytes: systemInfo.memory_free || 0,
            usage_percent: systemInfo.memory_total > 0 ? 
                (systemInfo.memory_used / systemInfo.memory_total) * 100 : 0,
            process_bytes: systemInfo.process_memory || 0
        },
        disk: {
            total_bytes: systemInfo.disk_total || 0,
            used_bytes: systemInfo.disk_used || 0,
            free_bytes: systemInfo.disk_free || 0,
            usage_percent: systemInfo.disk_total > 0 ? 
                (systemInfo.disk_used / systemInfo.disk_total) * 100 : 0,
            io_reads: systemInfo.disk_reads || 0,
            io_writes: systemInfo.disk_writes || 0
        },
        network: {
            bytes_in: systemInfo.network_in || 0,
            bytes_out: systemInfo.network_out || 0,
            packets_in: systemInfo.packets_in || 0,
            packets_out: systemInfo.packets_out || 0,
            connections_active: systemInfo.connections_active || 0
        },
        threads: {
            active: systemInfo.threads_active || 0,
            total: systemInfo.threads_total || 0,
            idle: systemInfo.threads_idle || 0,
            queue_size: systemInfo.thread_queue_size || 0
        },
        file_descriptors: {
            used: systemInfo.fd_used || 0,
            max: systemInfo.fd_max || 0,
            usage_percent: systemInfo.fd_max > 0 ? 
                (systemInfo.fd_used / systemInfo.fd_max) * 100 : 0
        }
    };
}

// Resource trend analysis
function analyzeResourceTrends(resourceHistory, lookbackMinutes = 30) {
    if (resourceHistory.length < 2) {
        return { status: 'insufficient_data' };
    }
    
    const now = new Date();
    const cutoff = new Date(now.getTime() - lookbackMinutes * 60 * 1000);
    
    const recentData = resourceHistory.filter(r => 
        new Date(r.timestamp) >= cutoff
    ).sort((a, b) => new Date(a.timestamp) - new Date(b.timestamp));
    
    if (recentData.length < 2) {
        return { status: 'insufficient_recent_data' };
    }
    
    const trends = {
        cpu: analyzeTrend(recentData.map(r => r.cpu.usage_percent)),
        memory: analyzeTrend(recentData.map(r => r.memory.usage_percent)),
        disk: analyzeTrend(recentData.map(r => r.disk.usage_percent)),
        file_descriptors: analyzeTrend(recentData.map(r => r.file_descriptors.usage_percent)),
        thread_utilization: analyzeTrend(recentData.map(r => 
            r.threads.total > 0 ? (r.threads.active / r.threads.total) * 100 : 0
        ))
    };
    
    // Calculate network throughput trends
    if (recentData.length >= 2) {
        const networkThroughput = [];
        for (let i = 1; i < recentData.length; i++) {
            const prev = recentData[i-1];
            const curr = recentData[i];
            const timeDiff = (new Date(curr.timestamp) - new Date(prev.timestamp)) / 1000; // seconds
            
            if (timeDiff > 0) {
                const bytesPerSec = (curr.network.bytes_in + curr.network.bytes_out - 
                                   prev.network.bytes_in - prev.network.bytes_out) / timeDiff;
                networkThroughput.push(bytesPerSec);
            }
        }
        trends.network_throughput = analyzeTrend(networkThroughput);
    }
    
    return {
        status: 'success',
        lookback_minutes: lookbackMinutes,
        data_points: recentData.length,
        trends: trends,
        alerts: generateResourceAlerts(trends, recentData[recentData.length - 1])
    };
}

// Analyze trend for a single metric
function analyzeTrend(values) {
    if (values.length < 2) return { trend: 'unknown' };
    
    const current = values[values.length - 1];
    const previous = values[values.length - 2];
    const average = values.reduce((a, b) => a + b, 0) / values.length;
    const min = Math.min(...values);
    const max = Math.max(...values);
    
    // Calculate linear regression slope
    const n = values.length;
    const sumX = (n * (n - 1)) / 2; // 0 + 1 + 2 + ... + (n-1)
    const sumY = values.reduce((a, b) => a + b, 0);
    const sumXY = values.reduce((acc, y, x) => acc + x * y, 0);
    const sumXX = (n * (n - 1) * (2 * n - 1)) / 6; // 0² + 1² + 2² + ... + (n-1)²
    
    const slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX);
    
    const percentChange = previous !== 0 ? ((current - previous) / previous) * 100 : 0;
    
    return {
        current: current,
        previous: previous,
        average: average,
        min: min,
        max: max,
        slope: slope,
        percent_change: Math.round(percentChange * 100) / 100,
        trend: slope > 0.1 ? 'increasing' : slope < -0.1 ? 'decreasing' : 'stable',
        volatility: calculateVolatility(values)
    };
}

// Calculate volatility (standard deviation)
function calculateVolatility(values) {
    if (values.length < 2) return 0;
    
    const mean = values.reduce((a, b) => a + b, 0) / values.length;
    const squaredDiffs = values.map(value => Math.pow(value - mean, 2));
    const variance = squaredDiffs.reduce((a, b) => a + b, 0) / values.length;
    return Math.sqrt(variance);
}

// Generate resource alerts
function generateResourceAlerts(trends, currentData, thresholds = {}) {
    const alerts = [];
    const defaultThresholds = {
        cpu_warning: 70,
        cpu_critical: 90,
        memory_warning: 80,
        memory_critical: 95,
        disk_warning: 85,
        disk_critical: 95,
        fd_warning: 80,
        fd_critical: 90,
        thread_warning: 80,
        thread_critical: 95,
        ...thresholds
    };
    
    // CPU alerts
    if (currentData.cpu.usage_percent >= defaultThresholds.cpu_critical) {
        alerts.push({
            level: 'CRITICAL',
            resource: 'cpu',
            message: `CPU usage (${currentData.cpu.usage_percent.toFixed(1)}%) is critically high`,
            value: currentData.cpu.usage_percent,
            threshold: defaultThresholds.cpu_critical,
            trend: trends.cpu.trend
        });
    } else if (currentData.cpu.usage_percent >= defaultThresholds.cpu_warning) {
        alerts.push({
            level: 'WARNING',
            resource: 'cpu',
            message: `CPU usage (${currentData.cpu.usage_percent.toFixed(1)}%) is high`,
            value: currentData.cpu.usage_percent,
            threshold: defaultThresholds.cpu_warning,
            trend: trends.cpu.trend
        });
    }
    
    // Memory alerts
    if (currentData.memory.usage_percent >= defaultThresholds.memory_critical) {
        alerts.push({
            level: 'CRITICAL',
            resource: 'memory',
            message: `Memory usage (${currentData.memory.usage_percent.toFixed(1)}%) is critically high`,
            value: currentData.memory.usage_percent,
            threshold: defaultThresholds.memory_critical,
            trend: trends.memory.trend
        });
    } else if (currentData.memory.usage_percent >= defaultThresholds.memory_warning) {
        alerts.push({
            level: 'WARNING',
            resource: 'memory',
            message: `Memory usage (${currentData.memory.usage_percent.toFixed(1)}%) is high`,
            value: currentData.memory.usage_percent,
            threshold: defaultThresholds.memory_warning,
            trend: trends.memory.trend
        });
    }
    
    // Disk alerts
    if (currentData.disk.usage_percent >= defaultThresholds.disk_critical) {
        alerts.push({
            level: 'CRITICAL',
            resource: 'disk',
            message: `Disk usage (${currentData.disk.usage_percent.toFixed(1)}%) is critically high`,
            value: currentData.disk.usage_percent,
            threshold: defaultThresholds.disk_critical,
            trend: trends.disk.trend
        });
    } else if (currentData.disk.usage_percent >= defaultThresholds.disk_warning) {
        alerts.push({
            level: 'WARNING',
            resource: 'disk',
            message: `Disk usage (${currentData.disk.usage_percent.toFixed(1)}%) is high`,
            value: currentData.disk.usage_percent,
            threshold: defaultThresholds.disk_warning,
            trend: trends.disk.trend
        });
    }
    
    // File descriptor alerts
    if (currentData.file_descriptors.usage_percent >= defaultThresholds.fd_critical) {
        alerts.push({
            level: 'CRITICAL',
            resource: 'file_descriptors',
            message: `File descriptor usage (${currentData.file_descriptors.usage_percent.toFixed(1)}%) is critically high`,
            value: currentData.file_descriptors.usage_percent,
            threshold: defaultThresholds.fd_critical,
            trend: trends.file_descriptors.trend
        });
    }
    
    return alerts;
}

// Predict resource exhaustion
function predictResourceExhaustion(trends, currentData, hoursAhead = 24) {
    const predictions = {};
    
    ['cpu', 'memory', 'disk', 'file_descriptors'].forEach(resource => {
        const trend = trends[resource];
        if (!trend || trend.slope <= 0) {
            predictions[resource] = { status: 'stable_or_improving' };
            return;
        }
        
        // Simple linear extrapolation
        const currentValue = trend.current;
        const hourlyRate = trend.slope * 60; // Assuming slope is per minute, convert to hourly
        const predictedValue = currentValue + (hourlyRate * hoursAhead);
        
        const exhaustionThreshold = resource === 'cpu' ? 100 : 
                                  resource === 'memory' ? 95 : 
                                  resource === 'disk' ? 95 : 90;
        
        if (predictedValue >= exhaustionThreshold) {
            const hoursToExhaustion = (exhaustionThreshold - currentValue) / hourlyRate;
            predictions[resource] = {
                status: 'at_risk',
                predicted_value: Math.round(predictedValue * 100) / 100,
                hours_to_exhaustion: Math.max(0, Math.round(hoursToExhaustion * 100) / 100),
                severity: hoursToExhaustion < 4 ? 'critical' : 
                         hoursToExhaustion < 12 ? 'high' : 'medium'
            };
        } else {
            predictions[resource] = {
                status: 'healthy',
                predicted_value: Math.round(predictedValue * 100) / 100
            };
        }
    });
    
    return {
        prediction_horizon_hours: hoursAhead,
        predictions: predictions,
        overall_risk: Object.values(predictions).some(p => p.status === 'at_risk') ? 'at_risk' : 'healthy'
    };
}

// Format bytes for display
function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// Export for JDBX integration
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        trackResourceUtilization,
        analyzeResourceTrends,
        predictResourceExhaustion,
        formatBytes
    };
}