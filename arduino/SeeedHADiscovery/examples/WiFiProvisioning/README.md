# WiFi Provisioning Example

This example demonstrates the web-based WiFi provisioning feature of the SeeedHADiscovery library, providing a captive portal for WiFi configuration.

## Features

- **Web-based Configuration**: Configure WiFi via browser on any device
- **Captive Portal**: Automatically opens configuration page when connected to AP
- **Network Scanning**: Scan and display available WiFi networks
- **Credential Persistence**: Saved credentials persist across reboots
- **Refresh Networks**: Click to rescan available networks
- **Modern UI**: Beautiful, responsive web interface
- **Reset Button**: Long press a button for 6 seconds to clear credentials and re-enter provisioning mode
- **ESP32-C5 5GHz Support**: Configure band mode for dual-band WiFi

## How It Works

1. **First Boot** (no saved credentials):
   - Device creates an AP hotspot (default: `Seeed_HA_Device_AP`)
   - Red LED pattern indicates provisioning mode

2. **WiFi Configuration**:
   - Connect your phone/computer to the AP
   - Browser automatically opens captive portal, or navigate to `http://192.168.4.1`
   - Select your WiFi network from the list
   - Enter password and click Connect
   - Device saves credentials and restarts

3. **Subsequent Boots**:
   - Device automatically connects using saved credentials
   - If connection fails, falls back to AP mode for reconfiguration

## Hardware Requirements

- Any ESP32 board (ESP32, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-S3, etc.)

> **Note**: XIAO ESP32-C5 supports both 2.4GHz and 5GHz dual-band WiFi

## Software Dependencies

- SeeedHADiscovery library
- ArduinoJson (by Benoit Blanchon)
- WebSockets (by Markus Sattler)

## Usage

### Basic Usage

```cpp
#include <SeeedHADiscovery.h>

SeeedHADiscovery ha;

void setup() {
    Serial.begin(115200);
    
    ha.setDeviceInfo("My Device", "ESP32", "1.0.0");
    ha.enableDebug(true);
    
    // Start with WiFi provisioning
    bool connected = ha.beginWithProvisioning("My_Device_AP");
    
    if (!connected) {
        // In AP mode, waiting for configuration
        return;
    }
    
    // WiFi connected, continue with setup...
}

void loop() {
    ha.handle();  // Must call!
    
    if (ha.isProvisioningActive()) {
        // Still in provisioning mode
        delay(10);
        return;
    }
    
    // Normal operation...
}
```

### Custom AP Name

```cpp
// Use custom AP name
ha.beginWithProvisioning("My_Custom_AP_Name");
```

### ESP32-C5 5GHz WiFi Configuration

```cpp
// Force specific band mode on ESP32-C5
#define WIFI_BAND_MODE WIFI_BAND_MODE_5G_ONLY  // or WIFI_BAND_MODE_2G_ONLY or WIFI_BAND_MODE_AUTO

// In setup(), before beginWithProvisioning:
#if defined(WIFI_BAND_MODE) && defined(CONFIG_SOC_WIFI_SUPPORT_5G)
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 2)
        WiFi.setBandMode(WIFI_BAND_MODE);
    #endif
#endif
```

### Clear Saved Credentials

```cpp
// To reconfigure WiFi, clear saved credentials
ha.clearWiFiCredentials();
// Device will enter AP mode on next boot
```

### Enable Reset Button

```cpp
// Enable reset button on GPIO0 (BOOT button on most ESP32 boards)
// Long press for 6 seconds to clear credentials and start AP mode
ha.enableResetButton(0);  // GPIO0, active LOW (default)

// For active HIGH button:
ha.enableResetButton(5, false);  // GPIO5, active HIGH
```

### Using SeeedWiFiProvisioning Directly

For more control, you can use `SeeedWiFiProvisioning` class directly:

```cpp
#include <SeeedWiFiProvisioning.h>

SeeedWiFiProvisioning provisioning;

void setup() {
    provisioning.setAPSSID("My_AP");
    provisioning.setConnectTimeout(20000);  // 20 seconds
    provisioning.enableDebug(true);
    
    // Register callbacks
    provisioning.onWiFiConnected([]() {
        Serial.println("WiFi connected!");
    });
    
    provisioning.onAPStarted([]() {
        Serial.println("AP mode started!");
    });
    
    provisioning.begin();
}

void loop() {
    provisioning.handle();
}
```

## Web Interface

The captive portal provides a modern, responsive interface:

- **Network List**: Shows available WiFi networks with signal strength
- **Security Indicator**: Lock icon for password-protected networks
- **Signal Bars**: Visual signal strength indicator
- **Encryption Type**: Shows WPA2, WPA3, etc.
- **Refresh Button**: Rescan for networks
- **Reset Button**: Clear saved credentials

## API Reference

### SeeedHADiscovery Methods

| Method | Description |
|--------|-------------|
| `beginWithProvisioning(apSSID)` | Start with WiFi provisioning support |
| `isProvisioningActive()` | Check if AP mode is active |
| `clearWiFiCredentials()` | Clear saved WiFi credentials |
| `enableResetButton(pin, activeLow)` | Enable reset button (long press 6s to reset WiFi) |
| `disableResetButton()` | Disable reset button |
| `getProvisioning()` | Get SeeedWiFiProvisioning instance |

### SeeedWiFiProvisioning Methods

| Method | Description |
|--------|-------------|
| `setAPSSID(ssid)` | Set AP hotspot name |
| `setAPPassword(password)` | Set AP password (empty = open) |
| `setConnectTimeout(ms)` | Set connection timeout |
| `enableDebug(enable)` | Enable/disable debug output |
| `begin()` | Start with saved credentials or AP mode |
| `begin(ssid, password)` | Start with specific credentials |
| `startAPMode()` | Force start AP mode |
| `stopAPMode()` | Stop AP mode |
| `handle()` | Handle provisioning tasks (call in loop) |
| `isWiFiConnected()` | Check WiFi connection status |
| `isAPModeActive()` | Check if AP mode is active |
| `hasCredentials()` | Check if credentials are saved |
| `getSavedSSID()` | Get saved SSID |
| `clearCredentials()` | Clear saved credentials |
| `scanNetworks()` | Scan for WiFi networks |
| `enableResetButton(pin, activeLow)` | Enable reset button (6s long press) |
| `disableResetButton()` | Disable reset button |

## Troubleshooting

### Device doesn't create AP

- Check if WiFi credentials are already saved
- Try clearing credentials: `ha.clearWiFiCredentials()`

### Can't connect to AP

- Ensure you're close enough to the device
- Try disabling mobile data on your phone
- Some devices may require manually connecting to the AP

### Captive portal doesn't open automatically

- Navigate manually to `http://192.168.4.1`
- Try clearing browser cache

### Connection fails after configuration

- Verify the password is correct
- Check if the router is reachable
- Try moving closer to the router

### ESP32-C5 not finding 5GHz networks

- Ensure `WIFI_BAND_MODE` is set correctly
- Verify your router broadcasts on 5GHz band
- Check if ESP-IDF version supports 5GHz (requires 5.4.2+)

## License

MIT License - See LICENSE file for details.
