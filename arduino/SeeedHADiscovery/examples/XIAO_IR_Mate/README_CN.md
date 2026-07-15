# IR Mate 万能红外遥控器

该示例让 IR Mate 在离线状态下直接控制格力空调，并可接入 Home Assistant，通过内置码库控制空调、学习红外命令，并在设备页配置触摸动作。

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

添加设备后，对应的控件会直接出现在 IR Mate 设备页上：

- **空调**：从内置码库添加的空调会生成完整的温控卡片，包含电源、模式、温度、风速和扫风。每次改动都会查出匹配的信号并发射。
- **设备**与**命令**下拉框：选择当前设备，并发送它的任意命令。
- **单击 / 双击 / 三击 / 长按**下拉框：把命令绑定到对应触摸手势，并同步到设备。
- **学习单击 / 双击 / 三击 / 长按**按钮:采集一段红外信号并直接存入对应手势槽位,适合库中没有的冷门遥控器。

学习进行期间设备才会开启红外接收。学习成功、取消或超时后，接收器会自动关闭。

## 空调码库

插件内置了覆盖 342 个品牌型号的空调码库。在温控卡片里选好型号后，插件会按当前模式、风速和温度查出对应波形并发射，因此大多数空调无需学习即可使用。码库之外的遥控器可用学习按钮或 `remote` 学习服务兜底。

格力 YAN / YAW1F 使用固件内置码库，并驱动离线触摸默认动作。可学习设备的品牌与型号列表保存在 Home Assistant 插件的 `ir_profiles.json` 中，可以继续扩展。

学习得到的完整红外波形和码库选择都保存在 Home Assistant 的持久化存储中。绑定手势后，所需的触摸动作波形会复制到 IR Mate 的 NVS，因此设备离线时仍能按最后一次同步的配置工作。

内置空调码库源自 MIT 许可的 [SmartIR](https://github.com/litinoveweedle/SmartIR) 项目，许可证见 `custom_components/seeed_ha_discovery/codes/LICENSE`。

## 状态反馈

- 红外发送或学习成功：绿灯亮起，短震一次。
- 红外发送或学习失败：红灯亮起，短震两次。
- 配网、断网或尚未接入 Home Assistant：蓝灯闪烁。
- Home Assistant 连接完成并进入待机：状态灯熄灭。

自动化也可以继续调用 `Universal remote` 实体上的标准 `remote` 服务，该实体默认启用：

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
