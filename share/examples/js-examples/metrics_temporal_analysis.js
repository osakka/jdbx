/**
 * JavaScript Temporal Analysis Functions for JDBX Metrics
 * Provides advanced time-series analysis, percentiles, and aggregations
 */

// Calculate percentiles for response times
function calculatePercentiles(values, percentiles = [50, 75, 90, 95, 99]) {
    if (!values || values.length === 0) {
        return percentiles.reduce((acc, p) => ({ ...acc, [`p${p}`]: 0 }), {});
    }
    
    const sorted = [...values].sort((a, b) => a - b);
    const result = {};
    
    percentiles.forEach(percentile => {
        const index = (percentile / 100) * (sorted.length - 1);
        if (index === Math.floor(index)) {
            result[`p${percentile}`] = sorted[index];
        } else {
            const lower = sorted[Math.floor(index)];
            const upper = sorted[Math.ceil(index)];
            result[`p${percentile}`] = lower + (upper - lower) * (index - Math.floor(index));
        }
    });
    
    return result;
}

// Moving average calculator
function calculateMovingAverages(timeSeries, windows = [5, 15, 30]) {
    const result = {
        timestamps: timeSeries.map(d => d.timestamp),
        values: timeSeries.map(d => d.value),
        moving_averages: {}
    };
    
    windows.forEach(window => {
        const ma = [];
        for (let i = 0; i < timeSeries.length; i++) {
            const start = Math.max(0, i - window + 1);
            const end = i + 1;
            const windowData = timeSeries.slice(start, end);
            const average = windowData.reduce((sum, d) => sum + d.value, 0) / windowData.length;
            ma.push(average);
        }
        result.moving_averages[`ma${window}`] = ma;
    });
    
    return result;
}

// Rate calculation (operations per second/minute)
function calculateRates(timeSeries, unit = 'minute') {
    if (timeSeries.length < 2) {
        return { rates: [], average_rate: 0 };
    }
    
    const rates = [];
    const multiplier = unit === 'second' ? 1 : unit === 'minute' ? 60 : 3600; // hour
    
    for (let i = 1; i < timeSeries.length; i++) {
        const prev = timeSeries[i - 1];
        const curr = timeSeries[i];
        
        const timeDiff = (new Date(curr.timestamp) - new Date(prev.timestamp)) / 1000; // seconds
        const valueDiff = curr.value - prev.value;
        
        if (timeDiff > 0) {
            const rate = (valueDiff / timeDiff) * multiplier;
            rates.push({
                timestamp: curr.timestamp,
                rate: rate,
                period_seconds: timeDiff
            });
        }
    }
    
    const averageRate = rates.length > 0 ? 
        rates.reduce((sum, r) => sum + r.rate, 0) / rates.length : 0;
    
    return {
        rates: rates,
        average_rate: averageRate,
        max_rate: rates.length > 0 ? Math.max(...rates.map(r => r.rate)) : 0,
        min_rate: rates.length > 0 ? Math.min(...rates.map(r => r.rate)) : 0,
        unit: `per_${unit}`
    };
}

// Time-based aggregation (hourly, daily, weekly)
function aggregateByTimeWindow(timeSeries, windowSize = 'hour') {
    const aggregated = {};
    
    // Define window size in milliseconds
    const windowMs = {
        'minute': 60 * 1000,
        'hour': 60 * 60 * 1000,
        'day': 24 * 60 * 60 * 1000,
        'week': 7 * 24 * 60 * 60 * 1000
    }[windowSize] || 60 * 60 * 1000; // Default to hour
    
    timeSeries.forEach(dataPoint => {
        const timestamp = new Date(dataPoint.timestamp);
        const windowStart = new Date(Math.floor(timestamp.getTime() / windowMs) * windowMs);
        const windowKey = windowStart.toISOString();
        
        if (!aggregated[windowKey]) {
            aggregated[windowKey] = {
                window_start: windowKey,
                window_size: windowSize,
                count: 0,
                sum: 0,
                min: Infinity,
                max: -Infinity,
                values: []
            };
        }
        
        const bucket = aggregated[windowKey];
        bucket.count++;
        bucket.sum += dataPoint.value;
        bucket.min = Math.min(bucket.min, dataPoint.value);
        bucket.max = Math.max(bucket.max, dataPoint.value);
        bucket.values.push(dataPoint.value);
    });
    
    // Calculate final statistics for each window
    Object.values(aggregated).forEach(bucket => {
        bucket.average = bucket.count > 0 ? bucket.sum / bucket.count : 0;
        bucket.median = calculateMedian(bucket.values);
        bucket.std_dev = calculateStandardDeviation(bucket.values);
        bucket.percentiles = calculatePercentiles(bucket.values);
        delete bucket.values; // Remove raw values to save space
    });
    
    return Object.values(aggregated).sort((a, b) => 
        new Date(a.window_start) - new Date(b.window_start)
    );
}

// Median calculation
function calculateMedian(values) {
    if (values.length === 0) return 0;
    
    const sorted = [...values].sort((a, b) => a - b);
    const mid = Math.floor(sorted.length / 2);
    
    return sorted.length % 2 === 0 ? 
        (sorted[mid - 1] + sorted[mid]) / 2 : 
        sorted[mid];
}

// Standard deviation calculation
function calculateStandardDeviation(values) {
    if (values.length === 0) return 0;
    
    const mean = values.reduce((sum, val) => sum + val, 0) / values.length;
    const squaredDiffs = values.map(val => Math.pow(val - mean, 2));
    const variance = squaredDiffs.reduce((sum, diff) => sum + diff, 0) / values.length;
    
    return Math.sqrt(variance);
}

