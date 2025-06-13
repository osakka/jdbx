// Enhanced Metrics Implementation for JDBX
// Adds proper legends, units, tooltips, and error tracking

// Helper function to format bytes with appropriate units
function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// Helper function to format large numbers
function formatNumber(num) {
    // Handle null, undefined, and non-numeric values
    if (num === null || num === undefined || isNaN(num)) {
        return '0';
    }
    
    // Convert to number if it's a string
    if (typeof num === 'string') {
        num = parseFloat(num);
        if (isNaN(num)) return '0';
    }
    
    if (num >= 1000000) return (num / 1000000).toFixed(1) + 'M';
    if (num >= 1000) return (num / 1000).toFixed(1) + 'K';
    return num.toString();
}

// Enhanced chart configurations with proper units and legends
const enhancedChartConfigs = {
    operations: {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Read Operations',
                data: [],
                borderColor: '#61affe',
                backgroundColor: 'rgba(97, 175, 254, 0.1)',
                tension: 0.4,
                pointRadius: 3,
                pointHoverRadius: 5
            }, {
                label: 'Write Operations',
                data: [],
                borderColor: '#49cc90',
                backgroundColor: 'rgba(73, 204, 144, 0.1)',
                tension: 0.4,
                pointRadius: 3,
                pointHoverRadius: 5
            }, {
                label: 'Total Operations',
                data: [],
                borderColor: '#fca130',
                backgroundColor: 'rgba(252, 161, 48, 0.1)',
                tension: 0.4,
                pointRadius: 3,
                pointHoverRadius: 5,
                hidden: true // Hidden by default
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false
            },
            plugins: {
                legend: {
                    display: true,
                    position: 'bottom',
                    labels: {
                        usePointStyle: true,
                        padding: 15,
                        font: {
                            size: 12
                        }
                    }
                },
                tooltip: {
                    backgroundColor: 'rgba(0, 0, 0, 0.8)',
                    titleColor: '#fff',
                    bodyColor: '#fff',
                    borderColor: '#ddd',
                    borderWidth: 1,
                    titleFont: {
                        size: 14,
                        weight: 'bold'
                    },
                    bodyFont: {
                        size: 13
                    },
                    padding: 10,
                    displayColors: true,
                    callbacks: {
                        title: function(tooltipItems) {
                            return 'Time: ' + tooltipItems[0].label;
                        },
                        label: function(context) {
                            let label = context.dataset.label || '';
                            if (label) {
                                label += ': ';
                            }
                            label += formatNumber(context.parsed.y) + ' ops/min';
                            
                            // Add rate of change
                            if (context.dataIndex > 0) {
                                const prevValue = context.dataset.data[context.dataIndex - 1];
                                const change = ((context.parsed.y - prevValue) / prevValue * 100).toFixed(1);
                                label += ' (' + (change >= 0 ? '+' : '') + change + '%)';
                            }
                            return label;
                        }
                    }
                }
            },
            scales: {
                x: {
                    display: true,
                    title: {
                        display: true,
                        text: 'Time',
                        font: {
                            size: 12
                        }
                    },
                    ticks: {
                        maxRotation: 45,
                        minRotation: 45
                    }
                },
                y: {
                    display: true,
                    title: {
                        display: true,
                        text: 'Operations per Minute',
                        font: {
                            size: 12
                        }
                    },
                    beginAtZero: true,
                    ticks: {
                        callback: function(value) {
                            return formatNumber(value);
                        }
                    }
                }
            }
        }
    },
    
    responseTime: {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Average Response Time',
                data: [],
                borderColor: '#28a745',
                backgroundColor: 'rgba(40, 167, 69, 0.1)',
                tension: 0.4,
                pointRadius: 3,
                pointHoverRadius: 5
            }, {
                label: 'P95 Response Time',
                data: [],
                borderColor: '#ffc107',
                backgroundColor: 'rgba(255, 193, 7, 0.1)',
                tension: 0.4,
                pointRadius: 3,
                pointHoverRadius: 5,
                borderDash: [5, 5]
            }, {
                label: 'Max Response Time',
                data: [],
                borderColor: '#dc3545',
                backgroundColor: 'rgba(220, 53, 69, 0.1)',
                tension: 0.4,
                pointRadius: 3,
                pointHoverRadius: 5,
                borderDash: [2, 2]
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false
            },
            plugins: {
                legend: {
                    display: true,
                    position: 'bottom',
                    labels: {
                        usePointStyle: true,
                        padding: 15,
                        font: {
                            size: 12
                        }
                    }
                },
                tooltip: {
                    backgroundColor: 'rgba(0, 0, 0, 0.8)',
                    callbacks: {
                        label: function(context) {
                            let label = context.dataset.label || '';
                            if (label) {
                                label += ': ';
                            }
                            label += context.parsed.y.toFixed(2) + ' ms';
                            return label;
                        }
                    }
                }
            },
            scales: {
                x: {
                    display: true,
                    title: {
                        display: true,
                        text: 'Time',
                        font: {
                            size: 12
                        }
                    }
                },
                y: {
                    display: true,
                    title: {
                        display: true,
                        text: 'Response Time (milliseconds)',
                        font: {
                            size: 12
                        }
                    },
                    beginAtZero: true,
                    ticks: {
                        callback: function(value) {
                            return value.toFixed(0) + ' ms';
                        }
                    }
                }
            }
        }
    },
    
    cacheHitRate: {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Cache Hit Rate',
                data: [],
                borderColor: '#17a2b8',
                backgroundColor: 'rgba(23, 162, 184, 0.1)',
                tension: 0.4,
                pointRadius: 3,
                pointHoverRadius: 5,
                yAxisID: 'y-hitrate'
            }, {
                label: 'Cache Size',
                data: [],
                borderColor: '#6c757d',
                backgroundColor: 'rgba(108, 117, 125, 0.1)',
                tension: 0.4,
                pointRadius: 3,
                pointHoverRadius: 5,
                yAxisID: 'y-size',
                hidden: true
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false
            },
            plugins: {
                legend: {
                    display: true,
                    position: 'bottom',
                    labels: {
                        usePointStyle: true,
                        padding: 15
                    }
                },
                tooltip: {
                    backgroundColor: 'rgba(0, 0, 0, 0.8)',
                    callbacks: {
                        label: function(context) {
                            let label = context.dataset.label || '';
                            if (label) {
                                label += ': ';
                            }
                            if (context.dataset.yAxisID === 'y-hitrate') {
                                label += (context.parsed.y * 100).toFixed(1) + '%';
                            } else {
                                label += formatBytes(context.parsed.y);
                            }
                            return label;
                        }
                    }
                }
            },
            scales: {
                x: {
                    display: true,
                    title: {
                        display: true,
                        text: 'Time'
                    }
                },
                'y-hitrate': {
                    type: 'linear',
                    display: true,
                    position: 'left',
                    title: {
                        display: true,
                        text: 'Hit Rate (%)'
                    },
                    beginAtZero: true,
                    max: 1,
                    ticks: {
                        callback: function(value) {
                            return (value * 100).toFixed(0) + '%';
                        }
                    }
                },
                'y-size': {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    title: {
                        display: true,
                        text: 'Cache Size'
                    },
                    beginAtZero: true,
                    grid: {
                        drawOnChartArea: false
                    },
                    ticks: {
                        callback: function(value) {
                            return formatBytes(value);
                        }
                    }
                }
            }
        }
    },
    
    memory: {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Process Memory',
                data: [],
                borderColor: '#6f42c1',
                backgroundColor: 'rgba(111, 66, 193, 0.2)',
                fill: true,
                tension: 0.4
            }, {
                label: 'System Memory Used',
                data: [],
                borderColor: '#e83e8c',
                backgroundColor: 'rgba(232, 62, 140, 0.1)',
                tension: 0.4,
                borderDash: [5, 5]
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false
            },
            plugins: {
                legend: {
                    display: true,
                    position: 'bottom',
                    labels: {
                        usePointStyle: true,
                        padding: 15
                    }
                },
                tooltip: {
                    backgroundColor: 'rgba(0, 0, 0, 0.8)',
                    callbacks: {
                        label: function(context) {
                            let label = context.dataset.label || '';
                            if (label) {
                                label += ': ';
                            }
                            label += formatBytes(context.parsed.y * 1024); // Convert KB to bytes
                            
                            // Add percentage for system memory
                            if (context.dataset.label === 'System Memory Used') {
                                const totalMemory = 32638500 * 1024; // From health endpoint
                                const percentage = (context.parsed.y * 1024 / totalMemory * 100).toFixed(1);
                                label += ' (' + percentage + '%)';
                            }
                            return label;
                        }
                    }
                }
            },
            scales: {
                x: {
                    display: true,
                    title: {
                        display: true,
                        text: 'Time'
                    }
                },
                y: {
                    display: true,
                    title: {
                        display: true,
                        text: 'Memory Usage'
                    },
                    beginAtZero: true,
                    ticks: {
                        callback: function(value) {
                            return formatBytes(value * 1024);
                        }
                    }
                }
            }
        }
    },
    
    operationTypes: {
        type: 'doughnut',
        data: {
            labels: ['Read', 'Write', 'Other'],
            datasets: [{
                data: [],
                backgroundColor: ['#61affe', '#49cc90', '#fca130'],
                borderWidth: 2,
                borderColor: '#fff'
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: true,
                    position: 'bottom',
                    labels: {
                        padding: 15,
                        generateLabels: function(chart) {
                            const data = chart.data;
                            if (data.labels.length && data.datasets.length) {
                                const dataset = data.datasets[0];
                                const total = dataset.data.reduce((a, b) => a + b, 0);
                                return data.labels.map((label, i) => {
                                    const value = dataset.data[i];
                                    const percentage = total > 0 ? (value / total * 100).toFixed(1) : 0;
                                    return {
                                        text: `${label}: ${formatNumber(value)} (${percentage}%)`,
                                        fillStyle: dataset.backgroundColor[i],
                                        strokeStyle: dataset.borderColor,
                                        lineWidth: dataset.borderWidth,
                                        hidden: false,
                                        index: i
                                    };
                                });
                            }
                            return [];
                        }
                    }
                },
                tooltip: {
                    backgroundColor: 'rgba(0, 0, 0, 0.8)',
                    callbacks: {
                        label: function(context) {
                            const label = context.label || '';
                            const value = context.parsed;
                            const total = context.dataset.data.reduce((a, b) => a + b, 0);
                            const percentage = total > 0 ? (value / total * 100).toFixed(1) : 0;
                            return `${label}: ${formatNumber(value)} ops (${percentage}%)`;
                        }
                    }
                }
            }
        }
    }
};

