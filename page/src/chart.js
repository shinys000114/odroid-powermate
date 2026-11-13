// SPDX-FileCopyrightText: 2025 Hardkernel Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-only

/**
 * @file chart.js
 * @description This module manages the uPlot instances for visualizing sensor data.
 * It handles initialization, theme updates, data updates, and resizing for the three separate charts.
 */

import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';
import {currentChartEl, graphTabPane, htmlEl, powerChartEl, voltageChartEl} from './dom.js';

// Store chart instances in an object
export const charts = {
    power: null,
    voltage: null,
    current: null
};

// Configuration for dynamic, step-wise Y-axis scaling
const scaleConfig = {
    power: {steps: [5, 20, 50, 160]},   // in Watts
    voltage: {steps: [5, 10, 15, 25]},    // in Volts
    current: {steps: [1, 2.5, 5, 10]}     // in Amps
};

const chartDefs = [
    {key: 'power', title: 'Power', unit: 'W', el: () => powerChartEl},
    {key: 'voltage', title: 'Voltage', unit: 'V', el: () => voltageChartEl},
    {key: 'current', title: 'Current', unit: 'A', el: () => currentChartEl},
];

const channelKeys = ['USB', 'MAIN', 'VIN'];
const CHART_DATA_POINTS = 30; // Number of data points to display on the chart
const FONT = '12px "Helvetica Neue", Helvetica, Arial, sans-serif';

// The last CHART_DATA_POINTS samples, kept even while the graph tab is hidden.
const labels = Array(CHART_DATA_POINTS).fill('');
const series = Object.fromEntries(Object.keys(scaleConfig).map(metric =>
    [metric, channelKeys.map(() => Array(CHART_DATA_POINTS).fill(null))]));
const xValues = Array.from({length: CHART_DATA_POINTS}, (_, i) => i);
const yMax = Object.fromEntries(Object.entries(scaleConfig).map(([metric, {steps}]) => [metric, steps[0]]));

let currentTheme = htmlEl.getAttribute('data-bs-theme') || 'light';

function themeColors() {
    const isDark = currentTheme === 'dark';
    const css = getComputedStyle(htmlEl);
    return {
        grid: isDark ? 'rgba(255, 255, 255, 0.1)' : 'rgba(0, 0, 0, 0.1)',
        label: isDark ? '#dee2e6' : '#212529',
        channels: [
            css.getPropertyValue('--chart-usb-color').trim() || '#0d6efd',
            css.getPropertyValue('--chart-main-color').trim() || '#198754',
            css.getPropertyValue('--chart-vin-color').trim() || '#dc3545',
        ],
    };
}

/** The line color at half opacity, for legend and tooltip boxes. */
function translucent(color) {
    const hex = color.replace('#', '');
    if (!/^[0-9a-f]{6}$/i.test(hex)) return color;
    const [r, g, b] = [0, 2, 4].map(i => parseInt(hex.slice(i, i + 2), 16));
    return `rgba(${r}, ${g}, ${b}, 0.5)`;
}

/** Floating tooltip with the time and every channel's value at the cursor. */
function tooltipPlugin(colors) {
    let tip;
    return {
        hooks: {
            init: (u) => {
                tip = document.createElement('div');
                tip.className = 'chart-tooltip';
                tip.style.display = 'none';
                u.over.appendChild(tip);
                u.over.addEventListener('mouseleave', () => { tip.style.display = 'none'; });
            },
            setCursor: (u) => {
                const idx = u.cursor.idx;
                if (idx == null || u.data.slice(1).every(s => s[idx] == null)) {
                    tip.style.display = 'none';
                    return;
                }
                const rows = u.series.slice(1).map((s, i) => {
                    const v = u.data[i + 1][idx];
                    return `<div><span class="chart-tooltip-box" style="background:${colors[i]}"></span>${s.label}: ${v == null ? '-' : v.toFixed(2)}</div>`;
                }).join('');
                tip.innerHTML = `<div class="chart-tooltip-title">${labels[idx]}</div>${rows}`;
                tip.style.display = '';
                const left = u.valToPos(idx, 'x');
                const flip = left > u.over.clientWidth / 2;
                tip.style.left = `${flip ? left - tip.offsetWidth - 10 : left + 10}px`;
                tip.style.top = `${Math.max(0, u.cursor.top - tip.offsetHeight / 2)}px`;
            },
        },
    };
}

