# IR Mate 万能红外遥控器

该示例将 IR Mate 接入 Home Assistant，提供红外发射、红外接收和按键学习能力。

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

示例中的 `build_opt.h` 只启用原始红外波形收发能力，用于缩小固件体积。

## 使用

1. 烧录 `IRMateUniversalRemote.ino`。
2. 连接 `Seeed_IR_Mate` 热点完成 Wi-Fi 配网。
3. 在 Home Assistant 中添加自动发现的 `IR Mate`。
4. 使用 `remote.learn_command` 学习按键。
5. 使用 `remote.send_command` 发送已学习的按键。

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

学习到的按键由 Home Assistant 持久化保存，重启后仍可使用。

实体触摸按键会重放最近一次接收到的红外信号。该信号存放在内存中，重启后需要重新学习。
