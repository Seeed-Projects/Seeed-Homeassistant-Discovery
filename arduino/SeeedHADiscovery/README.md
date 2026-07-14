# Seeed Home Assistant Discovery (WiFi)

[![Version](https://img.shields.io/badge/version-1.6.0-blue.svg)](https://github.com/limengdu/Seeed-Homeassistant-Discovery)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32-orange.svg)](https://www.espressif.com/)

A lightweight Arduino library for connecting ESP32 devices to Home Assistant via WiFi. Features automatic device discovery, real-time communication, and extensive sensor/switch support.

## ✨ Features

- **Auto Discovery** - Automatic device discovery via mDNS, no manual configuration needed
- **Real-time Communication** - WebSocket-based bidirectional communication with Home Assistant
- **Sensor Support** - Report sensor data (temperature, humidity, battery, etc.) to HA
- **Switch Control** - Receive control commands from HA to control LEDs, relays, etc.
- **HA State Subscription** - Subscribe to and receive Home Assistant entity state changes
- **Camera Streaming** - MJPEG camera streaming support (ESP32-S3 Sense)
- **WiFi Provisioning** - Web-based captive portal for WiFi configuration
- **5GHz WiFi** - ESP32-C5 supports dual-band 2.4GHz/5GHz WiFi

## 🔧 Supported Hardware

| Board | WiFi Band | Camera | Notes |
|-------|-----------|--------|-------|
| XIAO ESP32-C3 | 2.4GHz | ❌ | Low power, compact |
| XIAO ESP32-C5 | 2.4GHz + 5GHz | ❌ | Dual-band WiFi support |
| XIAO ESP32-C6 | 2.4GHz | ❌ | Thread/Zigbee capable |
| XIAO ESP32-S3 | 2.4GHz | ✅ | Camera + PSRAM support |
| XIAO ESP32-S3 Sense | 2.4GHz | ✅ | Built-in OV2640 camera |

## 📦 Installation

### Method 1: Download ZIP

1. Go to [GitHub Repository](https://github.com/limengdu/Seeed-Homeassistant-Discovery)
2. Click **Code → Download ZIP**
3. Extract the ZIP file
4. Copy the `arduino/SeeedHADiscovery` folder to your Arduino libraries directory:
   - Windows: `Documents/Arduino/libraries/`
   - macOS: `~/Documents/Arduino/libraries/`
   - Linux: `~/Arduino/libraries/`
5. Restart Arduino IDE

### Method 2: Git Clone

```bash
git clone https://github.com/limengdu/Seeed-Homeassistant-Discovery.git
```

Then copy the `arduino/SeeedHADiscovery` folder to your Arduino libraries directory.

> **Note:** This library is not available in Arduino Library Manager. Please use manual installation.

## 📚 Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | ^7.0.0 | JSON serialization |
| [WebSockets](https://github.com/Links2004/arduinoWebSockets) | ^2.4.0 | WebSocket communication |

## 🚀 Quick Start

### Basic Sensor Example

```cpp
#include <SeeedHADiscovery.h>

SeeedHADiscovery ha;
SeeedHASensor* tempSensor;
SeeedHASensor* humiSensor;

void setup() {
    Serial.begin(115200);
    
    // Set device info (optional)
    ha.setDeviceInfo("Living Room Sensor", "XIAO ESP32-C3", "1.0.0");
    
    // Connect to WiFi
    ha.begin("Your_WiFi_SSID", "Your_WiFi_Password");
    
    // Add sensors
    tempSensor = ha.addSensor("temperature", "Temperature", "temperature", "°C");
    humiSensor = ha.addSensor("humidity", "Humidity", "humidity", "%");
}

void loop() {
    ha.handle();  // Must call in loop!
    
    // Update sensor values
    tempSensor->setValue(25.5);
    humiSensor->setValue(60.0);
    
    delay(5000);
}
```

### Switch Control Example

```cpp
#include <SeeedHADiscovery.h>

SeeedHADiscovery ha;
SeeedHASwitch* ledSwitch;

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    
    ha.begin("Your_WiFi_SSID", "Your_WiFi_Password");
    
    // Add switch with callback
    ledSwitch = ha.addSwitch("led", "LED Light", "mdi:led-on");
    ledSwitch->onStateChange([](bool state) {
        digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
        Serial.printf("LED: %s\n", state ? "ON" : "OFF");
    });
}

void loop() {
    ha.handle();
}
```

### WiFi Provisioning Example

```cpp
#include <SeeedHADiscovery.h>

SeeedHADiscovery ha;
bool wifiConnected = false;

void setup() {
    Serial.begin(115200);
    
    // Start with WiFi provisioning support
    // If no saved credentials, starts AP mode for configuration
    wifiConnected = ha.beginWithProvisioning("My_IoT_Device_AP");
    
    if (wifiConnected) {
        // Add sensors/switches here
    }
}

void loop() {
    ha.handle();  // Handles both HA communication and provisioning
}
```

## 📂 Examples

| Example | Description |
|---------|-------------|
| [TemperatureHumidity](examples/TemperatureHumidity/) | Basic sensor data reporting |
| [LEDSwitch](examples/LEDSwitch/) | Controllable LED switch |
| [ButtonSwitch](examples/ButtonSwitch/) | Physical button + HA switch |
| [HAStateSubscribe](examples/HAStateSubscribe/) | Subscribe to HA entity states |
| [CameraStream](examples/CameraStream/) | MJPEG camera streaming (S3 Sense) |
| [WiFiProvisioning](examples/WiFiProvisioning/) | Web-based WiFi configuration |
| [XIAO_IR_Mate](examples/XIAO_IR_Mate/) | Universal infrared remote with offline Gree control and HA learning |
| [IoTButtonV2_DeepSleep](examples/IoTButtonV2_DeepSleep/) | Battery-powered IoT button with deep sleep |
| [reTerminal_E1001_HASubscribe_Display](examples/reTerminal_E1001_HASubscribe_Display/) | E-Paper display with HA states |
| [reTerminal_E1002_HASubscribe_Display](examples/reTerminal_E1002_HASubscribe_Display/) | Color E-Paper display with HA states |

The IR Mate example keeps its infrared implementation and optional dependencies inside the example folder.

## 🔌 API Reference

### SeeedHADiscovery Class

#### Configuration
```cpp
void setDeviceInfo(const String& name, const String& model, const String& version);
void enableDebug(bool enable = true);
```

#### Connection
```cpp
bool begin(const char* ssid, const char* password);
bool beginWithProvisioning(const String& apSSID = "Seeed_IoT_Device_AP");
void clearWiFiCredentials();
void enableResetButton(int pin, bool activeLow = true);
```

#### Entity Management
```cpp
SeeedHASensor* addSensor(const String& id, const String& name, const String& deviceClass = "", const String& unit = "");
SeeedHASwitch* addSwitch(const String& id, const String& name, const String& icon = "");
```

#### HA State Subscription
```cpp
void onHAState(HAStateCallback callback);
SeeedHAState* getHAState(const String& entityId);
```

#### Protocol Extensions
```cpp
void onProtocolMessage(const String& type, HAProtocolMessageCallback callback);
void onDiscovery(HADiscoveryCallback callback);
void sendProtocolMessage(JsonDocument& doc, uint8_t clientNum = 255);
```

#### Runtime
```cpp
void handle();  // Must call in loop()
bool isWiFiConnected() const;
bool isHAConnected() const;
void notifySleep();  // Call before entering deep sleep
```

### SeeedHASensor Class

```cpp
void setValue(float value);
void setStateClass(const String& stateClass);
void setPrecision(int precision);
void setIcon(const String& icon);
```

### SeeedHASwitch Class

```cpp
void setState(bool state);
void toggle();
bool getState() const;
void onStateChange(SwitchCallback callback);
```

## 🔗 ESP32-C5 5GHz WiFi Support

ESP32-C5 is the only XIAO board supporting 5GHz WiFi. To configure WiFi band mode:

```cpp
#include <WiFi.h>

void setup() {
    // Set WiFi band mode (requires Arduino ESP32 Core 3.3.0+)
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 2)
        // WIFI_BAND_MODE_AUTO - Auto select (default)
        // WIFI_BAND_MODE_2G_ONLY - 2.4GHz only
        // WIFI_BAND_MODE_5G_ONLY - 5GHz only
        WiFi.setBandMode(WIFI_BAND_MODE_AUTO);
    #endif
    
    // Then proceed with normal connection
    ha.begin("Your_5GHz_SSID", "Your_Password");
}
```

## 🏠 Home Assistant Integration

This library works with the [Seeed HA Discovery](https://github.com/limengdu/Seeed-Homeassistant-Discovery) custom integration for Home Assistant.

### Installation

1. Install HACS in Home Assistant
2. Add custom repository: `https://github.com/limengdu/Seeed-Homeassistant-Discovery`
3. Install "Seeed HA Discovery" integration
4. Restart Home Assistant
5. Go to Settings → Devices & Services → Add Integration → Seeed HA Discovery

Devices will be automatically discovered when they connect to the same network.

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📧 Support

- GitHub Issues: [Report a bug](https://github.com/limengdu/Seeed-Homeassistant-Discovery/issues)
- Seeed Forum: [Community Support](https://forum.seeedstudio.com/)
