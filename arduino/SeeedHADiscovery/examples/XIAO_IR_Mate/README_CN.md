# IR Mate 万能红外遥控器

该示例让 IR Mate 在离线状态下直接控制格力空调，并可选接入 Home Assistant 学习和发送其他遥控器按键。

需要 Home Assistant 2026.7.0 或更高版本。

## 引脚

| 功能 | GPIO |
|---|---:|
| 红外发射 | 3 |
| 红外接收 | 4 |
| 触摸按键 | 5 |
| 震动马达 | 6 |
| RGB 状态灯 | 7 |
| 配网重置 | 9 |

## 依赖

- Seeed Home Assistant Discovery
- IRremoteESP8266 2.9.0+
- Adafruit NeoPixel 1.15.2+

示例中的 `build_opt.h` 启用格力空调发射和原始红外波形收发能力。

## 离线使用

烧录 `XIAO_IR_Mate.ino` 后无需配置 Wi-Fi，触摸按键可以直接控制格力空调：

- 单击：切换电源，默认使用制冷模式和 25℃。
- 双击：升高 1℃。
- 三击：降低 1℃。

设备使用与 YAN 系列遥控器兼容的格力协议。当前空调状态保存在 ESP32 的 NVS 中，断电重启后继续使用。

## 可选接入 Home Assistant

1. 连接 `Seeed_IR_Mate` 热点完成 Wi-Fi 配网。
2. 在 Home Assistant 中添加自动发现的 `IR Mate`。
3. 使用 `remote.learn_command` 学习按键。
4. 使用 `remote.send_command` 发送已学习的按键。

Home Assistant 发起学习后，设备才会开启红外接收。学习成功、取消或超时后，接收器会自动关闭。

操作示例：

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

学习到的按键由 Home Assistant 持久化保存，重启后仍可使用。离线触摸控制使用设备本地保存的格力空调状态，与 Home Assistant 的学习按键相互独立。
