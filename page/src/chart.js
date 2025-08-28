/**
 * @file chart.js
 * @description This module manages the Chart.js instances for visualizing sensor data.
 * It handles initialization, theme updates, data updates, and resizing for the three separate charts.
 */

import { Chart, registerables } from 'chart.js';
import { powerChartCtx, voltageChartCtx, currentChartCtx, htmlEl, graphTabPane } from './dom.js';

// Register all necessary Chart.js components
Chart.register(...registerables);

// Store chart instances in an object
export const charts = {
    power: null,
    voltage: null,
    current: null
};

const channelKeys = ['USB', 'MAIN', 'VIN'];
const CHART_DATA_POINTS = 30; // Number of data points to display on the chart

/**
 * Creates an array of empty labels for initial chart rendering.
 * @returns {Array<string>} An array of empty strings.
 */
function initialLabels() {
    return Array(CHART_DATA_POINTS).fill('');
}

/**
 * Creates an array of null data points for initial chart rendering.
 * @returns {Array<null>} An array of nulls.
 */
function initialData() {
    return Array(CHART_DATA_POINTS).fill(null);
}

/**
 * Creates a common configuration object for a single line chart.
 * @param {string} title - The title of the chart (e.g., 'Power (W)').
 * @param {number} minValue - The minimum value for Y-axis.
 * @param {number} maxValue - The maximum value for Y-axis.
 * @returns {Object} A Chart.js options object.
 */
function createChartOptions(title, minValue, maxValue) {
    return {
        responsive: true,
        maintainAspectRatio: false,
        interaction: { mode: 'index', intersect: false },
        plugins: {
            legend: { position: 'top' },
            title: { display: true, text: title }
        },
        scales: {
            x: { ticks: { autoSkipPadding: 10, maxRotation: 0, minRotation: 0 } },
            y: {
                min: minValue,
                max: maxValue,
                beginAtZero: true, // Start at zero for better comparison
                ticks: {
                    stepSize: (maxValue - minValue) / 8
                }
            }
        }
    };
}

/**
 * Creates the dataset objects for a chart.
 * @param {string} unit - The unit for the dataset label (e.g., 'W', 'V', 'A').
 * @returns {Array<Object>} An array of Chart.js dataset objects.
 */
function createDatasets(unit) {
    return channelKeys.map(channel => ({
        label: `${channel} (${unit})`,
        data: initialData(),
        borderWidth: 2,
        fill: false,
        tension: 0.2,
        pointRadius: 2
    }));
}


/**
 * Initializes all three charts (Power, Voltage, Current).
 * If chart instances already exist, they are destroyed and new ones are created.
 */
export function initCharts() {
    // Destroy existing charts if they exist
    for (const key in charts) {
        if (charts[key]) {
            charts[key].destroy();
        }
    }

    // Create Power Chart
    if (powerChartCtx) {
        const powerOptions = createChartOptions('Power', 0, 50); // Adjusted max value
        charts.power = new Chart(powerChartCtx, {
            type: 'line',
            data: {
                labels: initialLabels(),
                datasets: createDatasets('W')
            },
            options: powerOptions
        });
    }

    // Create Voltage Chart
    if (voltageChartCtx) {
        const voltageOptions = createChartOptions('Voltage', 0, 24);
        charts.voltage = new Chart(voltageChartCtx, {
            type: 'line',
            data: {
                labels: initialLabels(),
                datasets: createDatasets('V')
            },
            options: voltageOptions
        });
    }

    // Create Current Chart
    if (currentChartCtx) {
        const currentOptions = createChartOptions('Current', 0, 5); // Adjusted max value
        charts.current = new Chart(currentChartCtx, {
            type: 'line',
            data: {
                labels: initialLabels(),
                datasets: createDatasets('A')
            },
            options: currentOptions
        });
    }
}

/**
 * Applies a new theme (light or dark) to all charts.
 * @param {string} themeName - The name of the theme to apply ('light' or 'dark').
 */
export function applyChartsTheme(themeName) {
    const isDark = themeName === 'dark';
    const gridColor = isDark ? 'rgba(255, 255, 255, 0.1)' : 'rgba(0, 0, 0, 0.1)';
    const labelColor = isDark ? '#dee2e6' : '#212529';

    // Define colors for each channel. These could be from CSS variables.
    const channelColors = [
        getComputedStyle(htmlEl).getPropertyValue('--chart-usb-color').trim() || '#0d6efd', // Blue
        getComputedStyle(htmlEl).getPropertyValue('--chart-main-color').trim() || '#198754', // Green
        getComputedStyle(htmlEl).getPropertyValue('--chart-vin-color').trim() || '#dc3545'  // Red
    ];

    const updateThemeForChart = (chart) => {
        if (!chart) return;
        chart.options.scales.x.grid.color = gridColor;
        chart.options.scales.y.grid.color = gridColor;
        chart.options.scales.x.ticks.color = labelColor;
        chart.options.scales.y.ticks.color = labelColor;
        chart.options.plugins.legend.labels.color = labelColor;
        chart.options.plugins.title.color = labelColor;

        chart.data.datasets.forEach((dataset, index) => {
            dataset.borderColor = channelColors[index];
        });

        chart.update('none');
    };

    updateThemeForChart(charts.power);
    updateThemeForChart(charts.voltage);
    updateThemeForChart(charts.current);
}

/**
 * Updates all charts with new sensor data.
 * @param {Object} data - The new sensor data object from the WebSocket.
 */
export function updateCharts(data) {
    const timeLabel = new Date(data.timestamp * 1000).toLocaleTimeString();

    const updateSingleChart = (chart, metric) => {
        if (!chart) return;

        // Shift old data
        chart.data.labels.shift();
        chart.data.datasets.forEach(dataset => dataset.data.shift());

        // Push new data
        chart.data.labels.push(timeLabel);

        channelKeys.forEach((key, index) => {
            if (data[key] && data[key][metric] !== undefined) {
                const value = data[key][metric];
                chart.data.datasets[index].data.push(value.toFixed(2));
            } else {
                chart.data.datasets[index].data.push(null); // Push null if data for a channel is missing
            }
        });

        // Only update the chart if the tab is visible
        if (graphTabPane.classList.contains('show')) {
            chart.update('none');
        }
    };

    updateSingleChart(charts.power, 'power');
    updateSingleChart(charts.voltage, 'voltage');
    updateSingleChart(charts.current, 'current');
}

/**
 * Resizes all chart canvases. This is typically called on window resize events.
 */
export function resizeCharts() {
    for (const key in charts) {
        if (charts[key]) {
            charts[key].resize();
        }
    }
}
