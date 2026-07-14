# IR Mate Universal Infrared Remote

This example lets IR Mate control a Gree air conditioner offline and connect to Home Assistant to manage appliances, learn infrared commands, and configure touch actions.

Home Assistant 2026.7.0 or later is required.

## Pins

| Function | GPIO |
|---|---:|
| Infrared transmitter | 3 |
| Infrared receiver | 4 |
| Touch button | 5 |
| Vibration motor | 6 |
| RGB status LED | 7 |
| Provisioning reset | 9 |

## Dependencies

- Seeed Home Assistant Discovery
- IRremoteESP8266 2.9.0+
- Adafruit NeoPixel 1.15.2+

The included `build_opt.h` enables Gree air-conditioner transmission and raw infrared send and capture support.

## Offline use

After flashing `XIAO_IR_Mate.ino`, the touch button controls a Gree air conditioner without Wi-Fi:

- Single touch: toggle power with cooling mode and a 25°C default.
- Double touch: increase the temperature by 1°C.
- Triple touch: decrease the temperature by 1°C.
- Long press: cycle the air-conditioner mode.

The device uses a Gree protocol compatible with YAN-series remotes. Its current air-conditioner state and four touch bindings are stored in ESP32 NVS and remain available while offline and after a restart.

## Optional Home Assistant setup

1. Connect to the `Seeed_IR_Mate` access point and configure Wi-Fi.
2. Add the discovered `IR Mate` device in Home Assistant.
3. Open `Settings → Devices & services → Seeed HA Discovery`, find the IR Mate config entry, and select Configure.
4. Select a device type, brand, and model, then add the appliance.
5. Select Learn for each command that requires capture and follow the on-screen prompt.
6. Assign single, double, triple, and long-press actions, then select Save and sync.

The infrared receiver is enabled only after Home Assistant starts learning. It is disabled again when learning succeeds, is cancelled, or times out.

The Gree YAN / YAW1F profile uses the firmware's built-in codes and is ready to test or bind. Other brand profiles provide common command slots that can be tested, relearned, deleted, or bound after learning. The brand and model catalog is stored in the Home Assistant integration's `ir_profiles.json` file and can be extended.

Home Assistant stores the complete learned waveforms in persistent storage. Save and sync copies the four selected touch actions to IR Mate NVS, so the device continues using the last synchronized configuration while offline.

## Status feedback

- Successful infrared transmission or learning: green light and one short vibration.
- Failed infrared transmission or learning: red light and two short vibrations.
- Provisioning, disconnected, or not yet paired with Home Assistant: blinking blue light.
- Connected to Home Assistant and idle: status light off.

Automations can also use the standard `remote` services. The remote entity is hidden by default and can be enabled in the entity registry:

```yaml
action: remote.learn_command
target:
  entity_id: remote.ir_mate_universal_remote
data:
  device: air_conditioner
  command: power
  command_type: ir
  timeout: 30
```

```yaml
action: remote.send_command
target:
  entity_id: remote.ir_mate_universal_remote
data:
  device: air_conditioner
  command: power
```
