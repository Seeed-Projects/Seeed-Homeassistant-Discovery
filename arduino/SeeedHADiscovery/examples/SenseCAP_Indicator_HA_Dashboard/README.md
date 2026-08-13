# SenseCAP Indicator HA Dashboard

This Arduino example provides the display and touch foundation for a generic
SenseCAP Indicator Home Assistant dashboard. It initializes the 480 x 480
ST7701 display, runs a polished LVGL meeting-room dashboard, and reads the
FT5x06 touch controller.

## Hardware

- SenseCAP Indicator D1 or D1S
- USB-C data cable connected to the native USB port

## Arduino dependencies

- ESP32 board package 3.1.2
- GFX Library for Arduino 1.5.3
- PCA95x5 0.1.3
- LVGL 9.2.2
- Seeed Home Assistant Discovery 1.6.1
- ArduinoJson 7.2.1
- WebSockets 2.7.1

Install these items with Arduino Boards Manager and Library Manager. Installing
Seeed Home Assistant Discovery also installs ArduinoJson and WebSockets. The
FT5x06 touch driver is included in this example.

## Board settings

Select `ESP32S3 Dev Module` and use these options:

- Flash Size: `8MB (64Mb)`
- Partition Scheme: `8M with spiffs (3MB APP/1.5MB SPIFFS)`
- PSRAM: `OPI PSRAM`
- USB CDC On Boot: `Enabled`

Select the 3MB application partition shown above. Arduino IDE's default
`Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)` provides only about 1.25MB
for the application, which cannot contain LVGL, Wi-Fi provisioning, and Home
Assistant communication together.

## Build with Arduino CLI

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi,CDCOnBoot=cdc" \
  arduino/SeeedHADiscovery/examples/SenseCAP_Indicator_HA_Dashboard
```

After uploading, the screen shows the Room 106 meeting-room dashboard. The
Overview page presents occupancy, Home Assistant status, CO2, temperature,
humidity, motion-sensor battery, electric-window state, television power, and
monthly energy. The Energy page also presents today's energy and live TV power.

The bottom navigation opens Overview, Controls, and Energy pages. The Controls
page provides left-switch, right-switch, electric-window, and TV cards. These
cards request Home Assistant to toggle their mapped entities. The air-conditioner
card is a reserved, non-interactive slot until the room device is installed.
After confirmation, `Leave Room` requests Home Assistant to turn off six
configured room devices, including the reserved air-conditioner entity. Control
state labels only change after Home Assistant sends the real state.

## Wi-Fi provisioning

On first boot, or when no saved Wi-Fi credentials exist, the screen displays
`WiFi Setup`:

1. Connect a phone or computer to `SenseCAP_Indicator_AP`.
2. Open `http://192.168.4.1`; most phones also open the captive portal.
3. Select the target Wi-Fi network, enter its password, and save.
4. The device restarts and connects automatically. The header progresses from
   `HA Waiting` to `HA Online`.

The device stores the Wi-Fi credentials and reconnects after normal restarts.

The firmware lets LVGL finish its first frame before provisioning starts. Wi-Fi
connection and hotspot scanning run on the ESP32-S3's other CPU core so display
refresh and touch handling continue to run.
The example's RGB display wrapper lets the display controller read its PSRAM
framebuffer directly, so Wi-Fi credential reads and writes can run while the
dashboard remains visible.

## Home Assistant subscriptions

After adding the device to the Seeed HA Discovery integration, select these
entities on its Home Assistant configuration page:

| Dashboard value | Home Assistant entity |
| --- | --- |
| Occupancy | `sensor.xiaomi_03_1163_occupancy_sensor_2` |
| Motion battery | `sensor.xiaomi_03_1163_battery_level_2` |
| CO2 | `sensor.scd41_air_quality_monitor_carbon_dioxide` |
| Temperature | `sensor.scd41_air_quality_monitor_temperature` |
| Humidity | `sensor.scd41_air_quality_monitor_humidity` |
| Left room switch | `switch.xiaomi_2wpro2_37c3_left_switch_service_2` |
| Right room switch | `switch.xiaomi_2wpro2_37c3_right_switch_service_2` |
| Electric window | `cover.liyan_liyan_4d07_window_opener_2` |
| TV power | `switch.cuco_v3_3244_switch_2` |
| Live TV power | `sensor.cuco_v3_3244_electric_power_2` |
| Today's energy | `sensor.cuco_v3_3244_power_cost_today_2` |
| Monthly energy | `sensor.cuco_v3_3244_power_cost_month_2` |

Window, TV, and `Leave Room` controls use the same entity selection as their
authorization boundary. Also select these entities:

| Control target | Home Assistant entity |
| --- | --- |
| Left room switch | `switch.xiaomi_2wpro2_37c3_left_switch_service_2` |
| Right room switch | `switch.xiaomi_2wpro2_37c3_right_switch_service_2` |
| Reserved air conditioner | `switch.indicator_switch1_1` |
| TV power | `switch.cuco_v3_3244_switch_2` |
| Room light | `light.liyan_liyan_4d07_light_2` |
| Room media player | `media_player.xiaomi_lx06_3740_play_control_2` |

The left switch, right switch, window, and TV entities already appear in the
display table. Select the reserved air-conditioner entity even before the
physical air conditioner is installed because it is part of the `Leave Room`
batch command. The integration only executes device actions for entities
selected on this page. Temperature and humidity are rounded to one decimal
place on the display. Presence states such as `has one` and `no one` are
displayed as `Occupied` and `Vacant`.

To reuse the firmware in another room, edit the room name, provisioning AP name,
and entity IDs in `RoomDashboardConfig.h`. The display driver, UI, and HA state
handling remain unchanged.

At 115200 baud, each mapped update prints a message such as:

```text
Dashboard state queued: sensor.scd41_air_quality_monitor_temperature = 24.6
```

The first touch also prints its raw and mapped coordinates. For example:

```text
Touch pressed: raw=(292, 85), mapped=(187, 394)
```

The touch driver limits I2C wait time and automatically resets the controller
after repeated read failures. A successful recovery prints
`Touch controller recovered`.

## Scope

The example includes Wi-Fi provisioning, HA connection status, live Room 106
entity state display, left-switch, right-switch, window, and TV touch control,
an air-conditioner placeholder, and the confirmed `Leave Room` batch turn-off
action.

## Reference

- [SenseCAP Indicator ESP32 development with Arduino](https://wiki.seeedstudio.com/SenseCAP_Indicator_ESP32_Arduino/)
