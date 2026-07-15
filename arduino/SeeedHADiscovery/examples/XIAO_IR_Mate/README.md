# IR Mate Universal Infrared Remote

This example lets IR Mate control a Gree air conditioner offline and connect to Home Assistant to control air conditioners from a bundled code library, learn infrared commands, and configure touch actions from the device page.

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

The brand list is sorted by brand and model so the catalog is easy to scan. Once an appliance is added, its controls appear directly on the IR Mate device page, grouped into sections:

- **Controls**: the **Appliance** and **Command** dropdowns pick the active appliance and send any of its commands, and air conditioners from the bundled code library get a full climate card with power, mode, temperature, fan, and swing.
- **Configuration**: the **Single / Double / Triple / Long press** dropdowns bind a command to each touch gesture and sync the choice to the device, and the **Learn single / double / triple / long press** buttons capture a signal straight into that gesture slot for uncommon remotes.
- **Diagnostic**: the **Last learned IR signal** and **Last transmitted IR signal** sensors report the pulse count and expose the full waveform as attributes.

The infrared receiver is enabled only while learning is in progress. It is disabled again when learning succeeds, is cancelled, or times out.

## Verifying a learned signal

Learning is fully observable end to end:

1. Press a **Learn** button and press the remote's key while pointing it at IR Mate. A notification reports success with the pulse count and a waveform preview, or explains that no signal was captured.
2. Open the **Last learned IR signal** sensor to see the captured pulse count and the full `timings` array in its attributes. The same array prints on the device serial console as `IR learn OK`.
3. Select **Command** (or the gesture's bound command) to transmit. The **Last transmitted IR signal** sensor and the serial `IR TX` line show the emitted waveform.
4. The learned and transmitted `timings` match pulse for pulse, and the target appliance responding confirms the capture is correct.

## Air-conditioner code library

The integration ships with a large air-conditioner code library covering 342 brand and model profiles. Selecting a model in the climate card looks up the correct waveform for the current mode, fan speed, and temperature and transmits it, so most air conditioners work without any learning. For remotes outside the library, use the learn buttons or the remote learning service as a fallback.

The Gree YAN / YAW1F profile uses the firmware's built-in codes and drives the offline touch defaults. The brand and model catalog for learnable appliances is stored in the Home Assistant integration's `ir_profiles.json` file and can be extended.

Home Assistant stores the complete learned waveforms and library selections in persistent storage. Binding a gesture copies the selected touch action to IR Mate NVS, so the device continues using the last synchronized configuration while offline.

The bundled air-conditioner codes are derived from the MIT-licensed [SmartIR](https://github.com/litinoveweedle/SmartIR) project; see `custom_components/seeed_ha_discovery/codes/LICENSE`.

## Status feedback

- Successful infrared transmission or learning: green light and one short vibration.
- Failed infrared transmission or learning: red light and two short vibrations.
- Provisioning, disconnected, or not yet paired with Home Assistant: blinking blue light.
- Connected to Home Assistant and idle: status light off.

Automations can also use the standard `remote` services on the `Universal remote` entity, which is enabled by default:

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
