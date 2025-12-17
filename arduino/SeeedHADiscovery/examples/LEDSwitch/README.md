# LED Switch Example

Control an LED from Home Assistant via WiFi. This example demonstrates basic switch entity creation and bidirectional control.

## Features

- Control LED on/off from Home Assistant
- Real-time state feedback
- Support for both onboard and external LEDs
- Configurable LED polarity (active high/low)
- **WiFi Provisioning**: Web-based WiFi configuration (no hardcoded credentials needed)

## Hardware Requirements

- XIAO ESP32-C3/C5/C6/S3 or other ESP32 development boards
- LED (onboard or external)

> **Note**: XIAO ESP32-C5 supports both 2.4GHz and 5GHz dual-band WiFi

### LED Pin Reference

| Board | Onboard LED Pin | Notes |
|-------|----------------|-------|
| XIAO ESP32-S3 | GPIO21 | Has User LED |
| XIAO ESP32-C6 | GPIO15 | Has User LED |
| XIAO ESP32-C3 | N/A | **No User LED** - requires external LED |
| XIAO ESP32-C5 | GPIO27 | Has User LED, supports 5GHz WiFi |

### External LED Wiring

If using external LED:

```
GPIO Pin ---[220Ω]--- LED (+) --- LED (-) --- GND
```

| Component | Connection |
|-----------|------------|
| LED positive (long leg) | GPIO through 220Ω resistor |
| LED negative (short leg) | GND |

## Software Dependencies

### Required Libraries

Install via Arduino Library Manager:

| Library | Author | Description |
|---------|--------|-------------|
| **ArduinoJson** | Benoit Blanchon | JSON parsing |
| **WebSockets** | Markus Sattler | WebSocket communication |

### SeeedHADiscovery Library

Install manually from [GitHub](https://github.com/limengdu/SeeedHADiscovery).

## Quick Start

### 1. WiFi Configuration

**Option A: WiFi Provisioning (Recommended)**

WiFi provisioning is enabled by default. On first boot:
1. Device creates AP hotspot: `Seeed_LED_AP`
2. Connect your phone/computer to this AP
3. Browser opens automatically, or navigate to `http://192.168.4.1`
4. Select your WiFi network and enter password
5. Device restarts and connects to your WiFi

**Option B: Hardcoded Credentials**

To use hardcoded credentials instead:
```cpp
#define USE_WIFI_PROVISIONING false
const char* WIFI_SSID = "Your_WiFi_SSID";
const char* WIFI_PASSWORD = "Your_WiFi_Password";
```

### 2. Configure LED (if using external)

For XIAO ESP32-C3 or external LED:
```cpp
#undef LED_BUILTIN
#define LED_BUILTIN D0  // Your external LED pin
```

### 3. Configure LED Polarity

```cpp
// true = Active LOW (XIAO series - LOW turns LED ON)
// false = Active HIGH (External LEDs - HIGH turns LED ON)
#define LED_ACTIVE_LOW true
```

### 4. ESP32-C5 5GHz WiFi (Optional)

To force a specific WiFi band on ESP32-C5:
```cpp
#define WIFI_BAND_MODE WIFI_BAND_MODE_5G_ONLY  // or WIFI_BAND_MODE_2G_ONLY
```

### 5. Upload and Connect

1. Select board: **XIAO ESP32C6** (or your board)
2. Upload the sketch
3. Open Serial Monitor (115200 baud)
4. Add device in Home Assistant using displayed IP

## Home Assistant Setup

1. Go to **Settings** → **Devices & Services** → **Add Integration**
2. Search for **Seeed HA Discovery**
3. Enter the device IP address
4. A "LED" switch entity will appear

## Entity Created

| Entity | Type | Icon |
|--------|------|------|
| LED | Switch | `mdi:led-on` |

## Device Status Page

Access device status at: `http://<device_ip>/`

## Troubleshooting

### LED not turning on/off correctly
- Check `LED_ACTIVE_LOW` setting matches your LED circuit
- Verify LED wiring if using external LED
- Ensure GPIO pin is correct for your board

### WiFi connection fails
- Verify SSID and password
- Check WiFi signal strength
- LED will blink if connection fails

### Can't access provisioning page
- Make sure you're connected to the device's AP
- Try navigating manually to `http://192.168.4.1`

## License

Part of the SeeedHADiscovery library.
