# ODROID Remote API Documentation

This document outlines the HTTP REST and WebSocket APIs for communication between the web interface and the ESP32 device.

---

## WebSocket API

The WebSocket API provides a full-duplex communication channel for real-time data, such as sensor metrics and the
interactive serial console. All server-to-client communication is done via binary WebSocket frames containing Protocol
Buffers (protobuf) messages.

**Endpoint**: `/ws`

### Server-to-Client Messages

The server pushes binary messages to the client. Each message is a Protocol Buffers (protobuf) encoded `StatusMessage`.
This top-level message uses a `oneof` field to carry different payload types.

The protobuf schema is defined in `proto/status.proto`.

```proto
// Top-level message for all websocket communication
message StatusMessage {
   oneof payload {
    SensorData sensor_data = 1;
    WifiStatus wifi_status = 2;
    UartData uart_data = 3;
  }
}
```

The client decodes the `StatusMessage` and then handles the specific payload:

| `oneof` payload field | Contained Message | Description                                                                             |
|-----------------------|-------------------|-----------------------------------------------------------------------------------------|
| `sensor_data`         | `SensorData`      | Pushed periodically (e.g., every second) with the latest power metrics.                 |
| `wifi_status`         | `WifiStatus`      | Pushed periodically or on change to update the current Wi-Fi connection status.         |
| `uart_data`           | `UartData`        | Forwards raw binary data from the ODROID's serial (UART) port to the client's terminal. |

### Client-to-Server Messages

The client sends raw binary/text data, which is interpreted as terminal input.

- **Description**: Raw binary data representing user input from the web terminal. This data is forwarded directly to the ODROID's serial (UART) port.
- **Payload**: `(raw data)`

---

## HTTP REST API

The REST API is used for configuration and to trigger specific actions. All request and response bodies are in `application/json` format.

### Endpoint: `/api/control`

Manages power relays and system actions.

#### `GET /api/control`

Retrieves the current status of the power relays.

- **Success Response (200 OK)**
  ```json
  {
    "load_12v_on": true,
    "load_5v_on": false
  }
  ```
  - `load_12v_on` (boolean): The state of the main 12V power relay.
  - `load_5v_on` (boolean): The state of the 5V USB power relay.

#### `POST /api/control`

Sets the state of power relays or triggers a power action. You can send one or more commands in a single request.

- **Request Body Examples**:
  - To turn the main power on:
    ```json
    { "load_12v_on": true }
    ```
  - To trigger a system reset:
    ```json
    { "reset_trigger": true }
    ```
  - To toggle the power button:
    ```json
    { "power_trigger": true }
    ```

- **Request Fields**:
  - `load_12v_on` (boolean, optional): Sets the state of the 12V relay.
  - `load_5v_on` (boolean, optional): Sets the state of the 5V relay.
  - `reset_trigger` (boolean, optional): If `true`, momentarily triggers the reset button (pulls the line low for 3 seconds). The action is triggered only on a `true` value.
  - `power_trigger` (boolean, optional): If `true`, momentarily triggers the power button (pulls the line low for 3 seconds). The action is triggered only on a `true` value.

- **Success Response (200 OK)**: `{"status":"ok"}`

---

### Endpoint: `/api/setting`

Manages all Wi-Fi, network, and system-related configurations.

#### `GET /api/setting`

Retrieves the complete current network and system configuration.

- **Success Response (200 OK)**
  ```json
  {
    "connected": true,
    "ssid": "MyHome_WiFi",
    "rssi": -65,
    "mode": "apsta",
    "net_type": "static",
    "baudrate": "115200",
    "ip": {
        "ip": "192.168.1.100",
        "gateway": "192.168.1.1",
        "subnet": "255.255.255.0",
        "dns1": "8.8.8.8",
        "dns2": "8.8.4.4"
    }
  }
  ```
