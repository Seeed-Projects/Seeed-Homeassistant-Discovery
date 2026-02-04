# BLE-MQTT Gateway Example

Gateway firmware that scans BTHome BLE advertisements and forwards them to an MQTT server, designed to work with XIAO nRF52840 motion detection devices.

## Features

- Continuous BLE scanning for BTHome devices
- MQTT forwarding of motion state
- WiFi captive portal configuration (WiFiManager)
- Automatic WiFi and MQTT reconnection
- LED status indicators
- **Multi-board support** with automatic pin detection

## Supported Boards

| Board | LED Pin | BOOT Button | WiFi |
|-------|---------|-------------|------|
| XIAO ESP32-C3 | GPIO10 (D10) | GPIO9 | 2.4GHz |
| XIAO ESP32-S3 | GPIO21 (D21) | GPIO0 | 2.4GHz |
| XIAO ESP32-C6 | GPIO15 (D15) | GPIO9 | 2.4GHz |
| XIAO ESP32-C5 | GPIO27 | GPIO9 | 2.4GHz + 5GHz (WiFi 6) |

## Hardware Requirements

- One of the supported XIAO ESP32 boards (C3, S3, C6, or C5)
- USB-C cable

## Software Dependencies

### Arduino IDE Setup

1. **Board Manager**: Add ESP32 boards
   - URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Install: **esp32** by Espressif Systems

2. **Board Selection**:
   ```
   Tools → Board → ESP32 Arduino → Select your XIAO board:
   - XIAO_ESP32C3
   - XIAO_ESP32S3
   - XIAO_ESP32C6
   - XIAO_ESP32C5
   ```

3. **Partition Scheme**:
   ```
   Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)
   ```

### Required Libraries

Install via Arduino Library Manager:

| Library | Author | Purpose |
|---------|--------|---------|
| WiFiManager | tzapu | WiFi captive portal |
| PubSubClient | Nick O'Leary | MQTT client |
| ArduinoJson | Benoit Blanchon | JSON serialization |

Note: BLE functionality uses built-in ESP32 libraries, no additional installation needed.

## Usage

### 1. First-Time Configuration

1. After flashing, device creates WiFi hotspot `SeeedUA-Gateway`
2. Connect to this hotspot with phone or computer
3. Captive portal auto-opens (or visit http://192.168.4.1)
4. Configure settings:
   - **WiFi SSID and Password**
   - **MQTT Server**: MQTT broker address
   - **MQTT Port**: Default 1883
   - **MQTT User**: Username (optional)
   - **MQTT Password**: Password (optional)
5. Click Save, device restarts and connects

### 2. Reconfiguration

To modify settings:
1. **Hold BOOT button within 5 seconds** after power-on
2. Device enters config mode (LED fast blink)
3. Connect to hotspot and reconfigure

### 3. Normal Operation

- Device automatically scans for `SeeedUA-` BTHome devices
- Forwards received data to MQTT

## MQTT Message Format

### Topic

```
bthome/{mac_address}/state
```

Example: `bthome/c1f93e1d937c/state`

### Payload (JSON)

```json
{
  "motion": true,
  "rssi": -65,
  "name": "SeeedUA-937C"
}
```

| Field | Type | Description |
|-------|------|-------------|
| motion | boolean | true=motion detected, false=clear |
| rssi | integer | Signal strength (dBm) |
| name | string | Device name |

## LED Indicators

| Status | LED Behavior |
|--------|-------------|
| Config Mode | Fast blink (200ms) |
| Connecting | Slow blink (1s) |
| Normal Operation | Solid on |
| Data Received | Brief off flash |

## Serial Output Example

```
========================================
  XIAO ESP32 BLE-MQTT Gateway
========================================

Board: XIAO ESP32-C5
LED Pin: GPIO27
BOOT Button: GPIO9

Loaded MQTT config: 192.168.1.100:1883
[WiFi] Hold BOOT button within 5s to reconfigure
[WiFi] Connected!
[WiFi] IP: 192.168.1.50
[MQTT] Connecting to 192.168.1.100:1883
[MQTT] Connected!
[BLE] Initializing NimBLE...
[BLE] NimBLE initialization complete
[BLE] Starting scan...

========================================
  Initialization Complete!
========================================

Gateway ready, scanning for BTHome devices...

MQTT Topic Format: bthome/{mac}/state
Payload: {"motion":true/false,"rssi":-XX,"name":"..."}

[BLE] SeeedUA-937C (c1:f9:3e:1d:93:7c) RSSI:-45 Motion:Detected
[MQTT] -> bthome/c1f93e1d937c/state: {"motion":true,"rssi":-45,"name":"SeeedUA-937C"}
[BLE] SeeedUA-937C (c1:f9:3e:1d:93:7c) RSSI:-47 Motion:Clear
[MQTT] -> bthome/c1f93e1d937c/state: {"motion":false,"rssi":-47,"name":"SeeedUA-937C"}
```

## Home Assistant Integration

### Method 1: MQTT Sensor

Add to `configuration.yaml`:

```yaml
mqtt:
  binary_sensor:
    - name: "SeeedUA-937C Motion"
      state_topic: "bthome/c1f93e1d937c/state"
      device_class: motion
      value_template: "{{ 'ON' if value_json.motion else 'OFF' }}"
      json_attributes_topic: "bthome/c1f93e1d937c/state"
      json_attributes_template: "{{ {'rssi': value_json.rssi, 'name': value_json.name} | tojson }}"
```

### Method 2: MQTT Discovery

You can add HA MQTT Discovery messages in the ESP32 code for automatic device appearance in Home Assistant.

## Architecture

```
┌─────────────────┐     BLE      ┌─────────────────┐    WiFi/MQTT    ┌─────────────────┐
│                 │  ─────────>  │                 │  ───────────>   │                 │
│  XIAO nRF52840  │   BTHome     │  XIAO ESP32     │                 │  MQTT Broker    │
│  (Motion Detect)│   Broadcast  │  (BLE Gateway)  │                 │                 │
│                 │              │                 │                 │                 │
└─────────────────┘              └─────────────────┘                 └─────────────────┘
                                                                            │
                                                                            │ MQTT
                                                                            ▼
                                                                     ┌─────────────────┐
                                                                     │                 │
                                                                     │ Home Assistant  │
                                                                     │                 │
                                                                     └─────────────────┘
```

## Configuration Options

| Parameter | Location | Description |
|-----------|----------|-------------|
| GATEWAY_NAME | Code | WiFi AP name |
| LED_PIN | Code | Built-in LED GPIO |
| scan_params | Code | BLE scan interval/window |

## Troubleshooting

### WiFi Connection Failed
- Verify WiFi SSID and password are correct
- ESP32-C5 supports both 2.4GHz and 5GHz (WiFi 6)

### MQTT Connection Failed
- Check MQTT server address and port
- Verify if server requires username/password
- Check serial output for error codes

### Not Receiving BLE Data
- Ensure nRF52840 device is nearby and advertising
- Verify device name starts with `SeeedUA-`
- Try restarting both devices

### Entering Config Mode
- Hold BOOT button within 5 seconds of power-on
- LED will start fast blinking

### MQTT Error Codes
| Code | Meaning |
|------|---------|
| -4 | Connection timeout |
| -3 | Connection lost |
| -2 | Connect failed |
| -1 | Disconnected |
| 1 | Bad protocol |
| 2 | Bad client ID |
| 3 | Unavailable |
| 4 | Bad credentials |
| 5 | Unauthorized |

## License

Part of the SeeedHADiscoveryBLE library. MIT License.
