# Seeed Home Assistant Discovery BLE (Bluetooth)

[![Version](https://img.shields.io/badge/version-1.6.1-blue.svg)](https://github.com/limengdu/Seeed-Homeassistant-Discovery)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32%20|%20nRF52840-orange.svg)](https://www.espressif.com/)

A lightweight Arduino library for connecting ESP32 and nRF52840 devices to Home Assistant via Bluetooth Low Energy (BLE). Based on BTHome v2 protocol with native HA support.

## ✨ Features

- **BTHome v2 Protocol** - Native Home Assistant support, no custom integration needed for sensors
- **Passive Broadcast** - Low power sensor data advertising
- **GATT Control** - Bidirectional communication for switches and controls
- **HA State Subscription** - Receive Home Assistant entity state changes via BLE
- **Multi-Platform** - Supports ESP32 (NimBLE) and nRF52840 (ArduinoBLE)
- **Low Power** - Ideal for battery-powered devices

## 🔧 Supported Hardware

| Board | BLE Stack | Notes |
|-------|-----------|-------|
| XIAO ESP32-C3 | NimBLE | Compact, WiFi+BLE |
| XIAO ESP32-C5 | NimBLE | Dual-band WiFi + BLE |
| XIAO ESP32-C6 | NimBLE | Thread/Zigbee + BLE |
| XIAO ESP32-S3 | NimBLE | High performance + BLE |
| XIAO nRF52840 | ArduinoBLE | Ultra-low power BLE |
| XIAO nRF52840 Sense | ArduinoBLE | IMU + Microphone + BLE |

## 📦 Installation

### Method 1: Download ZIP

1. Go to [GitHub Repository](https://github.com/limengdu/Seeed-Homeassistant-Discovery)
2. Click **Code → Download ZIP**
3. Extract the ZIP file
4. Copy the `arduino/SeeedHADiscoveryBLE` folder to your Arduino libraries directory:
   - Windows: `Documents/Arduino/libraries/`
   - macOS: `~/Documents/Arduino/libraries/`
   - Linux: `~/Arduino/libraries/`
5. Restart Arduino IDE

### Method 2: Git Clone

```bash
git clone https://github.com/limengdu/Seeed-Homeassistant-Discovery.git
```

Then copy the `arduino/SeeedHADiscoveryBLE` folder to your Arduino libraries directory.

> **Note:** This library is not available in Arduino Library Manager. Please use manual installation.

## 📚 Dependencies

### For ESP32 (XIAO ESP32-C3/C5/C6/S3)

| Library | Version | Purpose |
|---------|---------|---------|
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | ^1.4.0 | BLE stack for ESP32 |

### For nRF52840 (XIAO nRF52840)

| Library | Version | Purpose |
|---------|---------|---------|
| [ArduinoBLE](https://github.com/arduino-libraries/ArduinoBLE) | ^1.3.0 | BLE stack for nRF52840 |

## 🚀 Quick Start

### Basic Sensor Example (BTHome Broadcast)

```cpp
#include <SeeedHADiscoveryBLE.h>

SeeedHADiscoveryBLE ble;
SeeedBLESensor* tempSensor;
SeeedBLESensor* humiSensor;

void setup() {
    Serial.begin(115200);
    
    // Initialize BLE (sensor-only mode)
    ble.begin("Room Sensor");
    
    // Add BTHome sensors
    tempSensor = ble.addTemperature();
    humiSensor = ble.addHumidity();
    
    // Start advertising
    ble.advertise();
}

void loop() {
    ble.loop();
    
    // Update sensor values
    tempSensor->setValue(25.5);  // °C
    humiSensor->setValue(60.0);  // %
    
    // Update advertisement data
    ble.updateAdvertiseData();
    
    delay(5000);
}
```

### Switch Control Example (GATT Bidirectional)

```cpp
#include <SeeedHADiscoveryBLE.h>

SeeedHADiscoveryBLE ble;
SeeedBLESwitch* ledSwitch;

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    
    // Initialize BLE with control enabled (GATT service)
    ble.begin("LED Controller", true);
    
    // Add switch
    ledSwitch = ble.addSwitch("led", "LED Light");
    ledSwitch->onStateChange([](bool state) {
        digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
        Serial.printf("LED: %s\n", state ? "ON" : "OFF");
    });
    
    ble.advertise();
}

void loop() {
    ble.loop();  // Must call to handle GATT events
}
```

### HA State Subscription Example

```cpp
#include <SeeedHADiscoveryBLE.h>

SeeedHADiscoveryBLE ble;

void setup() {
    Serial.begin(115200);
    
    // Subscribe to HA entities (before begin)
    ble.subscribeEntity(0, "sensor.living_room_temperature");
    ble.subscribeEntity(1, "switch.light");
    
    // Register state change callback
    ble.onHAState([](uint8_t index, const char* entityId, const char* state, float value) {
        Serial.printf("HA[%d] %s = %s", index, entityId, state);
        if (value != 0) Serial.printf(" (%.2f)", value);
        Serial.println();
    });
    
    // Initialize with control enabled
    ble.begin("HA Subscriber", true);
    ble.advertise();
}

void loop() {
    ble.loop();
    
    // Access states directly
    SeeedBLEHAState* temp = ble.getHAState(0);
    if (temp && temp->hasValue()) {
        Serial.printf("Temperature: %.1f\n", temp->getFloat());
    }
}
```

## 📂 Examples

| Example | Description | Platform |
|---------|-------------|----------|
| [TemperatureBLE](examples/TemperatureBLE/) | Basic sensor broadcast (BTHome) | ESP32 / nRF52840 |
| [ButtonBLE](examples/ButtonBLE/) | Button events via BLE | ESP32 / nRF52840 |
| [LEDSwitchBLE](examples/LEDSwitchBLE/) | Controllable LED via GATT | ESP32 / nRF52840 |
| [HAStateSubscribeBLE](examples/HAStateSubscribeBLE/) | Subscribe to HA entity states | ESP32 / nRF52840 |
| [XIAO_nRF52840_LowPowerMotionDetect](examples/XIAO_nRF52840_LowPowerMotionDetect/) | Ultra-low power motion detection with IMU wake-up | nRF52840 Sense |
| [XIAO_ESP32_Series_BluetoothProxy](examples/XIAO_ESP32_Series_BluetoothProxy/) | BLE-MQTT Gateway, forward BTHome to MQTT | ESP32-C3/C5/C6/S3 |

## 🔌 API Reference

### SeeedHADiscoveryBLE Class

#### Configuration
```cpp
void setDeviceName(const char* name);
void enableDebug(bool enable = true);
void setAdvertiseInterval(uint32_t intervalMs);
void setTxPower(int8_t power);
```

#### Initialization
```cpp
bool begin(const char* deviceName = "Seeed Sensor");
bool begin(const char* deviceName, bool enableControl);
void stop();
void loop();  // Must call in loop() for GATT
```

#### Sensor Management
```cpp
SeeedBLESensor* addSensor(BTHomeObjectId objectId);
SeeedBLESensor* addTemperature();
SeeedBLESensor* addHumidity();
SeeedBLESensor* addBattery();
SeeedBLESensor* addButton();
```

#### Switch Management
```cpp
SeeedBLESwitch* addSwitch(const char* id, const char* name);
```

#### HA State Subscription
```cpp
SeeedBLEHAState* subscribeEntity(uint8_t entityIndex, const char* entityId);
void onHAState(BLEHAStateCallback callback);
SeeedBLEHAState* getHAState(uint8_t entityIndex);
```

#### Broadcasting
```cpp
void advertise();
void updateAdvertiseData();
```

#### Status
```cpp
bool isRunning() const;
bool isConnected() const;
String getAddress();
```

### SeeedBLESensor Class

```cpp
void setValue(int32_t value);
void setValue(float value);
void setState(bool state);  // For binary sensors
void triggerButton(BTHomeButtonEvent event);  // For button events
```

### SeeedBLESwitch Class

```cpp
void setState(bool state);
void toggle();
bool getState() const;
void onStateChange(BLESwitchCallback callback);
```

## 📡 BTHome Sensor Types

| Sensor | Object ID | Data Type |
|--------|-----------|-----------|
| Temperature | `BTHOME_TEMPERATURE` | 0.01°C precision |
| Humidity | `BTHOME_HUMIDITY` | 0.01% precision |
| Battery | `BTHOME_BATTERY` | 1% precision |
| Pressure | `BTHOME_PRESSURE` | 0.01 hPa precision |
| Illuminance | `BTHOME_ILLUMINANCE` | 0.01 lux precision |
| CO2 | `BTHOME_CO2` | 1 ppm precision |
| PM2.5 | `BTHOME_PM25` | 1 µg/m³ precision |
| Button | `BTHOME_BUTTON` | Event-based |
| Motion | `BTHOME_BINARY_MOTION` | Binary state |
| Occupancy | `BTHOME_BINARY_OCCUPANCY` | Binary state |

## 🏠 Home Assistant Integration

### For Sensors (BTHome)

BTHome devices are natively supported by Home Assistant. Simply:

1. Go to **Settings → Devices & Services**
2. Look for discovered BTHome devices
3. Click **Configure** to add

### For Switches (GATT Control)

Switch control requires the [Seeed HA Discovery](https://github.com/limengdu/Seeed-Homeassistant-Discovery) custom integration:

1. Install HACS in Home Assistant
2. Add custom repository: `https://github.com/limengdu/Seeed-Homeassistant-Discovery`
3. Install "Seeed HA Discovery" integration
4. Restart Home Assistant
5. Go to Settings → Devices & Services → Add Integration → Seeed HA Discovery

## ⚡ Power Consumption

| Mode | Current (typical) |
|------|-------------------|
| Advertising (1s interval) | ~15µA |
| Connected (idle) | ~30µA |
| Transmitting | ~8mA |
| Deep Sleep | ~2µA |

## 🔋 Battery Life Tips

1. **Increase advertising interval** for longer battery life:
   ```cpp
   ble.setAdvertiseInterval(10000);  // 10 seconds
   ```

2. **Use sensor-only mode** if you don't need control:
   ```cpp
   ble.begin("Sensor");  // No GATT service
   ```

3. **Reduce TX power** if range is sufficient:
   ```cpp
   ble.setTxPower(-8);  // Lower power
   ```

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📧 Support

- GitHub Issues: [Report a bug](https://github.com/limengdu/Seeed-Homeassistant-Discovery/issues)
- Seeed Forum: [Community Support](https://forum.seeedstudio.com/)

