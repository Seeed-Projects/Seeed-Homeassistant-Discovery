# reTerminal E1002 HA Display Dashboard

Display Home Assistant entity states on the reTerminal E1002's 6-color E-Paper display. Creates a colorful dashboard with automatic updates.

## Features

- **Web-based WiFi Provisioning (Captive Portal)** - No hardcoded WiFi credentials needed
- Subscribe to any Home Assistant entities
- 6-color E-Paper display (800x480)
- Beautiful colorful dashboard UI
- Color-coded entity cards by device class
- Up to 6 entity cards
- Automatic display refresh
- Connection status indicator
- Smart refresh logic (avoids unnecessary updates)
- **GPIO3 Reset Button** - Long press 6s to clear WiFi credentials
- **GPIO6 Status LED** - Visual feedback indicator
- **GPIO45 Buzzer** - Audio feedback for WiFi reset

## Hardware Requirements

- **reTerminal E1002** with 6-color E-Paper display
- Display resolution: 800x480
- GPIO3: Reset button (long press 6s to reset WiFi)
- GPIO6: Status LED (active LOW)
- GPIO45: Buzzer (audio feedback)

## Supported Colors

6 colors available:

| Color Constant | Description |
|---------------|-------------|
| `TFT_WHITE` | White |
| `TFT_BLACK` | Black |
| `TFT_RED` | Red |
| `TFT_GREEN` | Green |
| `TFT_BLUE` | Blue |
| `TFT_YELLOW` | Yellow |

## Software Dependencies

### Required Libraries

| Library | Source | Description |
|---------|--------|-------------|
| **Seeed_GFX** | [GitHub](https://github.com/Seeed-Studio/Seeed_GFX) | E-Paper graphics |
| **ArduinoJson** | Library Manager | JSON parsing |
| **WebSockets** | Library Manager | WebSocket communication |

### SeeedHADiscovery Library

Install manually from [GitHub](https://github.com/limengdu/SeeedHADiscovery).

## Quick Start

### 1. Upload Firmware

1. Select board for reTerminal E1002
2. Ensure `EPAPER_ENABLE` is defined in User_Setup.h
3. Upload the sketch

### 2. WiFi Provisioning

On first boot (or after reset), the device enters provisioning mode:

1. E-Paper displays colorful provisioning screen with instructions
2. Connect your phone/computer to WiFi: **reTerminal_E1002_AP**
3. Open browser and visit: **http://192.168.4.1**
4. Select your WiFi network and enter password
5. Device auto-connects and displays IP address

> **Reset WiFi**: Long press GPIO3 button for 6 seconds, buzzer alarm sounds, release to reset

### 3. Configure in Home Assistant

1. Find device in **Settings** → **Devices & Services**
2. Click **Configure**
3. Select entities to subscribe (max 6)
4. Save - display will update automatically

### Alternative: Hardcoded WiFi (Optional)

If you prefer not to use provisioning, modify the code:

```cpp
#define USE_WIFI_PROVISIONING false  // Disable provisioning

const char* WIFI_SSID = "your-wifi-ssid";
const char* WIFI_PASSWORD = "your-wifi-password";
```

## Color Coding by Device Class

| Device Class | Card Color |
|-------------|------------|
| temperature | Red |
| humidity | Blue |
| battery | Green |
| illuminance | Yellow |
| power/energy | Green |
| other | Black |

## Display Layout

```
┌─────────────────────────────────────────────────────┐
│  [Blue Header] Home Assistant Dashboard  [Status]   │
├───────────────┬───────────────┬───────────────────┬─┤
│ ▌ Card 1     │ ▌ Card 2     │ ▌ Card 3          │ │
│ ▌ [Value]    │ ▌ [Value]    │ ▌ [Value]         │ │
├───────────────┼───────────────┼───────────────────┼─┤
│ ▌ Card 4     │ ▌ Card 5     │ ▌ Card 6          │ │
│ ▌ [Value]    │ ▌ [Value]    │ ▌ [Empty]         │ │
├───────────────┴───────────────┴───────────────────┴─┤
│  Device ID | IP Address          | Last Refresh     │
└─────────────────────────────────────────────────────┘
```

(▌ = colored accent bar on each card)

## Refresh Logic

| Trigger | Condition |
|---------|-----------|
| Initial refresh | After first data collection (5s wait) |
| Config change | When entities are added/removed in HA |
| Periodic refresh | Every 5 minutes |
| Connection change | When HA connects/disconnects |

## Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `USE_WIFI_PROVISIONING` | true | Enable web-based WiFi provisioning |
| `AP_SSID` | reTerminal_E1002_AP | AP hotspot name for provisioning |
| `PIN_RESET_BUTTON` | 3 | Reset button pin |
| `PIN_STATUS_LED` | 6 | Status LED pin |
| `PIN_BUZZER` | 45 | Buzzer pin |
| `DISPLAY_REFRESH_INTERVAL` | 300000ms | Periodic refresh (5 min) |
| `DATA_COLLECTION_WAIT` | 5000ms | Wait time before initial refresh |
| `MAX_DISPLAY_ENTITIES` | 6 | Maximum entities shown |

## LED & Buzzer Status Indicators

| Status | LED Behavior | Buzzer Feedback |
|--------|--------------|-----------------|
| Boot | Quick flash 2 times | - |
| Enter provisioning mode | Slow flash 3 times | - |
| WiFi connected | Quick flash 3-5 times | - |
| Reset button held 6s | Quick flash + stays on | Alarm sound (1500/1000Hz x3) + beep |
| WiFi reset triggered | LED off | Long beep (800Hz, 500ms) |

## Serial Output

Debug output via Serial1 (pins 43/44):
- Connection status
- Entity updates
- Refresh triggers

## Startup Screen

Shows colorful startup screen with:
- HA logo
- Device name
- Connection status
- IP address
- Color palette bars at bottom

## Troubleshooting

### Display not updating
- Check `EPAPER_ENABLE` is defined
- Verify entities are subscribed in HA
- Check Serial1 output for errors

### "EPAPER_ENABLE not defined"
- Enable E-Paper in User_Setup.h of TFT_eSPI/Seeed_GFX

### Colors look wrong
- 6-color E-Paper has limited color palette
- Only use the 6 supported colors

## License

Part of the SeeedHADiscovery library.