- **Response Fields**:
  - `connected` (boolean): Current Wi-Fi connection state.
  - `ssid` (string): The SSID of the connected network.
  - `rssi` (integer): The Received Signal Strength Indicator in dBm. Only present if connected.
  - `mode` (string): The current Wi-Fi mode (`"sta"` or `"apsta"`).
  - `net_type` (string): The network type (`"dhcp"` or `"static"`).
  - `baudrate` (string): The current UART baud rate.
  - `ip` (object): Contains IP configuration details. Present even if using DHCP (may show the last-leased IP).
    - `ip` (string): The device's IP address.
    - `gateway` (string): The network gateway address.
    - `subnet` (string): The network subnet mask.
    - `dns1` (string): The primary DNS server address.
    - `dns2` (string): The secondary DNS server address.

#### `POST /api/setting`

This is a multi-purpose endpoint. The server determines the action based on the fields provided in the request body.

- **Action: Connect to a Wi-Fi Network**
  - **Request Body**:
    ```json
    {
      "ssid": "MyHome_WiFi",
      "password": "my_secret_password"
    }
    ```
  - **Success Response (200 OK)**:
    ```json
    { "status": "connection_initiated" }
    ```

- **Action: Configure Network Type (DHCP/Static)**
  - **Request Body (for DHCP)**:
    ```json
    { "net_type": "dhcp" }
    ```
  - **Request Body (for Static IP)**:
    *Note: The `ip` object structure is consistent with the `GET /api/setting` response.*
    ```json
    {
      "net_type": "static",
      "ip": {
        "ip": "192.168.1.100",
        "gateway": "192.168.1.1",
        "subnet": "255.255.255.0",
        "dns1": "8.8.8.8",
        "dns2": "8.8.4.4"
      }
    }
    ```
  - **Success Response (200 OK)**:
    - `{"status":"dhcp_config_applied"}`
    - `{"status":"static_config_applied"}`

- **Action: Configure Wi-Fi Mode (STA/APSTA)**
  - **Request Body (for STA mode)**:
    ```json
    { "mode": "sta" }
    ```
  - **Request Body (for AP+STA mode)**:
    ```json
    {
        "mode": "apsta",
        "ap_ssid": "ODROID-Remote-AP",
        "ap_password": "hardkernel"
    }
    ```
    *Note: `ap_password` is optional. If omitted, the AP will be open.*
  - **Success Response (200 OK)**: `{"status":"mode_switch_initiated"}`

- **Action: Configure UART Baud Rate**
  - **Request Body**:
    ```json
    { "baudrate": "115200" }
    ```
  - **Success Response (200 OK)**:
    ```json
    { "status": "baudrate_updated" }
    ```

---

### Endpoint: `/api/wifi/scan`

Scans for available Wi-Fi networks.

#### `GET /api/wifi/scan`

- **Success Response (200 OK)**: Returns a JSON array of found networks.
  ```json
  [
    {
      "ssid": "MyHome_WiFi",
      "rssi": -55,
      "authmode": "WPA2_PSK"
    },
    {
      "ssid": "GuestNetwork",
      "rssi": -78,
      "authmode": "OPEN"
    }
  ]
  ```
- **Response Fields**:
  - `ssid` (string): The network's Service Set Identifier.
  - `rssi` (integer): Signal strength in dBm.
  - `authmode` (string): The authentication mode (e.g., `"OPEN"`, `"WPA_PSK"`, `"WPA2_PSK"`).

---

### Endpoint: `/datalog.csv`

Provides access to the historical sensor data log.

#### `GET /datalog.csv`

- **Description**: Downloads a CSV file containing the history of sensor data readings (voltage, current, power). The log is rotated when it reaches its maximum size (1MB).
- **Success Response (200 OK)**: The body of the response is the CSV file content.
- **Response Headers**:
  - `Content-Type: text/csv`
  - `Content-Disposition: attachment; filename="datalog.csv"`
- **CSV Format**:
  ```csv
  timestamp,voltage,current,power
  1672531200,12.01,1.52,18.25
  1672531201,12.02,1.53,18.39
  ...
  ```

---

### General Error Responses

In case of an error, the server will respond with an appropriate HTTP status code.

- **`400 Bad Request`**: The request is malformed, contains invalid parameters, or is otherwise incorrect. The response body may contain a JSON object with more details.
  ```json
  {
    "error": "Invalid request body"
  }
  ```
- **`404 Not Found`**: The requested endpoint does not exist.
- **`500 Internal Server Error`**: The server encountered an unexpected condition that prevented it from fulfilling the request.