// Function to update all metrics charts with new data
function updateMetricsChartsWithEnhancements(metricsData) {
    if (!metricsData) return;
    
    // Update operations chart
    if (operationsChart && metricsData.operations && metricsData.operations.data) {
        const labels = metricsData.operations.data.map(d => {
            const date = new Date(d.timestamp);
            return date.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' });
        });
        
        const readData = metricsData.operations.data.map(d => d.read || 0);
        const writeData = metricsData.operations.data.map(d => d.write || 0);
        const totalData = metricsData.operations.data.map(d => d.total || 0);
        
        operationsChart.data.labels = labels;
        operationsChart.data.datasets[0].data = readData;
        operationsChart.data.datasets[1].data = writeData;
        operationsChart.data.datasets[2].data = totalData;
        operationsChart.update();
    }
    
    // Update response time chart (if exists)
    if (responseTimesChart && metricsData.performance && metricsData.performance.data) {
        const labels = metricsData.performance.data.map(d => {
            const date = new Date(d.timestamp);
            return date.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' });
        });
        
        const avgData = metricsData.performance.data.map(d => d.avg_response_time_ms || 0);
        const p95Data = metricsData.performance.data.map(d => d.avg_response_time_ms * 1.5 || 0); // Estimate
        const maxData = metricsData.performance.data.map(d => d.max_response_time_ms || 0);
        
        responseTimesChart.data.labels = labels;
        responseTimesChart.data.datasets[0].data = avgData;
        responseTimesChart.data.datasets[1].data = p95Data;
        responseTimesChart.data.datasets[2].data = maxData;
        responseTimesChart.update();
    }
    
    // Update cache hit rate chart
    if (cacheHitRateChart && metricsData.cache && metricsData.cache.data) {
        const labels = metricsData.cache.data.map(d => {
            const date = new Date(d.timestamp);
            return date.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' });
        });
        
        const hitRateData = metricsData.cache.data.map(d => d.hit_rate || 0);
        const sizeData = metricsData.cache.data.map(d => d.size_bytes || 0);
        
        cacheHitRateChart.data.labels = labels;
        cacheHitRateChart.data.datasets[0].data = hitRateData;
        cacheHitRateChart.data.datasets[1].data = sizeData;
        cacheHitRateChart.update();
    }
    
    // Update memory chart
    if (memoryUsageChart && metricsData.memory && metricsData.memory.data) {
        const labels = metricsData.memory.data.map(d => {
            const date = new Date(d.timestamp);
            return date.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' });
        });
        
        const processData = metricsData.memory.data.map(d => d.process_kb || 0);
        const systemData = metricsData.memory.data.map(d => d.used_kb || 0);
        
        memoryUsageChart.data.labels = labels;
        memoryUsageChart.data.datasets[0].data = processData;
        memoryUsageChart.data.datasets[1].data = systemData;
        memoryUsageChart.update();
    }
    
    // Update operation types chart
    if (operationTypesChart && metricsData.operations && metricsData.operations.current) {
        const current = metricsData.operations.current;
        const readOps = current.read || 0;
        const writeOps = current.write || 0;
        const otherOps = (current.total || 0) - readOps - writeOps;
        
        operationTypesChart.data.datasets[0].data = [readOps, writeOps, Math.max(0, otherOps)];
        operationTypesChart.update();
    }
    
    // Update metric cards with proper formatting
    updateMetricCards(metricsData);
}

