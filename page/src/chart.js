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
 * @returns {Object} A Chart.js options object.
 */
function createChartOptions(title) {
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
            y: { }
        }
    };
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
        charts.power = new Chart(powerChartCtx, {
            type: 'line',
            data: {
                labels: initialLabels(),
                datasets: [
                    { label: 'Power (W)', data: initialData(), borderWidth: 2, fill: false, tension: 0.2, pointRadius: 2 },
                    { label: 'Avg Power', data: initialData(), borderWidth: 1.5, borderDash: [10, 5], fill: false, tension: 0, pointRadius: 0 }
                ]
            },
            options: createChartOptions('Power')
        });
    }

    // Create Voltage Chart
    if (voltageChartCtx) {
        charts.voltage = new Chart(voltageChartCtx, {
            type: 'line',
            data: {
                labels: initialLabels(),
                datasets: [
                    { label: 'Voltage (V)', data: initialData(), borderWidth: 2, fill: false, tension: 0.2, pointRadius: 2 },
                    { label: 'Avg Voltage', data: initialData(), borderWidth: 1.5, borderDash: [10, 5], fill: false, tension: 0, pointRadius: 0 }
                ]
            },
            options: createChartOptions('Voltage')
        });
    }

    // Create Current Chart
    if (currentChartCtx) {
        charts.current = new Chart(currentChartCtx, {
            type: 'line',
            data: {
                labels: initialLabels(),
                datasets: [
                    { label: 'Current (A)', data: initialData(), borderWidth: 2, fill: false, tension: 0.2, pointRadius: 2 },
                    { label: 'Avg Current', data: initialData(), borderWidth: 1.5, borderDash: [10, 5], fill: false, tension: 0, pointRadius: 0 }
                ]
            },
            options: createChartOptions('Current')
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

    const powerColor = getComputedStyle(htmlEl).getPropertyValue('--chart-power-color');
    const voltageColor = getComputedStyle(htmlEl).getPropertyValue('--chart-voltage-color');
    const currentColor = getComputedStyle(htmlEl).getPropertyValue('--chart-current-color');

    const updateThemeForChart = (chart, color) => {
        if (!chart) return;
        chart.options.scales.x.grid.color = gridColor;
        chart.options.scales.y.grid.color = gridColor;
        chart.options.scales.x.ticks.color = labelColor;
        chart.options.scales.y.ticks.color = labelColor;
        chart.options.plugins.legend.labels.color = labelColor;
        chart.options.plugins.title.color = labelColor;
        chart.data.datasets[0].borderColor = color;
        chart.data.datasets[1].borderColor = color;
        chart.data.datasets[1].borderDash = [10, 5];
        chart.update('none');
    };

    updateThemeForChart(charts.power, powerColor);
    updateThemeForChart(charts.voltage, voltageColor);
    updateThemeForChart(charts.current, currentColor);
}

/**
 * Updates all charts with new sensor data.
 * @param {Object} data - The new sensor data object from the WebSocket.
 */
export function updateCharts(data) {
    const timeLabel = new Date(data.timestamp * 1000).toLocaleTimeString();

    const updateSingleChart = (chart, value) => {
        if (!chart) return;

        // Shift old data
        chart.data.labels.shift();
        chart.data.datasets.forEach(dataset => dataset.data.shift());

        // Push new data
        chart.data.labels.push(timeLabel);
        chart.data.datasets[0].data.push(value.toFixed(2));

        // Calculate average and adjust Y-axis scale
        const dataArray = chart.data.datasets[0].data.filter(v => v !== null).map(v => parseFloat(v));
        if (dataArray.length > 0) {
            const sum = dataArray.reduce((acc, val) => acc + val, 0);
            const avg = (sum / dataArray.length).toFixed(2);
            chart.data.datasets[1].data.push(avg);

            // Adjust Y-axis scale for centering
            const minVal = Math.min(...dataArray);
            const maxVal = Math.max(...dataArray);
            let padding = (maxVal - minVal) * 0.1; // 10% padding of the range

            if (padding === 0) {
                // If all values are the same, add 10% padding of the value itself, or a small default
                padding = maxVal > 0 ? maxVal * 0.1 : 0.1;
            }

            chart.options.scales.y.min = Math.max(0, minVal - padding);
            chart.options.scales.y.max = maxVal + padding;

        } else {
            chart.data.datasets[1].data.push(null);
        }

        // Only update the chart if the tab is visible
        if (graphTabPane.classList.contains('show')) {
            chart.update('none');
        }
    };

    updateSingleChart(charts.power, data.power);
    updateSingleChart(charts.voltage, data.voltage);
    updateSingleChart(charts.current, data.current);
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
