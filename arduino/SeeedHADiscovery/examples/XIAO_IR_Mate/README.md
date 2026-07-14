# IR Mate Universal Infrared Remote

This example lets IR Mate control a Gree air conditioner offline and optionally connect to Home Assistant to learn and send other remote-control commands.

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

The device uses a Gree protocol compatible with YAN-series remotes. Its current air-conditioner state is stored in ESP32 NVS and remains available after a restart.

## Optional Home Assistant setup

1. Connect to the `Seeed_IR_Mate` access point and configure Wi-Fi.
2. Add the discovered `IR Mate` device in Home Assistant.
3. Learn a command with `remote.learn_command`.
4. Replay it with `remote.send_command`.

The infrared receiver is enabled only after Home Assistant starts learning. It is disabled again when learning succeeds, is cancelled, or times out.

Example actions:

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

Learned commands are stored by Home Assistant and remain available after a restart. Offline touch control uses the Gree air-conditioner state stored on the device and is independent of Home Assistant learning.