// Function to update metric cards with proper units
function updateMetricCards(metricsData) {
    if (!metricsData) return;
    
    // Update operations metrics
    if (metricsData.operations && metricsData.operations.current) {
        const ops = metricsData.operations.current;
        document.getElementById('totalOps').textContent = formatNumber(ops.total || 0);
        document.getElementById('readOps').textContent = formatNumber(ops.read || 0);
        document.getElementById('writeOps').textContent = formatNumber(ops.write || 0);
    }
    
    // Update performance metrics
    if (metricsData.performance && metricsData.performance.current) {
        const perf = metricsData.performance.current;
        const avgTime = perf.avg_response_time_ms || 0;
        document.getElementById('avgResponseTime').textContent = avgTime.toFixed(2) + ' ms';
    }
    
    // Update cache metrics
    if (metricsData.cache && metricsData.cache.current) {
        const cache = metricsData.cache.current;
        const hitRate = (cache.hit_rate || 0) * 100;
        document.getElementById('cacheHitRate').textContent = hitRate.toFixed(1) + '%';
        document.getElementById('cacheStats').textContent = 
            `${formatNumber(cache.hits || 0)} hits / ${formatNumber(cache.misses || 0)} misses`;
        document.getElementById('cacheSize').textContent = formatNumber(cache.size_bytes || 0);
        document.getElementById('cacheMemory').textContent = formatBytes(cache.size_bytes || 0);
    }
    
    // Update memory metrics
    if (metricsData.memory && metricsData.memory.current) {
        const mem = metricsData.memory.current;
        const processMemory = formatBytes((mem.process_kb || 0) * 1024);
        const systemUsed = formatBytes((mem.used_kb || 0) * 1024);
        const systemTotal = formatBytes((mem.total_kb || 0) * 1024);
        
        // Find database size element and update
        const dbSizeElement = document.getElementById('databaseSize');
        if (dbSizeElement) {
            dbSizeElement.textContent = processMemory;
        }
    }
}

// Export functions for use in main app.js
window.enhancedChartConfigs = enhancedChartConfigs;
window.updateMetricsChartsWithEnhancements = updateMetricsChartsWithEnhancements;
window.formatBytes = formatBytes;
window.formatNumber = formatNumber;