# ODROID Remote HTTP Power Manager (OWPM)

A web-based remote power and monitoring tool for ODROID Single Board Computers (SBCs), powered by an ESP32. This project provides a comprehensive web interface to control power, monitor real-time metrics, and access the serial console of your ODROID from any web browser on your local network.

## Features

- **Web-Based UI**: Modern, responsive interface built with Bootstrap and vanilla JavaScript.
- **Power Control**: Independently toggle Main (e.g., 12V) and USB (5V) power supplies.
- **Power Actions**: Remotely trigger Reset and Power On/Off actions.
- **Real-time Metrics**: Monitor voltage, current, and power consumption with a live-updating graph.
- **Interactive Serial Terminal**: Access your ODROID's serial console directly from the web UI via WebSockets.
- **Wi-Fi Management**:
    - Scan and connect to Wi-Fi networks (STA mode).
    - Enable Access Point mode (AP+STA) to connect directly to the device.
    - Configure static IP settings.
- **System Info**: View device uptime and connection status.
- **Customizable Theme**: Switch between light and dark modes, with the preference saved locally.

## Prerequisites

Before you begin, ensure you have the following installed and configured on your system:

- **[ESP-IDF (Espressif IoT Development Framework)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)**: This project is developed and tested with ESP-IDF v5.x.
- **[Node.js and npm](https://nodejs.org/)**: Required to build the web application. Node.js LTS version (e.g., 18.x or later) is recommended.

## How to Build and Flash

1.  **Clone the repository:**
    ```bash
    git clone <your-repository-url>
    cd odroid-remote-http
    ```

2.  **Build the Web Application:**
    The web interface needs to be compiled before building the main firmware.
    ```bash
    cd page
    npm install
    npm run build
    cd ..
    ```

3.  **Set up the ESP-IDF environment:**
    Open a terminal and source the ESP-IDF export script. The path may vary depending on your installation location.
    ```bash
    . $HOME/esp/esp-idf/export.sh
    ```

4.  **Set the target chip:**
    Specify your ESP32 variant (e.g., `esp32`, `esp32s3`).
    ```bash
    idf.py set-target esp32
    ```

5.  **(Optional) Configure the project:**
    You can configure project-specific settings, such as default Wi-Fi credentials, by running `menuconfig`.
    ```bash
    idf.py menuconfig
    ```

6.  **Build the project:**
    This command compiles the application, bootloader, partition table, and embeds the compiled web page data.
    ```bash
    idf.py build
    ```

7.  **Flash the firmware:**
    Connect your ESP32 board to your computer and replace `/dev/ttyUSB0` with your device's serial port.
    ```bash
    idf.py -p /dev/ttyUSB0 flash
    ```

8.  **Monitor the output:**
    To view the serial logs from the device, use the `monitor` command. This is useful for finding the device's IP address after it connects to your Wi-Fi.
    ```bash
    idf.py -p /dev/ttyUSB0 monitor
    ```
    To exit the monitor, press `Ctrl+]`.

## Usage

1.  After flashing, the ESP32 will either connect to the pre-configured Wi-Fi network or start an Access Point (AP).
2.  Check the serial monitor logs to find the IP address assigned to the device in STA mode, or the default AP address (usually `192.168.4.1`).
3.  Open a web browser and navigate to the device's IP address.
4.  You should now see the ODROID Remote control panel.