/**
 * JDBX Chart Management System
 * Centralized chart lifecycle management with proper cleanup
 */

import { state } from '../core/state.js';

/**
 * Chart configuration defaults
 */
const DEFAULT_CHART_OPTIONS = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
        legend: {
            display: true,
            position: 'top'
        }
    },
    scales: {
        y: {
            beginAtZero: true
        }
    }
};

/**
 * Create or update chart with proper lifecycle management
 * @param {string} chartId - Chart identifier
 * @param {string} canvasId - Canvas element ID
 * @param {string} type - Chart type (line, bar, pie, etc.)
 * @param {object} data - Chart data
 * @param {object} options - Chart options (optional)
 * @returns {object} Chart.js instance
 */
export function createChart(chartId, canvasId, type, data, options = {}) {
    // Destroy existing chart if it exists
    destroyChart(chartId);

    // Get canvas element
    const canvas = document.getElementById(canvasId);
    if (!canvas) {
        throw new Error(`Canvas element not found: ${canvasId}`);
    }

    // Get 2D context
    const ctx = canvas.getContext('2d');
    if (!ctx) {
        throw new Error(`Cannot get 2D context for canvas: ${canvasId}`);
    }

    // Merge options with defaults
    const chartOptions = {
        ...DEFAULT_CHART_OPTIONS,
        ...options,
        plugins: {
            ...DEFAULT_CHART_OPTIONS.plugins,
            ...options.plugins
        }
    };

    // Apply theme-specific options
    applyThemeToChartOptions(chartOptions);

    // Create new chart
    const chart = new Chart(ctx, {
        type,
        data,
        options: chartOptions
    });

    // Store chart instance in state
    state.addChart(chartId, chart);

    return chart;
}

/**
 * Update existing chart data
 * @param {string} chartId - Chart identifier
 * @param {object} newData - New chart data
 */
export function updateChart(chartId, newData) {
    const chart = getChart(chartId);
    
    if (!chart) {
        console.warn(`Chart not found: ${chartId}`);
        return;
    }

    // Update data
    chart.data = newData;
    
    // Trigger re-render
    chart.update();
}

/**
 * Update chart data with animation
 * @param {string} chartId - Chart identifier
 * @param {object} newData - New chart data
 * @param {string} animationMode - Animation mode ('active', 'resize', 'show', 'hide')
 */
export function updateChartAnimated(chartId, newData, animationMode = 'active') {
    const chart = getChart(chartId);
    
    if (!chart) {
        console.warn(`Chart not found: ${chartId}`);
        return;
    }

    // Update data
    chart.data = newData;
    
    // Trigger animated re-render
    chart.update(animationMode);
}

/**
 * Get chart instance by ID
 * @param {string} chartId - Chart identifier
 * @returns {object|null} Chart.js instance or null
 */
export function getChart(chartId) {
    const charts = state.get('chartInstances');
    return charts.get(chartId) || null;
}

/**
 * Destroy specific chart
 * @param {string} chartId - Chart identifier
 */
export function destroyChart(chartId) {
    state.removeChart(chartId);
}

/**
 * Destroy all charts
 */
export function destroyAllCharts() {
    state.clearAllCharts();
}

/**
 * Apply current theme to chart options
 * @param {object} options - Chart options to modify
 */
function applyThemeToChartOptions(options) {
    const isDarkMode = document.body.getAttribute('data-theme') === 'dark';
    
    if (isDarkMode) {
        // Dark theme colors
        const darkColors = {
            gridColor: '#404040',
            textColor: '#e1e1e1',
            borderColor: '#404040'
        };

        // Apply dark theme to scales
        if (options.scales) {
            Object.keys(options.scales).forEach(scaleKey => {
                const scale = options.scales[scaleKey];
                if (scale.grid) {
                    scale.grid.color = darkColors.gridColor;
                }
                if (scale.ticks) {
                    scale.ticks.color = darkColors.textColor;
                }
            });
        }

        // Apply dark theme to plugins
        if (options.plugins) {
            if (options.plugins.legend) {
                options.plugins.legend.labels = {
                    ...options.plugins.legend.labels,
                    color: darkColors.textColor
                };
            }

            if (options.plugins.title) {
                options.plugins.title.color = darkColors.textColor;
            }
        }
    }
}

/**
 * Update all charts with current theme
 */
export function updateAllChartsTheme() {
    const charts = state.get('chartInstances');
    
    charts.forEach((chart, chartId) => {
        if (chart && chart.options) {
            applyThemeToChartOptions(chart.options);
            chart.update();
        }
    });
}

/**
 * Get chart canvas dimensions
 * @param {string} chartId - Chart identifier
 * @returns {object} Width and height of canvas
 */
export function getChartDimensions(chartId) {
    const chart = getChart(chartId);
    
    if (!chart || !chart.canvas) {
        return { width: 0, height: 0 };
    }

    return {
        width: chart.canvas.width,
        height: chart.canvas.height
    };
}

/**
 * Resize chart to fit container
 * @param {string} chartId - Chart identifier
 */
export function resizeChart(chartId) {
    const chart = getChart(chartId);
    
    if (chart && typeof chart.resize === 'function') {
        chart.resize();
    }
}

/**
 * Resize all charts
 */
export function resizeAllCharts() {
    const charts = state.get('chartInstances');
    
    charts.forEach((chart, chartId) => {
        resizeChart(chartId);
    });
}

/**
 * Export chart as image
 * @param {string} chartId - Chart identifier
 * @param {string} format - Image format ('png', 'jpeg', 'webp')
 * @returns {string} Base64 data URL
 */
export function exportChart(chartId, format = 'png') {
    const chart = getChart(chartId);
    
    if (!chart || !chart.canvas) {
        throw new Error(`Cannot export chart: ${chartId}`);
    }

    const mimeType = `image/${format}`;
    return chart.canvas.toDataURL(mimeType);
}

/**
 * Download chart as image file
 * @param {string} chartId - Chart identifier
 * @param {string} filename - Download filename
 * @param {string} format - Image format ('png', 'jpeg', 'webp')
 */
export function downloadChart(chartId, filename = 'chart', format = 'png') {
    const dataURL = exportChart(chartId, format);
    
    const link = document.createElement('a');
    link.download = `${filename}.${format}`;
    link.href = dataURL;
    link.click();
}

// Set up automatic chart cleanup on page unload
if (typeof window !== 'undefined') {
    window.addEventListener('beforeunload', () => {
        destroyAllCharts();
    });

    // Make functions globally available
    window.createChart = createChart;
    window.updateChart = updateChart;
    window.destroyChart = destroyChart;
    window.updateAllChartsTheme = updateAllChartsTheme;
}