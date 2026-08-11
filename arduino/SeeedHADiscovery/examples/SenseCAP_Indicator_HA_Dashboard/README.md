# SenseCAP Indicator HA Dashboard

This Arduino example is the first hardware bring-up stage for a generic
SenseCAP Indicator Home Assistant dashboard. It initializes the 480 x 480
ST7701 display and shows a static verification screen.

## Hardware

- SenseCAP Indicator D1 or D1S
- USB-C data cable connected to the native USB port

## Arduino dependencies

- ESP32 board package 3.1.2
- GFX Library for Arduino 1.5.3
- PCA95x5 0.1.3

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

After uploading, the screen shows `SenseCAP Indicator`, `Display Ready`, the
panel resolution, and four color blocks. The serial monitor prints
`Display test screen ready` at 115200 baud.

## Scope

This stage establishes the reusable Arduino display foundation. Wi-Fi
provisioning, Home Assistant state delivery, entity rendering, touch input,
and device control will be added in later stages.

## Reference

- [SenseCAP Indicator ESP32 development with Arduino](https://wiki.seeedstudio.com/SenseCAP_Indicator_ESP32_Arduino/)
