# Low Power Motion Detection BLE Example

Ultra-low power motion detection device using XIAO nRF52840 with BTHome protocol for Home Assistant integration.

## Features

- Motion detection via LSM6DS3 IMU Z-axis acceleration
- BTHome v2 protocol for automatic discovery
- Ultra-low power System OFF deep sleep (< 5 µA)
- Automatic wake-up on motion detection
- LED status indicators

## Hardware Requirements

- Seeed XIAO nRF52840 (with built-in LSM6DS3 IMU)
- USB-C cable (for programming)

## Software Dependencies

### Arduino IDE Setup

1. **Board Manager**: Add Seeed nRF52 Boards
   - URL: `https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json`
   - Install: **Seeed nRF52 Boards (Adafruit BSP)**

2. **Required Libraries**:
   - `Seeed Arduino LSM6DS3` - IMU driver
   - `Adafruit Bluefruit nRF52` - BLE support (included with board package)

### Board Selection

```
Tools → Board → Seeed nRF52 Boards → Seeed XIAO nRF52840
```

## How It Works

### State Machine

```
┌─────────────────────────────────────────────────────────────┐
│                     Startup / Wake-up                       │
└─────────────────────┬───────────────────────────────────────┘
                      ▼
┌─────────────────────────────────────────────────────────────┐
│  Broadcast motion=1 (Motion Detected)                       │
│  - Refresh advertising every 100ms                          │
│  - Green LED flash                                          │
└─────────────────────┬───────────────────────────────────────┘
                      │ Still for 6 seconds
                      ▼
┌─────────────────────────────────────────────────────────────┐
│  Broadcast motion=0 (Motion Cleared)                        │
│  - Refresh advertising every 100ms                          │
│  - Duration: 5 seconds                                      │
└─────────────────────┬───────────────────────────────────────┘
                      │ After 5 seconds
                      ▼
┌─────────────────────────────────────────────────────────────┐
│  Deep Sleep (System OFF)                                    │
│  - Blue LED blinks 3 times before sleep                     │
│  - Power consumption < 5 µA                                 │
│  - IMU interrupt wake-up                                    │
└─────────────────────────────────────────────────────────────┘
```

### BTHome Data Format

| Field | Value | Description |
|-------|-------|-------------|
| UUID | 0xFCD2 | BTHome Service UUID |
| Device Info | 0x44 | BTHome v2, trigger-based |
| Object ID | 0x21 | Binary Motion |
| Value | 0x01/0x00 | Detected/Clear |

## LED Indicators

| LED | Pattern | Meaning |
|-----|---------|---------|
| Green | 2 fast blinks | Woke from sleep |
| Green | 3 slow blinks | Normal power-on |
| Green | Short flash | Motion detected |
| Blue | 3 blinks | Entering sleep |
| Blue | Continuous | IMU initialization failed |

## Home Assistant Integration

### Auto Discovery

The device is automatically discovered via BTHome integration:

1. Go to **Settings → Devices & Services → Integrations**
2. Find **BTHome** integration
3. Click **Configure** to add the device
4. Device name format: `SeeedUA-XXXX` (last 4 digits of MAC)

### Using ESP32 Bluetooth Proxy

If the XIAO is far from Home Assistant, use an ESP32 as a Bluetooth proxy:

1. Install ESPHome on an ESP32
2. Configure Bluetooth proxy:

```yaml
bluetooth_proxy:
  active: true
```

### Created Entities

The device creates one binary sensor:

| Entity | Type | Device Class |
|--------|------|--------------|
| `binary_sensor.seeedua_xxxx_motion` | Binary Sensor | motion |

States: `Detected` / `Clear`

## Configuration Options

Adjustable parameters in the code:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `ACCEL_THRESHOLD_Z` | 1.30 | Z-axis acceleration threshold (g) |
| `DEBOUNCE_TIME` | 6000 | Motion→Still debounce time (ms) |
| `CLEAR_DURATION` | 5000 | Duration to broadcast clear before sleep (ms) |
| `ADVERTISE_INTERVAL` | 100 | Advertising refresh interval (ms) |

## Serial Output Example

```
========================================
  XIAO nRF52840 BTHome Motion Detect
========================================

Reset reason: 0x10000
>>> Woke from sleep (motion triggered) <<<

MAC Address: C1:F9:3E:1D:93:7C
Device Name: SeeedUA-937C

IMU initialization successful!
Motion wake! Starting BTHome advertising...
..... OK

========================================
  Initialization Complete!
========================================

Motion: broadcast motion=1
Still 6s: switch to motion=0
Still 11s: enter deep sleep

IMPORTANT: Disconnect USB to test power consumption!

>>> Motion=1
>>> Motion=0, sleep in 5s
>>> Entering sleep

>>> Entering deep sleep <<<
Waiting 3 seconds before sleep...
```

## Power Consumption

**IMPORTANT**: Disconnect USB to measure actual power consumption!

| State | Power |
|-------|-------|
| Deep Sleep (System OFF) | < 5 µA |
| Advertising | ~3 mA |

## Troubleshooting

### IMU Initialization Failed
- Blue LED continuously blinks
- Check I2C connections
- Verify board selection is correct

### Home Assistant Not Receiving State
- Ensure BTHome integration is installed
- Check if Bluetooth proxy is working
- View HA logs for Bluetooth-related messages

### Device Not Entering Sleep
- Place device on a stable, level surface
- Adjust Z-axis threshold if needed
- Check serial output for state transitions

### Device Not Waking Up
- Verify IMU interrupt is configured correctly
- Check if motion is sufficient to trigger wake-up
- Ensure device was properly in sleep mode

## License

Part of the SeeedHADiscoveryBLE library. MIT License.
