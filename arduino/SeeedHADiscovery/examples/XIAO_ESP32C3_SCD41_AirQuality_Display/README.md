# XIAO ESP32-C3 SCD41 Air Quality Display Example

This example reads carbon dioxide, temperature, and humidity from a Grove SCD41 with a XIAO ESP32-C3, presents the measurements on a 1.47-inch 172×320 ST7789 SPI LCD in landscape orientation, and connects the sensors to Home Assistant through SeeedHADiscovery.

The firmware follows the project's standard web-provisioning flow. On first boot it creates an access point and configuration portal, saves the WiFi credentials, and reconnects automatically on later boots. Hold the XIAO BOOT button for 6 seconds while the firmware is running to start provisioning again.

## Features

- Reads a new SCD41 measurement about every 5 seconds
- Displays CO₂, temperature, and relative humidity
- Changes the air-quality label and color with the CO₂ level
- Refreshes only the changing UI regions
- Shows sensor warm-up and wiring-error states
- Retries sensor initialization every 5 seconds after a connection failure
- Creates the `Seeed_AirMonitor_AP` hotspot on first boot
- Saves WiFi credentials and reconnects automatically
- Supports WiFi reconfiguration by holding BOOT for 6 seconds
- Publishes CO₂, temperature, and humidity entities to Home Assistant
- Shows provisioning, WiFi, and Home Assistant connection status on the LCD

## Hardware

- Seeed Studio XIAO ESP32-C3
- 1.47 inch LCD SPI Display, 172×320, ST7789V3
- Grove CO₂ & Temperature & Humidity Sensor (SCD41)
- Grove-to-Dupont cable or equivalent wiring

## Wiring

### LCD

| LCD pin | XIAO ESP32-C3 | Function |
|---|---|---|
| VCC | 3V3 | Power |
| GND | GND | Ground |
| DIN | D10 | SPI MOSI |
| CLK | D8 | SPI clock |
| CS | D1 | Chip select |
| DC | D3 | Data/command select |
| RST | D0 | Display reset |
| BL | D6 | Backlight control |

### SCD41

| SCD41 pin | XIAO ESP32-C3 | Function |
|---|---|---|
| VCC | 3V3 | Power |
| GND | GND | Ground |
| SDA | D4 | I²C data |
| SCL | D5 | I²C clock |

## Software Dependencies

Install these libraries from the Arduino IDE Library Manager:

| Library | Verified version |
|---|---|
| Adafruit GFX Library | 1.12.6 |
| Adafruit ST7735 and ST7789 Library | 1.11.0 |
| Sensirion I2C SCD4x | 1.1.0 |
| Sensirion Core | 0.7.3 |
| ArduinoJson | 7.4.2 |
| WebSockets | 2.7.1 |

Use the `SeeedHADiscovery` version in this repository at `arduino/SeeedHADiscovery`.

The verified board package is `esp32 by Espressif Systems` 3.3.10.

## Upload and Run

1. Connect the LCD and SCD41.
2. Select `XIAO_ESP32C3` in Arduino IDE.
3. Open `XIAO_ESP32C3_SCD41_AirQuality_Display.ino`.
4. Select the XIAO serial port and upload the sketch.
5. Open Serial Monitor at `115200` baud.
6. On first boot, connect a phone or computer to `Seeed_AirMonitor_AP`.
7. Open `http://192.168.4.1` and enter the credentials for a 2.4 GHz WiFi network.
8. The device saves the configuration, restarts, and connects automatically.

You can also run this compile check from the repository root:

```bash
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 arduino/SeeedHADiscovery/examples/XIAO_ESP32C3_SCD41_AirQuality_Display
```

You can also select the `SCD41 Air Quality Display` card in the project's [Web Flasher](https://seeed-projects.github.io/Seeed-Homeassistant-Discovery/flasher/) to install the prebuilt firmware.

## Connect to Home Assistant

1. Confirm that the device and Home Assistant are on the same local network.
2. Open `Settings → Devices & services → Add integration` in Home Assistant.
3. Search for and select `Seeed HA Discovery`.
4. Enter the device IP address shown in Serial Monitor.
5. The device page exposes these three entities:

| Entity | Unit | Description |
|---|---|---|
| Carbon Dioxide | ppm | SCD41 carbon-dioxide concentration |
| Temperature | °C | SCD41 temperature |
| Humidity | % | SCD41 relative humidity |

The top-right badge shows the connection state:

| Badge | Meaning |
|---|---|
| `SETUP` | The provisioning access point and portal are active |
| `OFFLINE` | WiFi is not connected |
| `WIFI` | WiFi is connected and Home Assistant is not connected yet |
| `HA ONLINE` | Home Assistant is connected |

## Expected Behavior

The display progresses through:

1. `INITIALIZING SCD41`
2. `WARMING UP`
3. The main CO₂ value, air-quality level, temperature, and humidity

Serial Monitor shows output similar to:

```text
Air quality display starting
SCD41 serial number: 0x...
SCD41 periodic measurement started
CO2: 650 ppm, Temperature: 24.3 C, Humidity: 48.1 %
```

During first-time provisioning, Serial Monitor also shows:

```text
WiFi provisioning is active
Connect to access point: Seeed_AirMonitor_AP
Open: http://192.168.4.1
```

The first periodic SCD41 measurement normally arrives after about 5 seconds. Run the sensor for about two hours before evaluating measurement stability and accuracy.

## UI Levels

| CO₂ concentration | Label | Color |
|---|---|---|
| ≤ 800 ppm | GOOD | Green |
| 801–1000 ppm | FAIR | Yellow |
| 1001–1500 ppm | POOR | Orange |
| > 1500 ppm | BAD | Red |

These thresholds control the example UI and can be adjusted in `classifyAirQuality()` for the intended environment.

## Verification

### Main flow

1. Start the device in a normal indoor environment.
2. Wait for the display to change from `WARMING UP` to live values.
3. Confirm that the screen values match Serial Monitor.
4. Keep the device running and confirm that updates occur about every 5 seconds without a full-screen flash.
5. Complete hotspot provisioning and confirm that the badge changes from `SETUP` to `WIFI`.
6. Add the device in Home Assistant and confirm that the badge changes to `HA ONLINE` and all three entities match the LCD values.

### Edge cases

- Disconnect the SCD41: the screen shows `SENSOR ERROR` and Serial Monitor reports the I²C error.
- Reconnect the SCD41: the device retries every 5 seconds, then returns to `WARMING UP` and live values.
- Move across a CO₂ threshold: the label and color change together.
- Reach a four-digit CO₂ value: the number remains inside the main card.
- Make WiFi unavailable: local sensor readings continue and the badge shows `OFFLINE`.
- Leave Home Assistant disconnected: WiFi stays connected and the badge shows `WIFI`.
- Change WiFi networks: hold BOOT for 6 seconds while the firmware is running, then reconnect to `Seeed_AirMonitor_AP` after restart.

## References

- [1.47 inch LCD SPI Display Wiki](https://wiki.seeedstudio.com/1-47inch_lcd_spi_display/)
- [Grove SCD41 Wiki](https://wiki.seeedstudio.com/Grove-CO2_%26_Temperature_%26_Humidity_Sensor-SCD41/)
- [Sensirion Arduino SCD4x Library](https://github.com/Sensirion/arduino-i2c-scd4x)
