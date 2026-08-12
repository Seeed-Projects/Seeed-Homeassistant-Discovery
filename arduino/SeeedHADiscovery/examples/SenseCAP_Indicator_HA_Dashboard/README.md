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

Install these items with Arduino Boards Manager and Library Manager. The FT5x06
touch driver is included in this example.

## Board settings

Select `ESP32S3 Dev Module` and use these options:

- Flash Size: `8MB (64Mb)`
- PSRAM: `OPI PSRAM`
- USB CDC On Boot: `Enabled`

## Build with Arduino CLI

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi,CDCOnBoot=cdc" \
  arduino/SeeedHADiscovery/examples/SenseCAP_Indicator_HA_Dashboard
```

After uploading, the screen shows a dark meeting-room dashboard with four
metric cards and a touch test card. Values remain `--` until the Home Assistant
stage supplies entity states. Tap the bottom card: the tap counter increments,
the title changes to `Touch confirmed`, and the serial monitor prints
`Touch test count: 1`. At 115200 baud, successful startup ends with
`LVGL dashboard ready`.

The first touch also prints its raw and mapped coordinates. For example:

```text
Touch pressed: raw=(292, 85), mapped=(187, 394)
```

The touch driver limits I2C wait time and automatically resets the controller
after repeated read failures. A successful recovery prints
`Touch controller recovered`.

## Scope

This stage establishes reusable Arduino display, LVGL UI, and touch input
modules. Wi-Fi provisioning, Home Assistant state delivery, entity selection,
and controllable-entity actions will be added in later stages.

## Reference

- [SenseCAP Indicator ESP32 development with Arduino](https://wiki.seeedstudio.com/SenseCAP_Indicator_ESP32_Arduino/)
