# IR Mate Universal Infrared Remote

This example connects IR Mate to Home Assistant with infrared transmit, receive, and command learning support.

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

The included `build_opt.h` enables only raw infrared send and capture support, keeping the firmware compact.

## Usage

1. Flash `IRMateUniversalRemote.ino`.
2. Connect to the `Seeed_IR_Mate` access point and configure Wi-Fi.
3. Add the discovered `IR Mate` device in Home Assistant.
4. Learn a command with `remote.learn_command`.
5. Replay it with `remote.send_command`.

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

Learned commands are stored by Home Assistant and remain available after a restart.

The physical touch button replays the most recently received signal. The signal is stored in memory and must be learned again after a restart.
