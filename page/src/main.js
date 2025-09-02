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

// --- Module Imports -- -
import { StatusMessage } from './proto.js';
import { initWebSocket } from './websocket.js';
import { setupTerminal, term } from './terminal.js';
import {
    applyTheme,
    initUI,
    updateControlStatus,
    updateSensorUI,
    updateWebsocketStatus,
    updateWifiStatusUI
} from './ui.js';
import { setupEventListeners } from './events.js';

// --- Globals ---
// StatusMessage is imported directly from the generated proto.js file.

// --- WebSocket Event Handlers ---

function onWsOpen() {
    updateWebsocketStatus(true);
    if (term) {
        term.write('\x1b[32mConnected to WebSocket Server\x1b[0m\r\n');
    }
}

function onWsClose() {
    updateWebsocketStatus(false);
    if (term) {
        term.write('\r\n\x1b[31mConnection closed. Reconnecting...\x1b[0m\r\n');
    }
    setTimeout(connect, 2000);
}

/**
 * Callback for when a message is received from the WebSocket server.
 * This version includes extensive logging for debugging purposes.
 * @param {MessageEvent} event - The WebSocket message event.
 */
function onWsMessage(event) {
    // Log any incoming message to the console for debugging.

    if (!(event.data instanceof ArrayBuffer)) {
        console.warn('Message is not an ArrayBuffer, skipping protobuf decoding.');
        return;
    }

    const buffer = new Uint8Array(event.data);
    try {
        const decodedMessage = StatusMessage.decode(buffer);
        const payloadType = decodedMessage.payload;

        switch (payloadType) {
            case 'sensorData': {
                const sensorData = decodedMessage.sensorData;
                if (sensorData) {
                    const sensorPayload = {
                        ...sensorData,
                        USB: sensorData.usb,
                        MAIN: sensorData.main,
                        VIN: sensorData.vin,
                    };
                    // Log the exact data being sent to the UI function
                    updateSensorUI(sensorPayload);
                }
                break;
            }

            case 'wifiStatus':
                updateWifiStatusUI(decodedMessage.wifiStatus);
                break;

            case 'uartData':
                if (term && decodedMessage.uartData && decodedMessage.uartData.data) {
                    term.write(decodedMessage.uartData.data);
                }
                break;

            default:
                console.warn('Received message with unknown or empty payload type:', payloadType);
                break;
        }
    } catch (e) {
        console.error('Error decoding protobuf message:', e);
    }
}


// --- Application Initialization ---

function connect() {
    updateControlStatus();
    initWebSocket({ onOpen: onWsOpen, onClose: onWsClose, onMessage: onWsMessage });
}

function initialize() {
    initUI();
    setupTerminal();

    const savedTheme = localStorage.getItem('theme') || (window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light');
    applyTheme(savedTheme);

    setupEventListeners();

    connect();
}

// --- Start Application ---
document.addEventListener('DOMContentLoaded', initialize);
