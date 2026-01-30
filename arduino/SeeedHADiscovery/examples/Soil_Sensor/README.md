# Temperature and Humidity Sensor Example

Real-time reporting of soil moisture data to Home Assistant. Supports real soil moisture sensors (such as Seeed Studio soil sensors) and simulated data.

## Features

- **Soil Moisture Sensor** (displayed in %)
- **Configurable Update Interval** (adjust data reporting frequency as needed)
- **Optional Real Soil Moisture Sensor Support** (Capacitive/Resistive)
- **Simulated Data Mode** (for hardware-free testing scenarios)
- **WiFi Provisioning**: Web-based WiFi configuration (no hard-coded credentials required)

## Hardware Requirements

- XIAO ESP32-C3/C5/C6/S3 or other ESP32 development boards
- Soil Moisture Sensor (optional - simulated data can be used, Seeed Studio soil sensor recommended)

&gt; **Note**: XIAO ESP32-C5 supports dual-band WiFi (2.4GHz and 5GHz)

### Sensor Pin Connections

| Function | Pin | Description |
| :--- | :--- | :--- |
| Excitation Signal (EX) | **D3** | Sends excitation square wave signal |
| Analog Signal (SIG) | **D1** | Reads soil capacitance response voltage |
| VCC | 3.3V | Power supply (Do not connect to 5V) |
| GND | GND | Ground |

### LED Indicator Connections (Common Cathode Connection)

| LED Color | Pin | Status Definition |
| :--- | :--- | :--- |
| Green | **D8** | Moisture &gt; 60% (Wet) |
| Yellow | **D10** | Moisture 30%-60% (Moderate) |
| Red | **D9** | Moisture &lt; 30% (Dry) |

## Software Dependencies

### Required Libraries

Install via Arduino Library Manager:

| Library Name | Author | Description |
| :--- | :--- | :--- |
| **ArduinoJson** | Benoit Blanchon | JSON parsing |
| **WebSockets** | Markus Sattler | WebSocket communication |

### SeeedHADiscovery Library

Manually install from [GitHub](https://github.com/limengdu/SeeedHADiscovery).

## Quick Start

### 1. WiFi Configuration

**Method A: WiFi Provisioning (Recommended)**

WiFi provisioning is enabled by default. Upon first startup:

1. Device creates AP hotspot: `Seeed_TempHum_AP`
2. Connect your phone/computer to this AP
3. Browser opens automatically, or manually visit `http://192.168.4.1`
4. Select your WiFi network and enter password
5. Device restarts and connects to your WiFi

**Method B: Hard-coded Credentials**

To use hard-coded credentials:

```cpp
#define USE_WIFI_PROVISIONING false
const char* WIFI_SSID = "YourWiFiName";
const char* WIFI_PASSWORD = "YourWiFiPassword";
```

### 2. Configure Update Interval

```cpp
const unsigned long UPDATE_INTERVAL = 5000;  // 5 seconds
```

### 3. ESP32-C5 5GHz WiFi(Optional)

Force specific WiFi band on ESP32-C5:
```cpp
#define WIFI_BAND_MODE WIFI_BAND_MODE_5G_ONLY  // 或 WIFI_BAND_MODE_2G_ONLY
```

### 4. Upload and Connect

1. Select board: XIAO ESP32C6 (or your board)
2. Upload program
3. Open serial monitor (115200 baud)
4. Add device in Home Assistant

## Home Assistant Setup

1. Go to Settings → Devices & Services → Add Integration
2. Search for Seeed HA Discovery
3. Enter device IP address
4. one sensor will appear:
   - Humidity

## Created Entities

| Entity        | Type   | Unit | Precision |
| :------------ | :----- | :--- | :-------- |
| Soil Moisture | Sensor | %    | Integer   |

## Simulated Data Mode

When USE_SOIL_SENSOR is not defined, the device will automatically enable simulated data mode:
Soil Moisture: Fluctuates between 0-100% (simulating the full range from dry to over-wet)
This mode is suitable for scenarios without real sensors, facilitating code testing and Home Assistant integration debugging.

## Device Status Page

Access device status: http://<DeviceIP>/

## Troubleshooting

### Values Not Updating in HA
- Check WiFi connection
- Verify device is connected to HA
- Check error messages in serial monitor

### Cannot Access Provisioning Page
- Ensure you are connected to the device's AP
- Try manually visiting http://192.168.4.1

## License

Part of the SeeedHADiscovery library.