// Anomaly detection using statistical methods
function detectAnomalies(timeSeries, sensitivity = 2) {
    if (timeSeries.length < 10) {
        return { anomalies: [], statistics: null };
    }
    
    const values = timeSeries.map(d => d.value);
    const mean = values.reduce((sum, val) => sum + val, 0) / values.length;
    const stdDev = calculateStandardDeviation(values);
    
    const threshold = sensitivity * stdDev;
    const anomalies = [];
    
    timeSeries.forEach((dataPoint, index) => {
        const deviation = Math.abs(dataPoint.value - mean);
        if (deviation > threshold) {
            anomalies.push({
                timestamp: dataPoint.timestamp,
                value: dataPoint.value,
                expected_range: {
                    min: mean - threshold,
                    max: mean + threshold
                },
                deviation: deviation,
                severity: deviation > (threshold * 2) ? 'high' : 'medium',
                z_score: stdDev > 0 ? (dataPoint.value - mean) / stdDev : 0
            });
        }
    });
    
    return {
        anomalies: anomalies,
        statistics: {
            mean: mean,
            std_dev: stdDev,
            threshold: threshold,
            sensitivity: sensitivity,
            total_points: timeSeries.length,
            anomaly_rate: anomalies.length / timeSeries.length
        }
    };
}

// Seasonal pattern detection
function detectSeasonalPatterns(timeSeries, periods = ['hour', 'day', 'week']) {
    const patterns = {};
    
    periods.forEach(period => {
        const periodData = {};
        
        timeSeries.forEach(dataPoint => {
            const timestamp = new Date(dataPoint.timestamp);
            let key;
            
            switch (period) {
                case 'hour':
                    key = timestamp.getUTCHours();
                    break;
                case 'day':
                    key = timestamp.getUTCDay(); // 0 = Sunday, 6 = Saturday
                    break;
                case 'week':
                    const weekNumber = Math.floor((timestamp - new Date(timestamp.getFullYear(), 0, 1)) / (7 * 24 * 60 * 60 * 1000));
                    key = weekNumber % 4; // 4-week cycle
                    break;
                default:
                    key = 0;
            }
            
            if (!periodData[key]) {
                periodData[key] = [];
            }
            periodData[key].push(dataPoint.value);
        });
        
        // Calculate statistics for each period bucket
        const periodStats = {};
        Object.entries(periodData).forEach(([key, values]) => {
            periodStats[key] = {
                count: values.length,
                average: values.reduce((sum, val) => sum + val, 0) / values.length,
                median: calculateMedian(values),
                min: Math.min(...values),
                max: Math.max(...values),
                std_dev: calculateStandardDeviation(values)
            };
        });
        
        patterns[period] = {
            period_type: period,
            buckets: periodStats,
            variation_coefficient: calculateVariationCoefficient(Object.values(periodStats).map(s => s.average))
        };
    });
    
    return patterns;
}

// Coefficient of variation
function calculateVariationCoefficient(values) {
    if (values.length === 0) return 0;
    
    const mean = values.reduce((sum, val) => sum + val, 0) / values.length;
    const stdDev = calculateStandardDeviation(values);
    
    return mean !== 0 ? stdDev / mean : 0;
}

// Forecast using simple linear regression
function forecastLinearTrend(timeSeries, periodsAhead = 12) {
    if (timeSeries.length < 2) {
        return { forecast: [], confidence: 'low', method: 'insufficient_data' };
    }
    
    // Convert timestamps to numeric values (hours since first data point)
    const firstTimestamp = new Date(timeSeries[0].timestamp).getTime();
    const data = timeSeries.map(d => ({
        x: (new Date(d.timestamp).getTime() - firstTimestamp) / (1000 * 60 * 60), // hours
        y: d.value
    }));
    
    // Calculate linear regression
    const n = data.length;
    const sumX = data.reduce((sum, d) => sum + d.x, 0);
    const sumY = data.reduce((sum, d) => sum + d.y, 0);
    const sumXY = data.reduce((sum, d) => sum + d.x * d.y, 0);
    const sumXX = data.reduce((sum, d) => sum + d.x * d.x, 0);
    
    const slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX);
    const intercept = (sumY - slope * sumX) / n;
    
    // Calculate R-squared for confidence measure
    const yMean = sumY / n;
    const ssRes = data.reduce((sum, d) => {
        const predicted = slope * d.x + intercept;
        return sum + Math.pow(d.y - predicted, 2);
    }, 0);
    const ssTot = data.reduce((sum, d) => sum + Math.pow(d.y - yMean, 2), 0);
    const rSquared = 1 - (ssRes / ssTot);
    
    // Generate forecast
    const lastX = data[data.length - 1].x;
    const forecast = [];
    
    for (let i = 1; i <= periodsAhead; i++) {
        const futureX = lastX + i;
        const predictedY = slope * futureX + intercept;
        const futureTimestamp = new Date(firstTimestamp + futureX * 60 * 60 * 1000);
        
        forecast.push({
            timestamp: futureTimestamp.toISOString(),
            predicted_value: Math.max(0, predictedY), // Ensure non-negative
            period_ahead: i
        });
    }
    
    return {
        forecast: forecast,
        confidence: rSquared > 0.8 ? 'high' : rSquared > 0.5 ? 'medium' : 'low',
        r_squared: rSquared,
        slope: slope,
        method: 'linear_regression',
        periods_ahead: periodsAhead
    };
}

// Export for JDBX integration
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        calculatePercentiles,
        calculateMovingAverages,
        calculateRates,
        aggregateByTimeWindow,
        detectAnomalies,
        detectSeasonalPatterns,
        forecastLinearTrend
    };
}