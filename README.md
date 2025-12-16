# Seeed Home Assistant Discovery

<p align="center">
  <img src="custom_components/seeed_ha_discovery/icon.png" width="128" alt="Seeed HA Discovery">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/ESP32-C3%20%7C%20C5%20%7C%20C6%20%7C%20S3-blue" alt="ESP32 Support">
  <img src="https://img.shields.io/badge/nRF52840-BLE-purple" alt="nRF52840 Support">
  <img src="https://img.shields.io/badge/Home%20Assistant-2025.12+-green" alt="Home Assistant">
  <img src="https://img.shields.io/badge/Arduino-IDE%20%7C%20PlatformIO-orange" alt="Arduino">
  <img src="https://img.shields.io/badge/HACS-Custom-41BDF5" alt="HACS Custom">
</p>

**Seeed HA Discovery** is a complete solution for easily connecting ESP32/nRF52840 devices to Home Assistant, provided by [Seeed Studio](https://www.seeedstudio.com/).

### 🎯 What Can It Do?

With just a few lines of code in **Arduino IDE** or **PlatformIO** for your **XIAO** series development boards, you can connect to Home Assistant via **WiFi** or **Bluetooth (BLE)**:

| Connection | Supported Devices | Features |
|------------|-------------------|----------|
| 📶 **WiFi** | XIAO ESP32-C3/C5/C6/S3 | Bidirectional communication, WebSocket real-time updates |
| 📡 **Bluetooth (BLE)** | XIAO ESP32-C3/C5/C6/S3, **XIAO nRF52840** | Ultra-low power, BTHome v2 protocol, passive advertising |

> 📡 **XIAO ESP32-C5** supports both 2.4GHz and 5GHz dual-band WiFi for better connectivity options

| Feature | Direction | WiFi | BLE |
|---------|-----------|------|-----|
| 📤 **Report Sensor Data** | Device → HA | ✅ | ✅ |
| 📥 **Receive Control Commands** | HA → Device | ✅ | ✅ (GATT) |
| 📷 **Camera Streaming** | Device → HA | ✅ (ESP32-S3) | ❌ |
| 🔄 **Get HA States** | HA → Device | ✅ (v2.3 New) | ✅ (v2.4 New) |
| 🔋 **Ultra-Low Power** | - | ❌ | ✅ (Broadcast Mode) |

### 💡 No Complex Configuration

- ✅ **No MQTT** - No need to set up an MQTT broker
- ✅ **No Cloud Services** - Pure local network communication, data stays at home
- ✅ **Auto Discovery** - Home Assistant automatically recognizes devices when they come online
- ✅ **Plug and Play** - Copy example code, modify configuration, and run

## ⚡ One-Click Installation

Click the button below to add this integration to your Home Assistant:

[![Open your Home Assistant instance and open a repository inside the Home Assistant Community Store.](https://my.home-assistant.io/badges/hacs_repository.svg)](https://my.home-assistant.io/redirect/hacs_repository/?owner=limengdu&repository=Seeed-Homeassistant-Discovery&category=integration)

> **Prerequisites**: Your Home Assistant must have [HACS](https://hacs.xyz/) installed

## 🔌 Web Firmware Flasher

**Don't want to set up Arduino IDE?** Use our web-based firmware flasher to program your device directly from the browser!

🌐 **[Open Web Flasher](https://limengdu.github.io/Seeed-Homeassistant-Discovery/)**

| Feature | Description |
|---------|-------------|
| 🖥️ **No Software Required** | Flash directly from Chrome/Edge browser |
| 🔍 **Auto Chip Detection** | Automatically detects ESP32-C3/C5/C6/S3 |
| 📦 **Pre-built Firmware** | Ready-to-use examples for quick start |
| 🌍 **Bilingual** | English and Chinese interface |

**Available Firmware:**

| Category | Firmware | Supported Chips |
|----------|----------|-----------------|
| 🏷️ **Seeed Products** | IoT Button V2 | ESP32-C6 |
| 🏷️ **Seeed Products** | CameraStream | ESP32-S3 Sense |
| 🏷️ **Seeed Products** | reTerminal E1001/E1002 | ESP32-S3 |
| 🔧 **Universal** | WiFi Provisioning | C3/C5/C6/S3 |
| 🔧 **Universal** | Button Switch | C3/C5/C6/S3 |
| 🔧 **Universal** | LED Switch | C5/C6/S3 |
| 🔧 **Universal** | HA State Subscribe | C3/C5/C6/S3 |
| 🔧 **Universal** | Temperature & Humidity | C3/C5/C6/S3 |

> 💡 **Tip**: Universal firmware supports auto chip detection - just connect your XIAO board and click flash!

## ✨ Features

### WiFi Version
- 🔍 **Auto Discovery** - Devices are automatically discovered by Home Assistant after connecting to WiFi
- 📡 **Real-time Communication** - Bidirectional real-time communication using WebSocket
- 🎯 **Simple to Use** - Connect sensors to HA with just a few lines of code
- 🌡️ **Sensor Support** - Support for temperature, humidity, and various other sensors (upstream data)
- 💡 **Switch Control** - Support for LED, relay, and other switch controls (downstream commands)
- 📷 **Camera Streaming** - Support XIAO ESP32-S3 Sense camera live feed (v2.2 New)
- 🔄 **HA State Subscription** - Device can subscribe to HA entity states, ideal for display applications (v2.3 New)
- 📱 **Status Page** - Built-in web page to view device status

### BLE Version (v2.0 New)
- 🔋 **Ultra-Low Power** - Passive broadcast mode, suitable for battery-powered devices
- 📡 **BTHome v2** - Uses the BTHome protocol natively supported by Home Assistant
- 🎯 **Zero Configuration** - No additional integration needed, HA automatically recognizes BTHome devices
- 📱 **Support nRF52840** - Not limited to ESP32, also supports XIAO nRF52840
- 🔘 **Event Support** - Support for button single click, double click, long press, and other events
- 🔄 **Bidirectional Control** - Support for GATT bidirectional communication, remote switch control
- 📥 **HA State Subscription** - BLE devices can receive HA entity states, ideal for display applications (v2.4 New)

## 🤔 Why Not Use ESPHome?

ESPHome is an excellent project, but it's not suitable for everyone. If you have the following needs, **Seeed HA Discovery** might be better for you:

### 1. 🎓 More Familiar with Arduino Programming

> *"I'm used to writing code with Arduino IDE, don't want to learn YAML configuration syntax"*

| ESPHome | Seeed HA Discovery |
|---------|-------------------|
| Uses YAML configuration files | Uses standard **C/C++ code** |
| Based on ESP-IDF framework by default (optional Arduino) | Based on **Arduino framework** |
| Need to learn new syntax | Leverage your existing Arduino skills |

```cpp
// Seeed HA Discovery - Just the Arduino code you're familiar with
void setup() {
    ha.begin("WiFi", "password");
    tempSensor = ha.addSensor("temp", "Temperature", "temperature", "°C");
}

void loop() {
    ha.handle();
    tempSensor->setValue(25.5);
}
```

### 2. 📚 Richer Arduino Ecosystem

> *"I want to use a certain Arduino library, but ESPHome doesn't support it"*

- ✅ **Use any Arduino library directly** - Sensor drivers, displays, communication modules...
- ✅ **Deep sleep, low power modes** - Full control of ESP32 power management
- ✅ **Complex business logic** - Implement any functionality you want with code
- ✅ **Custom communication protocols** - Not limited by the framework

### 3. 🔄 ESPHome Updates Too Frequently

> *"The configuration that worked last month is throwing errors this month"*

- ESPHome's **breaking updates** are frequent, making historical tutorials easily outdated
- Component APIs often change, requiring constant modifications to old code
- **Seeed HA Discovery** uses stable Arduino APIs with better backward compatibility

### 4. ⏱️ Compilation Speed

> *"ESPHome takes several minutes to compile"*

- ESPHome has more and more features, taking longer and longer to compile
- Arduino projects compile faster with higher iteration efficiency
- Incremental compilation is more effective, recompiling in seconds after code changes

### 5. 🚀 No Need to Wait for Official Review

> *"I want to add a new sensor, but ESPHome official review is too slow"*

- Adding new components to ESPHome requires submitting a PR with long review cycles and strict standards
- **Seeed HA Discovery** lets you freely write code without waiting for anyone
- Your sensors, your code, your pace

### 📊 Use Case Comparison

| Scenario | Recommended Solution |
|----------|---------------------|
| Quickly deploy standard sensors | ESPHome ✅ |
| Need custom Arduino code | **Seeed HA Discovery** ✅ |
| Don't want to learn new syntax | **Seeed HA Discovery** ✅ |
| Using uncommon sensors/modules | **Seeed HA Discovery** ✅ |
| Need low power/deep sleep | **Seeed HA Discovery** ✅ |
| Pure GUI configuration, zero code | ESPHome ✅ |

---

## 📦 Components

This project consists of three parts:

1. **Home Assistant Integration** (`custom_components/seeed_ha_discovery/`)
   - Automatically discover WiFi devices on the local network
   - Receive and display sensor data
   - Send control commands to devices

2. **WiFi Arduino Library** (`arduino/SeeedHADiscovery/`)
   - For ESP32 device WiFi programming
   - Support sensor reporting and switch control
   - WebSocket bidirectional communication

3. **BLE Arduino Library** (`arduino/SeeedHADiscoveryBLE/`) - **v2.0 New**
   - For ESP32/nRF52840 Bluetooth programming
   - Based on BTHome v2 protocol
   - Ultra-low power passive broadcast

## 🚀 Quick Start

### 1. Install Home Assistant Integration

**Method A: One-Click Installation via HACS (Recommended)**

Click the "One-Click Installation" button above, or add manually:

1. Open HACS → Integrations
2. Click "⋮" in the top right → "Custom repositories"
3. Enter `https://github.com/limengdu/Seeed-Homeassistant-Discovery`
4. Select category "Integration"
5. Click Add, then search for "Seeed HA Discovery" and install
6. Restart Home Assistant

**Method B: Manual Installation**

Copy the `custom_components/seeed_ha_discovery` folder to Home Assistant's `config/custom_components/` directory, then restart Home Assistant.

### 2. Install Arduino Library

Choose the appropriate library based on your connection method:

#### WiFi Version (SeeedHADiscovery)

**Arduino IDE:**
1. Download the `arduino/SeeedHADiscovery` folder
2. Copy to `Documents/Arduino/libraries/`
3. Install dependency libraries (via Library Manager):
   - ArduinoJson (by Benoit Blanchon)
   - WebSockets (by Markus Sattler)

**PlatformIO:**
```ini
lib_deps =
    bblanchon/ArduinoJson@^7.0.0
    links2004/WebSockets@^2.4.0
```

#### BLE Version (SeeedHADiscoveryBLE)

**Arduino IDE:**
1. Download the `arduino/SeeedHADiscoveryBLE` folder
2. Copy to `Documents/Arduino/libraries/`
3. Install the corresponding BLE dependency library based on your board:

| Board | Dependency | Installation Method |
|-------|------------|---------------------|
| **ESP32 series** (C3/C6/S3) | NimBLE-Arduino | Search "NimBLE-Arduino" in Arduino Library Manager |

> ⚠️ **ESP32 must install NimBLE-Arduino library**, otherwise compilation will fail!
>
> NimBLE is lighter and more stable than the official ESP32 Bluetooth library, and is the preferred choice for ESP32 BLE development.

**PlatformIO:**
```ini
; ESP32 series
lib_deps =
    h2zero/NimBLE-Arduino@^1.4.0

; nRF52840 (mbed)
; ArduinoBLE is built into Seeed mbed core, no additional installation needed
```

### 3. Write Arduino Program

#### WiFi Example - Temperature and Humidity Sensor

```cpp
#include <SeeedHADiscovery.h>

const char* WIFI_SSID = "Your WiFi Name";
const char* WIFI_PASSWORD = "Your WiFi Password";

SeeedHADiscovery ha;
SeeedHASensor* tempSensor;
SeeedHASensor* humiditySensor;

void setup() {
    Serial.begin(115200);
    ha.setDeviceInfo("Living Room Sensor", "ESP32-C3", "1.0.0");
    ha.enableDebug(true);

    if (!ha.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("WiFi connection failed!");
        while (1) delay(1000);
    }

    tempSensor = ha.addSensor("temperature", "Temperature", "temperature", "°C");
    tempSensor->setPrecision(1);

    humiditySensor = ha.addSensor("humidity", "Humidity", "humidity", "%");
    humiditySensor->setPrecision(0);
}

void loop() {
    ha.handle();

    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        lastUpdate = millis();
        tempSensor->setValue(25.5);
        humiditySensor->setValue(55);
    }
}
```

#### WiFi Example - Camera Streaming (XIAO ESP32-S3 Sense)

```cpp
#include <SeeedHADiscovery.h>
#include "esp_camera.h"

const char* WIFI_SSID = "Your_WiFi_SSID";
const char* WIFI_PASSWORD = "Your_WiFi_Password";

SeeedHADiscovery ha;

void setup() {
    Serial.begin(115200);
    
    // Initialize camera (XIAO ESP32-S3 Sense specific pins)
    camera_config_t config;
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.pin_xclk = 10;
    config.pin_sccb_sda = 40;
    config.pin_sccb_scl = 39;
    config.pin_d7 = 48;
    config.pin_d6 = 11;
    config.pin_d5 = 12;
    config.pin_d4 = 14;
    config.pin_d3 = 16;
    config.pin_d2 = 18;
    config.pin_d1 = 17;
    config.pin_d0 = 15;
    config.pin_vsync = 38;
    config.pin_href = 47;
    config.pin_pclk = 13;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    
    esp_camera_init(&config);
    
    ha.setDeviceInfo("XIAO Camera", "XIAO ESP32-S3 Sense", "1.0.0");
    ha.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Start camera server on port 82
    // Still image: http://<IP>:82/camera
    // MJPEG stream: http://<IP>:82/stream
    startCameraServer();
}

void loop() {
    ha.handle();
}
```

> **Note**: Camera example requires XIAO ESP32-S3 Sense with OV2640 camera module.
> Make sure to enable PSRAM in Arduino IDE: Tools → PSRAM → OPI PSRAM

#### BLE Example - Temperature and Humidity Sensor (Ultra-Low Power)

```cpp
#include <SeeedHADiscoveryBLE.h>

SeeedHADiscoveryBLE ble;
SeeedBLESensor* tempSensor;
SeeedBLESensor* humiditySensor;
SeeedBLESensor* batterySensor;

void setup() {
    Serial.begin(115200);
    ble.enableDebug(true);

    if (!ble.begin("XIAO Temperature Sensor")) {
        Serial.println("BLE initialization failed!");
        while (1) delay(1000);
    }

    // Use BTHome standard sensor types
    tempSensor = ble.addTemperature();
    humiditySensor = ble.addHumidity();
    batterySensor = ble.addBattery();
}

void loop() {
    // Set sensor values
    tempSensor->setValue(25.5);      // Temperature 25.5°C
    humiditySensor->setValue(55.0);  // Humidity 55%
    batterySensor->setValue(100);    // Battery 100%

    // Send BLE broadcast
    ble.advertise();

    // Wait 10 seconds (BLE is suitable for low frequency updates)
    delay(10000);
}
```

#### BLE Example - LED Switch Control (Bidirectional Communication)

```cpp
#include <SeeedHADiscoveryBLE.h>

SeeedHADiscoveryBLE ble;
SeeedBLESwitch* ledSwitch;

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    
    ble.enableDebug(true);
    
    // Enable GATT server (second parameter is true)
    if (!ble.begin("XIAO LED Controller", true)) {
        Serial.println("BLE initialization failed!");
        while (1) delay(1000);
    }

    // Add LED switch
    ledSwitch = ble.addSwitch("led", "Onboard LED");
    
    // Register callback: executed when HA sends control command
    ledSwitch->onStateChange([](bool state) {
        digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
        Serial.printf("LED: %s\n", state ? "ON" : "OFF");
    });
}

void loop() {
    ble.loop();  // Must call! Handles GATT events
    delay(10);
}
```

### 4. Add Device in Home Assistant

**WiFi Device:** Will be automatically discovered! Or add manually:
1. Go to **Settings** → **Devices & Services**
2. Click **Add Integration**
3. Search for **Seeed HA Discovery**
4. Enter the IP address of the ESP32

**BLE Device:** Uses BTHome protocol, will be automatically discovered by Home Assistant!
1. Make sure HA has a Bluetooth adapter or ESP32 Bluetooth proxy
2. Device will automatically appear in **Settings** → **Devices & Services** → **BTHome**

### 5. Configure HA State Subscription (v2.3 New)

Both WiFi and BLE devices can subscribe to Home Assistant entity states, which is particularly useful for display devices.

**Configuration Steps:**
1. Go to **Settings** → **Devices & Services** → **Seeed HA Discovery**
2. Find your device and click **Configure**
3. Select entities to subscribe from the dropdown list
4. Click **Submit**

**Supported Entity Types:**
- sensor
- binary_sensor
- switch
- light
- climate
- weather

**How It Works:**
- When entity state changes, HA **automatically pushes** to the device
- Subscriptions are **automatically restored** after device reconnection
- After modifying subscription config, device **immediately receives** new entity states

**BLE Device Limitations:**
- Maximum 16 entities supported (limited by BLE bandwidth)
- Requires GATT bidirectional mode (`ble.begin("Device Name", true)`)

---

## 📖 API Reference

### WiFi Library - SeeedHADiscovery Class

| Method | Description |
|--------|-------------|
| `setDeviceInfo(name, model, version)` | Set device information |
| `enableDebug(enable)` | Enable debug output |
| `begin(ssid, password)` | Connect to WiFi and start service |
| `addSensor(id, name, deviceClass, unit)` | Add sensor (upstream data) |
| `addSwitch(id, name, icon)` | Add switch (downstream control) |
| `handle()` | Handle network events (must call in loop) |
| `isWiFiConnected()` | Check WiFi connection |
| `isHAConnected()` | Check HA connection |
| `getLocalIP()` | Get IP address |
| `onHAState(callback)` | Register HA state change callback |
| `getHAState(entityId)` | Get state object for specified entity |
| `getHAStates()` | Get all subscribed entity states |
| `clearHAStates()` | Clear all HA state cache |

### WiFi Library - SeeedHASensor Class

| Method | Description |
|--------|-------------|
| `setValue(value)` | Set sensor value (automatically push to HA) |
| `setStateClass(stateClass)` | Set state class |
| `setPrecision(precision)` | Set decimal precision |
| `setIcon(icon)` | Set icon (mdi:xxx format) |

### WiFi Library - SeeedHASwitch Class

| Method | Description |
|--------|-------------|
| `onStateChange(callback)` | Register state change callback (receive HA commands) |
| `setState(state)` | Set switch state (sync to HA) |
| `toggle()` | Toggle switch state |
| `getState()` | Get current state |
| `setIcon(icon)` | Set icon (mdi:xxx format) |

### WiFi Library - SeeedHAState Class (v2.3 New)

| Method | Description |
|--------|-------------|
| `getEntityId()` | Get entity ID |
| `getFriendlyName()` | Get friendly name |
| `getString()` | Get state as string |
| `getFloat()` | Get state as float value |
| `getBool()` | Get state as boolean |
| `getUnit()` | Get unit of measurement |
| `getDeviceClass()` | Get device class |
| `hasValue()` | Check if has valid value |

### BLE Library - SeeedHADiscoveryBLE Class

| Method | Description |
|--------|-------------|
| `begin(deviceName, enableGattServer)` | Initialize BLE (second parameter enables bidirectional control) |
| `enableDebug(enable)` | Enable debug output |
| `addSensor(objectId)` | Add BTHome sensor |
| `addTemperature()` | Add temperature sensor (convenience method) |
| `addHumidity()` | Add humidity sensor (convenience method) |
| `addBattery()` | Add battery sensor (convenience method) |
| `addButton()` | Add button event (convenience method) |
| `addSwitch(id, name)` | Add switch (for bidirectional control) |
| `advertise()` | Send BLE broadcast |
| `loop()` | Handle GATT events (must call when GATT enabled) |
| `stop()` | Stop BLE |
| `onHAState(callback)` | Register HA state change callback (v2.4 New) |
| `getHAState(index)` | Get HA state object by index (v2.4 New) |
| `getSubscribedEntityCount()` | Get number of received entities (v2.4 New) |
| `isConnected()` | Check GATT connection status |

### BLE Library - SeeedBLESensor Class

| Method | Description |
|--------|-------------|
| `setValue(value)` | Set sensor value (integer or float) |
| `setState(state)` | Set binary state |
| `triggerButton(event)` | Trigger button event |

### BLE Library - SeeedBLESwitch Class

| Method | Description |
|--------|-------------|
| `onStateChange(callback)` | Register state change callback (receive HA commands) |
| `setState(state)` | Set switch state (sync to HA) |
| `getState()` | Get current state |

### BLE Library - SeeedBLEHAState Class (v2.4 New)

| Method | Description |
|--------|-------------|
| `getEntityId()` | Get HA entity ID |
| `getString()` | Get state as string |
| `getFloat()` | Get state as float value |
| `getInt()` | Get state as integer value |
| `getBool()` | Get state as boolean |
| `hasValue()` | Check if has valid value |

### BLE Button Event Types

| Event | Description |
|-------|-------------|
| `BTHOME_BUTTON_PRESS` | Single click |
| `BTHOME_BUTTON_DOUBLE` | Double click |
| `BTHOME_BUTTON_TRIPLE` | Triple click |
| `BTHOME_BUTTON_LONG_PRESS` | Long press |

### Common BTHome Sensor Types

| Type | Description | Precision |
|------|-------------|-----------|
| `BTHOME_TEMPERATURE` | Temperature | 0.01°C |
| `BTHOME_HUMIDITY` | Humidity | 0.01% |
| `BTHOME_PRESSURE` | Pressure | 0.01 hPa |
| `BTHOME_ILLUMINANCE` | Illuminance | 0.01 lux |
| `BTHOME_BATTERY` | Battery | 1% |
| `BTHOME_VOLTAGE` | Voltage | 0.001 V |
| `BTHOME_PM25` | PM2.5 | 1 μg/m³ |
| `BTHOME_CO2` | CO2 | 1 ppm |
| `BTHOME_BUTTON` | Button Event | - |

---

## 📁 Project Structure

```
seeed-ha-discovery/
├── custom_components/
│   └── seeed_ha_discovery/       # Home Assistant Integration
│       ├── __init__.py           # Main entry
│       ├── manifest.json         # Integration manifest (v2.3.0)
│       ├── config_flow.py        # Configuration flow
│       ├── const.py              # Constants definition
│       ├── coordinator.py        # Data coordinator
│       ├── device.py             # Device communication
│       ├── sensor.py             # Sensor platform
│       ├── switch.py             # Switch platform
│       ├── camera.py             # Camera platform (v2.2 New)
│       ├── strings.json          # Strings
│       └── translations/         # Translation files
├── arduino/
│   ├── SeeedHADiscovery/         # WiFi Arduino Library
│   │   ├── src/
│   │   │   ├── SeeedHADiscovery.h
│   │   │   └── SeeedHADiscovery.cpp
│   │   ├── examples/
│   │   │   ├── TemperatureHumidity/  # Temperature/Humidity sensor example
│   │   │   ├── LEDSwitch/            # LED switch example
│   │   │   ├── ButtonSwitch/         # Button switch example (v1.2)
│   │   │   └── CameraStream/         # Camera streaming example (v1.3)
│   │   ├── library.json
│   │   └── library.properties
│   └── SeeedHADiscoveryBLE/      # BLE Arduino Library (v2.0 New)
│       ├── src/
│       │   ├── SeeedHADiscoveryBLE.h
│       │   └── SeeedHADiscoveryBLE.cpp
│       ├── examples/
│       │   ├── TemperatureBLE/       # Temperature/Humidity sensor example (passive broadcast)
│       │   ├── ButtonBLE/            # Button switch example (GATT bidirectional)
│       │   ├── LEDSwitchBLE/         # LED switch example (GATT bidirectional)
│       │   └── HAStateSubscribeBLE/  # HA state subscription example (v2.4 New)
│       ├── library.json
│       └── library.properties
├── hacs.json
└── README.md
```

## 🔧 Supported Hardware

| Development Board | WiFi | BLE | Camera | Status |
|-------------------|------|-----|--------|--------|
| XIAO ESP32-C3 | ✅ | ✅ | ❌ | Tested |
| **XIAO ESP32-C5** | ✅ | ✅ | ❌ | Tested (5GHz WiFi) |
| XIAO ESP32-C6 | ✅ | ✅ | ❌ | Tested |
| XIAO ESP32-S3 | ✅ | ✅ | ❌ | Tested |
| **XIAO ESP32-S3 Sense** | ✅ | ✅ | ✅ | Tested |
| XIAO nRF52840 | ❌ | ✅ | ❌ | Tested |
| ESP32 (Original) | ✅ | ✅ | ❌ | Tested |

> 📷 **Camera feature** only supports **XIAO ESP32-S3 Sense** with OV2640 camera module
>
> 📡 **XIAO ESP32-C5** is the only XIAO that supports **5GHz WiFi**, offering better performance in congested 2.4GHz environments

## 📝 Communication Protocols

### WiFi Protocol (WebSocket JSON)

**Discovery Message** (Device → HA):
```json
{
  "type": "discovery",
  "entities": [
    {
      "id": "temperature",
      "name": "Temperature",
      "type": "sensor",
      "device_class": "temperature",
      "unit_of_measurement": "°C"
    }
  ]
}
```

**State Update** (Device → HA):
```json
{
  "type": "state",
  "entity_id": "temperature",
  "state": 26.0
}
```

**Control Command** (HA → Device):
```json
{
  "type": "command",
  "entity_id": "led",
  "command": "turn_on"
}
```

### BLE Protocol (BTHome v2)

Uses [BTHome v2](https://bthome.io/) standard protocol, natively supported by Home Assistant for automatic discovery.

**Broadcast Data Format:**
```
[Flags][Service Data: UUID=0xFCD2][Device Info][Sensor Data...]
```

**Manufacturer ID:** `0x5EED` (24301)

---

## ❓ Frequently Asked Questions (FAQ)

### Q1: What's the difference between WiFi and BLE? Which should I use?

| Feature | WiFi | BLE |
|---------|------|-----|
| Communication Direction | Bidirectional (WebSocket) | Bidirectional (Broadcast + GATT) |
| Power Consumption | Higher (~80mA) | Ultra-low (Broadcast <1mA, GATT ~15mA) |
| Transfer Speed | Fast | Slow |
| Connection Distance | Farther (50m+) | Closer (~10m) |
| Suitable Scenarios | Need fast response, high real-time requirements | Battery powered, low power priority |
| Supported Devices | ESP32 only | ESP32 + nRF52840 |

**Recommended Choice:**
- **Choose WiFi**: Need real-time control (like lights, fans), stable power supply
- **Choose BLE**: Battery powered, periodic sensor reporting, low power priority

### Q2: BLE has two working modes?

**Yes!** The BLE library supports two modes:

| Mode | Description | Power | Use Case |
|------|-------------|-------|----------|
| **Passive Broadcast Mode** | Only send data, don't receive commands | Ultra-low (<1mA) | Battery powered sensors |
| **GATT Bidirectional Mode** | Can send data and receive control commands | Lower (~15mA) | Devices needing remote control |

```cpp
// Passive broadcast mode (default)
ble.begin("Device Name");  // Only report data

// GATT bidirectional mode
ble.begin("Device Name", true);  // Second parameter true enables bidirectional communication
ble.addSwitch("led", "LED");  // Can add switches and other controllable entities
```

### Q3: BLE device not discovered by Home Assistant?

1. Make sure Home Assistant has a Bluetooth adapter
2. Or configure [ESP32 Bluetooth Proxy](https://esphome.io/components/bluetooth_proxy.html)
3. BTHome devices will appear automatically, no manual addition needed

### Q4: Is there a limit on the number of sensors?

**No hard-coded limit**. Theoretically only limited by device memory.

### Q5: Can units be customized?

- **WiFi version**: Units are completely defined by the Arduino side, are pure strings
- **BLE version**: Units are defined by the BTHome protocol, automatically matched

### Q6: What device_class are supported?

Refer to [Home Assistant Sensor Documentation](https://www.home-assistant.io/integrations/sensor/#device-class).

### Q7: How to use the camera feature?

**Hardware Requirements:**
- XIAO ESP32-S3 Sense (with OV2640 camera module)

**Software Configuration:**
1. Select board "XIAO_ESP32S3" in Arduino IDE
2. Enable PSRAM: Tools → PSRAM → OPI PSRAM
3. Upload the `CameraStream` example

**Access URLs:**
- Still image: `http://<device_ip>:82/camera`
- MJPEG stream: `http://<device_ip>:82/stream`

**In Home Assistant:**
Once the device is discovered, a camera entity will be automatically added with 4 FPS refresh rate.

### Q8: Multiple devices using the same code, can HA distinguish them?

**Yes!** Home Assistant distinguishes each device by its **unique identifier**:

| Connection Method | Unique Identifier | Example |
|-------------------|-------------------|---------|
| WiFi | MAC Address + mDNS ID | `seeed_ha_a1b2c3` |
| BLE | Bluetooth MAC Address | `0B:76:DD:33:FA:21` |

Even if 10 devices are flashed with exactly the same code, HA will recognize them as 10 independent devices.

⚠️ **But device names will be the same**, which may cause confusion. Suggestions:

**Method 1: Set different names for each device (recommended)**

```cpp
// WiFi devices
ha.setDeviceInfo("TempHumi-Living", "ESP32-C3", "1.0.0");  // Device 1
ha.setDeviceInfo("TempHumi-Bedroom", "ESP32-C3", "1.0.0");  // Device 2

// BLE devices
ble.begin("Sensor-Living");  // Device 1
ble.begin("Sensor-Bedroom");  // Device 2
```

**Method 2: Rename in HA after adding**

Find the device in Home Assistant's **Settings → Devices & Services**, click the device name to modify.

---

## 📄 License

This project uses **dual licensing**:

| Component | License | Description |
|-----------|---------|-------------|
| **Home Assistant Integration** | CC BY-NC-SA 4.0 | Non-commercial use, attribution required, share alike |
| **Arduino Libraries (WiFi/BLE)** | MIT | Free use, including commercial purposes |

### CC BY-NC-SA 4.0 (Integration)

**You are free to:**
- ✅ Share — Copy and redistribute the material in any medium or format
- ✅ Adapt — Remix, transform, and build upon the material

**Under the following terms:**
- 📝 **Attribution** — You must give appropriate credit to the original source
- 🚫 **NonCommercial** — You may not use the material for commercial purposes
- 🔄 **ShareAlike** — If you remix, transform, or build upon the material, you must distribute your contributions under the same license

### MIT (Arduino Libraries)

Arduino libraries use the MIT license, you are free to use, modify and distribute, including commercial purposes.

See [LICENSE](LICENSE) file for details.

---

## 🏢 About Seeed Studio

[Seeed Studio](https://www.seeedstudio.com/) is a company focused on IoT and edge computing, providing various development boards, sensors and modules.

## 🤝 Contributing

Issues and Pull Requests are welcome!

- GitHub: [limengdu/Seeed-Homeassistant-Discovery](https://github.com/limengdu/Seeed-Homeassistant-Discovery)