/** Width and plot height that fill the chart's container below its title and legend. */
function sizeFor(el, u) {
    const chrome = u ? u.root.offsetHeight - u.over.parentElement.offsetHeight : 60;
    return {width: el.clientWidth, height: Math.max(100, el.clientHeight - chrome)};
}

function createChart({key, title, unit, el}) {
    const container = el();
    if (!container || !container.clientWidth) return null;
    const colors = themeColors();
    const axis = {
        stroke: colors.label,
        font: FONT,
        grid: {stroke: colors.grid, width: 1},
        ticks: {stroke: colors.grid, width: 1, size: 6},
    };

    const opts = {
        title,
        ...sizeFor(container),
        pxAlign: false,
        cursor: {x: false, y: false, drag: {x: false, y: false}, points: {size: 8}},
        legend: {
            live: false,
            markers: {width: 2, fill: (u, i) => translucent(colors.channels[i - 1])},
        },
        scales: {
            x: {time: false, range: [0, CHART_DATA_POINTS - 1]},
            y: {range: () => [0, yMax[key]]},
        },
        axes: [
            {
                ...axis,
                space: 60,
                incrs: [1, 2, 3, 5, 10, 15, 30],
                values: (u, splits) => splits.map(i => labels[i] ?? ''),
            },
            {
                ...axis,
                size: 40,
                splits: () => Array.from({length: 6}, (_, i) => (yMax[key] / 5) * i),
                // Same decimals on every tick, as many as the step needs (0, 0.5, 1.0, ...).
                values: (u, splits) => {
                    const decimals = (String(yMax[key] / 5).split('.')[1] ?? '').length;
                    return splits.map(v => v.toFixed(decimals));
                },
            },
        ],
        series: [
            {},
            ...channelKeys.map((channel, i) => ({
                label: `${channel} (${unit})`,
                stroke: colors.channels[i],
                width: 2,
                paths: uPlot.paths.spline(),
                points: {show: false},
                spanGaps: false,
            })),
        ],
        plugins: [tooltipPlugin(colors.channels)],
    };

    container.replaceChildren();
    const u = new uPlot(opts, [xValues, ...series[key]], container);
    u.setSize(sizeFor(container, u));
    return u;
}

/**
 * Initializes all three charts (Power, Voltage, Current).
 * If chart instances already exist, they are destroyed and new ones are created.
 */
export function initCharts() {
    for (const def of chartDefs) {
        charts[def.key]?.destroy();
        charts[def.key] = createChart(def);
    }
}

/**
 * Applies a new theme (light or dark) to all charts.
 * @param {string} themeName - The name of the theme to apply ('light' or 'dark').
 */
export function applyChartsTheme(themeName) {
    currentTheme = themeName;
    // Axis, grid and line colors are baked into the canvas, so rebuild the charts.
    if (Object.values(charts).some(Boolean)) initCharts();
}

/**
 * Records one sample for a metric and adjusts its step-wise Y-axis maximum.
 */
function pushSample(metric, data) {
    series[metric].forEach((values, index) => {
        values.shift();
        const value = data[channelKeys[index]]?.[metric];
        values.push(value !== undefined ? +value.toFixed(2) : null);
    });

    const {steps} = scaleConfig[metric];
    const maxDataValue = Math.max(0, ...series[metric].flat().filter(v => v !== null));
    // The smallest step that is >= maxDataValue, or the largest step.
    yMax[metric] = steps.find(step => maxDataValue <= step) ?? steps[steps.length - 1];
}

/**
 * Updates all charts with new sensor data.
 * @param {Object} data - The new sensor data object from the WebSocket.
 */
export function updateCharts(data) {
    labels.shift();
    labels.push(new Date(data.timestamp).toLocaleTimeString());
    for (const metric of Object.keys(scaleConfig)) pushSample(metric, data);

    // Redraw only while the graph tab is visible.
    if (!graphTabPane.classList.contains('show')) return;
    for (const def of chartDefs) {
        charts[def.key]?.setData([xValues, ...series[def.key]]);
    }
}

/**
 * Resizes all charts to their containers. This is typically called on window resize events
 * and when the graph tab is shown again.
 */
export function resizeCharts() {
    for (const def of chartDefs) {
        const u = charts[def.key];
        const container = def.el();
        if (!container?.clientWidth) continue;
        if (!u) {
            // Not created yet, or rebuilt (theme change) while the tab was hidden.
            charts[def.key] = createChart(def);
            continue;
        }
        u.setSize(sizeFor(container, u));
        u.setData([xValues, ...series[def.key]]);
    }
}
