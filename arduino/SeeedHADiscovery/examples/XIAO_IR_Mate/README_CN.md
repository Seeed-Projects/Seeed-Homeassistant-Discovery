# IR Mate 万能红外遥控器

该示例让 IR Mate 在离线状态下直接控制格力空调，并可接入 Home Assistant 管理品牌设备、学习红外命令和配置触摸动作。

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
- 长按：切换空调模式。

设备使用与 YAN 系列遥控器兼容的格力协议。当前空调状态和四个触摸动作保存在 ESP32 的 NVS 中，断网或断电重启后继续使用。

## 可选接入 Home Assistant

1. 连接 `Seeed_IR_Mate` 热点完成 Wi-Fi 配网。
2. 在 Home Assistant 中添加自动发现的 `IR Mate`。
3. 打开 `设置 → 设备与服务 → Seeed HA Discovery`，找到 IR Mate 对应的配置入口并点击“配置”。
4. 选择设备类型、品牌和型号，然后添加要控制的设备。
5. 对需要学习的命令点击“学习”，按页面提示操作原遥控器。
6. 在“触摸动作绑定”中配置单击、双击、三击和长按，然后点击“保存并同步”。

Home Assistant 发起学习后，设备才会开启红外接收。学习成功、取消或超时后，接收器会自动关闭。

格力 YAN / YAW1F 使用固件内置码库，可以直接测试和绑定。其他品牌提供常用命令模板，完成学习后即可测试、重新学习、删除或绑定到触摸动作。品牌与型号列表保存在 Home Assistant 插件的 `ir_profiles.json` 中，可以继续扩展。

学习得到的完整红外波形保存在 Home Assistant 的持久化存储中。点击“保存并同步”后，四个触摸动作所需的波形会复制到 IR Mate 的 NVS，因此设备离线时仍能按最后一次同步的配置工作。

## 状态反馈

- 红外发送或学习成功：绿灯亮起，短震一次。
- 红外发送或学习失败：红灯亮起，短震两次。
- 配网、断网或尚未接入 Home Assistant：蓝灯闪烁。
- Home Assistant 连接完成并进入待机：状态灯熄灭。

自动化也可以继续调用标准 `remote` 服务。该实体默认隐藏，可在实体注册表中启用：

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
