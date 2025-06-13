/**
 * JavaScript Database Performance Metrics for JDBX
 * Provides query analysis, index optimization, and performance trend detection
 */

// Database operation performance tracker
function trackDatabaseOperation(operation, collection, duration, metadata = {}) {
    const perfMetric = {
        timestamp: new Date().toISOString(),
        operation: operation, // 'find', 'insert', 'update', 'delete', 'index'
        collection: collection,
        duration_ms: duration,
        document_count: metadata.document_count || 0,
        bytes_processed: metadata.bytes_processed || 0,
        index_used: metadata.index_used || false,
        index_name: metadata.index_name || null,
        query_complexity: calculateQueryComplexity(metadata.query || {}),
        cache_hit: metadata.cache_hit || false
    };

    return perfMetric;
}

// Calculate query complexity score
function calculateQueryComplexity(query) {
    let complexity = 1;
    
    // Count query conditions
    const conditions = Object.keys(query).length;
    complexity += conditions * 0.5;
    
    // Check for regex patterns
    Object.values(query).forEach(value => {
        if (value && value.$regex) complexity += 2;
        if (value && value.$in && Array.isArray(value.$in)) {
            complexity += value.$in.length * 0.1;
        }
        if (value && (value.$lt || value.$gt || value.$lte || value.$gte)) {
            complexity += 0.5;
        }
    });
    
    return Math.round(complexity * 100) / 100;
}

// Performance trend analysis
function analyzePerformanceTrends(performanceHistory, collection = null) {
    let data = performanceHistory;
    
    // Filter by collection if specified
    if (collection) {
        data = performanceHistory.filter(p => p.collection === collection);
    }
    
    if (data.length < 2) {
        return { trend: 'insufficient_data', analysis: 'Need at least 2 data points' };
    }
    
    // Group by operation type
    const byOperation = {};
    data.forEach(record => {
        if (!byOperation[record.operation]) {
            byOperation[record.operation] = [];
        }
        byOperation[record.operation].push(record);
    });
    
    const trends = {};
    
    Object.entries(byOperation).forEach(([operation, records]) => {
        if (records.length < 2) return;
        
        // Sort by timestamp
        records.sort((a, b) => new Date(a.timestamp) - new Date(b.timestamp));
        
        // Calculate moving averages
        const durations = records.map(r => r.duration_ms);
        const recent = durations.slice(-5); // Last 5 operations
        const older = durations.slice(0, -5);
        
        const recentAvg = recent.reduce((a, b) => a + b, 0) / recent.length;
        const olderAvg = older.length > 0 ? older.reduce((a, b) => a + b, 0) / older.length : recentAvg;
        
        const percentChange = older.length > 0 ? ((recentAvg - olderAvg) / olderAvg) * 100 : 0;
        
        trends[operation] = {
            recent_avg_ms: recentAvg,
            older_avg_ms: olderAvg,
            percent_change: Math.round(percentChange * 100) / 100,
            trend: percentChange > 10 ? 'degrading' : 
                   percentChange < -10 ? 'improving' : 'stable',
            total_operations: records.length,
            slowest_ms: Math.max(...durations),
            fastest_ms: Math.min(...durations)
        };
    });
    
    return trends;
}

// Index effectiveness calculator
function calculateIndexEffectiveness(queryHistory, indexUsage) {
    const effectiveness = {};
    
    // Group queries by collection
    const byCollection = {};
    queryHistory.forEach(query => {
        if (!byCollection[query.collection]) {
            byCollection[query.collection] = [];
        }
        byCollection[query.collection].push(query);
    });
    
    Object.entries(byCollection).forEach(([collection, queries]) => {
        const totalQueries = queries.length;
        const indexedQueries = queries.filter(q => q.index_used).length;
        const avgDurationWithIndex = queries
            .filter(q => q.index_used)
            .reduce((sum, q) => sum + q.duration_ms, 0) / (indexedQueries || 1);
        const avgDurationWithoutIndex = queries
            .filter(q => !q.index_used)
            .reduce((sum, q) => sum + q.duration_ms, 0) / (totalQueries - indexedQueries || 1);
            
        effectiveness[collection] = {
            total_queries: totalQueries,
            indexed_queries: indexedQueries,
            index_hit_rate: totalQueries > 0 ? (indexedQueries / totalQueries) : 0,
            avg_duration_with_index: avgDurationWithIndex,
            avg_duration_without_index: avgDurationWithoutIndex,
            performance_improvement: avgDurationWithoutIndex > 0 ? 
                ((avgDurationWithoutIndex - avgDurationWithIndex) / avgDurationWithoutIndex) : 0,
            recommendation: generateIndexRecommendation(queries)
        };
    });
    
    return effectiveness;
}

