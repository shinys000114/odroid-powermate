/**
 * @file main.js
 * @description The main entry point for the web application.
 * This file imports all necessary modules, sets up the application structure,
 * initializes components, and establishes the WebSocket connection.
 */

// --- Stylesheets ---
import 'bootstrap/dist/css/bootstrap.min.css';
import 'bootstrap-icons/font/bootstrap-icons.css';
import './style.css';

// --- Module Imports ---
import { initWebSocket } from './websocket.js';
import { setupTerminal, term } from './terminal.js';
import {
    applyTheme,
    initUI,
    updateControlStatus,
    updateSensorUI,
    updateWifiStatusUI,
    updateWebsocketStatus
} from './ui.js';
import { setupEventListeners } from './events.js';

// --- WebSocket Event Handlers ---

/**
 * Callback function for when the WebSocket connection is successfully opened.
 * Updates the UI to show an 'Online' status and fetches the initial control status.
 */
function onWsOpen() {
    updateWebsocketStatus(true);
    if (term) {
        term.write('\x1b[32mConnected to WebSocket Server\x1b[0m\r\n');
    }
    updateControlStatus();
}

/**
 * Callback function for when the WebSocket connection is closed.
 * Updates the UI to show an 'Offline' status and attempts to reconnect after a delay.
 */
function onWsClose() {
    updateWebsocketStatus(false);
    if (term) {
        term.write('\r\n\x1b[31mConnection closed. Reconnecting...\x1b[0m\r\n');
    }
    // Attempt to re-establish the connection after 2 seconds
    setTimeout(initialize, 2000);
}

/**
 * Callback function for when a message is received from the WebSocket server.
 * It handles both JSON messages (for sensor and status updates) and binary data (for the terminal).
 * @param {MessageEvent} event - The WebSocket message event.
 */
function onWsMessage(event) {
    if (typeof event.data === 'string') {
        try {
            const message = JSON.parse(event.data);
            if (message.type === 'sensor_data') {
                updateSensorUI(message);
            } else if (message.type === 'wifi_status') {
                updateWifiStatusUI(message);
            }
        } catch (e) {
            // Ignore non-JSON string messages
        }
    } else if (term && event.data instanceof ArrayBuffer) {
        // Write raw UART data to the terminal
        const data = new Uint8Array(event.data);
        term.write(data);
    }
}

// --- Application Initialization ---

/**
 * Initializes the entire application.
 * This function sets up the UI, theme, terminal, chart, WebSocket connection, and event listeners.
 */
function initialize() {
    // Initialize basic UI components
    initUI();

    // Set up the interactive components first
    setupTerminal();

    // Apply the saved theme or detect the user's preferred theme.
    // This must be done AFTER the chart and terminal are initialized.
    const savedTheme = localStorage.getItem('theme') || (window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light');
    applyTheme(savedTheme);

    // Establish the WebSocket connection with the defined handlers
    initWebSocket({
        onOpen: onWsOpen,
        onClose: onWsClose,
        onMessage: onWsMessage
    });

    // Attach all event listeners to the DOM elements
    setupEventListeners();
}

// --- Start Application ---

// Wait for the DOM to be fully loaded before initializing the application.
document.addEventListener('DOMContentLoaded', initialize);
