/**
 * @file events.js
 * @description This module sets up all the event listeners for the application.
 * It connects user interactions (like clicks and toggles) to the corresponding
 * functions in other modules (UI, API, etc.).
 */

import * as dom from './dom.js';
import * as api from './api.js';
import * as ui from './ui.js';
import { clearTerminal, fitTerminal, downloadTerminalOutput } from './terminal.js';
import { debounce, isMobile } from './utils.js';

// A flag to track if charts have been initialized
let chartsInitialized = false;
let listenersAttached = false;

/**
 * Sets up all event listeners for the application's interactive elements.
 * This function is now idempotent and will only attach listeners once.
 */
export function setupEventListeners() {
    if (listenersAttached) {
        console.log("Event listeners already attached. Skipping.");
        return;
    }
    console.log("Attaching event listeners...");

    // --- Theme Toggle ---
    dom.themeToggle.addEventListener('change', () => {
        const newTheme = dom.themeToggle.checked ? 'dark' : 'light';
        localStorage.setItem('theme', newTheme);
        ui.applyTheme(newTheme);
    });

    // --- Terminal Controls ---
    dom.clearButton.addEventListener('click', clearTerminal);
    dom.downloadButton.addEventListener('click', downloadTerminalOutput);

    // --- Power Controls ---
    dom.mainPowerToggle.addEventListener('change', () => api.postControlCommand({'load_12v_on': dom.mainPowerToggle.checked}).then(ui.updateControlStatus));
    dom.usbPowerToggle.addEventListener('change', () => api.postControlCommand({'load_5v_on': dom.usbPowerToggle.checked}).then(ui.updateControlStatus));
    dom.resetButton.addEventListener('click', () => api.postControlCommand({'reset_trigger': true}));
    dom.powerActionButton.addEventListener('click', () => api.postControlCommand({'power_trigger': true}));

    // --- Settings Modal Controls ---
    dom.scanWifiButton.addEventListener('click', ui.scanForWifi);
    dom.wifiConnectButton.addEventListener('click', ui.connectToWifi);
    dom.networkApplyButton.addEventListener('click', ui.applyNetworkSettings);
    dom.apModeApplyButton.addEventListener('click', ui.applyApModeSettings);
    dom.baudRateApplyButton.addEventListener('click', ui.applyBaudRateSettings);

    // --- Settings Modal Toggles (for showing/hiding sections) ---
    dom.apModeToggle.addEventListener('change', () => {
        dom.apModeConfig.style.display = dom.apModeToggle.checked ? 'block' : 'none';
    });

    dom.staticIpToggle.addEventListener('change', () => {
        dom.staticIpConfig.style.display = dom.staticIpToggle.checked ? 'block' : 'none';
    });

    // --- General App Listeners ---
    dom.settingsButton.addEventListener('click', ui.initializeSettings);

    // --- Accessibility: Remove focus from modal elements before hiding ---
    const blurActiveElement = () => {
        if (document.activeElement && typeof document.activeElement.blur === 'function') {
            document.activeElement.blur();
        }
    };
    dom.settingsModal.addEventListener('hide.bs.modal', blurActiveElement);
    dom.wifiModalEl.addEventListener('hide.bs.modal', blurActiveElement);

    // --- Bootstrap Tab Events ---
    document.querySelectorAll('button[data-bs-toggle="tab"]').forEach(tabEl => {
        tabEl.addEventListener('shown.bs.tab', async (event) => {
            const tabId = event.target.getAttribute('data-bs-target');

            if (tabId === '#graph-tab-pane') {
                // Dynamically import the chart module only when the tab is shown
                const chartModule = await import('./chart.js');

                if (!chartsInitialized) {
                    chartModule.initCharts();
                    chartsInitialized = true;
                } else {
                    chartModule.resizeCharts();
                }
            } else if (tabId === '#terminal-tab-pane') {
                // Fit the terminal when its tab is shown, especially for mobile.
                if (isMobile()) {
                    fitTerminal();
                }
            }
        });
    });

    // --- Window Resize Event ---
    // Debounced to avoid excessive calls during resizing.
    window.addEventListener('resize', debounce(ui.handleResize, 150));

    listenersAttached = true;
    console.log("Event listeners attached successfully.");
}