// Generate index recommendations
function generateIndexRecommendation(queries) {
    const fieldFrequency = {};
    
    queries.forEach(query => {
        if (query.duration_ms > 100 && !query.index_used) { // Slow queries without index
            const queryFields = extractQueryFields(query.query || {});
            queryFields.forEach(field => {
                fieldFrequency[field] = (fieldFrequency[field] || 0) + 1;
            });
        }
    });
    
    const recommendations = Object.entries(fieldFrequency)
        .filter(([field, count]) => count >= 3) // Field used in 3+ slow queries
        .sort((a, b) => b[1] - a[1])
        .slice(0, 3) // Top 3 recommendations
        .map(([field, count]) => ({
            field: field,
            query_count: count,
            priority: count >= 10 ? 'high' : count >= 5 ? 'medium' : 'low'
        }));
    
    return recommendations;
}

// Extract fields from query object
function extractQueryFields(query) {
    const fields = [];
    
    function extractFromObject(obj, prefix = '') {
        Object.keys(obj).forEach(key => {
            if (key.startsWith('$')) return; // Skip operators
            
            const fullKey = prefix ? `${prefix}.${key}` : key;
            fields.push(fullKey);
            
            if (typeof obj[key] === 'object' && obj[key] !== null && !Array.isArray(obj[key])) {
                extractFromObject(obj[key], fullKey);
            }
        });
    }
    
    extractFromObject(query);
    return [...new Set(fields)]; // Remove duplicates
}

// Database health score calculator
function calculateDatabaseHealthScore(metrics) {
    let score = 100;
    let factors = [];
    
    // Performance factor (40% of score)
    const avgResponseTime = metrics.avg_response_time_ms || 0;
    if (avgResponseTime > 1000) {
        score -= 30;
        factors.push('High average response time');
    } else if (avgResponseTime > 500) {
        score -= 15;
        factors.push('Moderate response time');
    }
    
    // Error rate factor (30% of score)
    const errorRate = metrics.error_rate || 0;
    if (errorRate > 5) {
        score -= 25;
        factors.push('High error rate');
    } else if (errorRate > 1) {
        score -= 10;
        factors.push('Moderate error rate');
    }
    
    // Index effectiveness factor (20% of score)
    const indexHitRate = metrics.index_hit_rate || 0;
    if (indexHitRate < 0.5) {
        score -= 15;
        factors.push('Low index utilization');
    } else if (indexHitRate < 0.8) {
        score -= 8;
        factors.push('Suboptimal index usage');
    }
    
    // Cache effectiveness factor (10% of score)
    const cacheHitRate = metrics.cache_hit_rate || 0;
    if (cacheHitRate < 0.3) {
        score -= 8;
        factors.push('Poor cache performance');
    }
    
    return {
        score: Math.max(0, score),
        grade: score >= 90 ? 'A' : score >= 80 ? 'B' : score >= 70 ? 'C' : score >= 60 ? 'D' : 'F',
        factors: factors,
        recommendations: generateHealthRecommendations(score, factors)
    };
}

function generateHealthRecommendations(score, factors) {
    const recommendations = [];
    
    if (factors.includes('High average response time')) {
        recommendations.push('Consider adding indexes for frequently queried fields');
        recommendations.push('Review query complexity and optimize where possible');
    }
    
    if (factors.includes('High error rate')) {
        recommendations.push('Investigate error sources and implement proper error handling');
        recommendations.push('Monitor resource utilization for capacity issues');
    }
    
    if (factors.includes('Low index utilization')) {
        recommendations.push('Analyze query patterns and create appropriate indexes');
        recommendations.push('Remove unused indexes to improve write performance');
    }
    
    if (factors.includes('Poor cache performance')) {
        recommendations.push('Increase cache size or adjust cache policies');
        recommendations.push('Review cache key patterns for optimization');
    }
    
    return recommendations;
}

// Export for JDBX integration
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        trackDatabaseOperation,
        analyzePerformanceTrends,
        calculateIndexEffectiveness,
        calculateDatabaseHealthScore,
        calculateQueryComplexity
    };
}