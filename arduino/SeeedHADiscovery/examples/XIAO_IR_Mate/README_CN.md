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
- 四击：切换空调模式。

板载触摸区为**自锁**开关:每按一下都会翻转状态,因此不论方向,每一下都算一次操作。快速连按后停手,约 **0.6 秒**静默,设备才判定手势：

- **单击**：按 1 下。
- **双击 / 三击 / 四击**：快速连续按 2、3 或 4 下。

设备使用与 YAN 系列遥控器兼容的格力协议。当前空调状态和四个触摸动作保存在 ESP32 的 NVS 中，断网或断电重启后继续使用。

## 可选接入 Home Assistant

1. 连接 `Seeed_IR_Mate` 热点完成 Wi-Fi 配网。
2. 在 Home Assistant 中添加自动发现的 `IR Mate`。
3. 打开 `设置 → 设备与服务 → Seeed HA Discovery`，找到 IR Mate 对应的配置入口并点击“配置”。
4. 选择设备类型、品牌和型号，然后添加要控制的设备。

品牌列表按品牌、型号字母排序，便于在长目录里定位。添加设备后，对应的控件会直接出现在 IR Mate 设备页上，并分区归类：

- **Controls(控制)**：**设备**与**命令**下拉框用于选择当前设备并发送它的任意命令；从内置码库添加的空调会生成完整的温控卡片(电源、模式、温度、风速、扫风)。库空调同样会在**命令**下拉框里提供 **Power / Temperature up / Temperature down / Mode / Fan speed** 等动作，与内置格力按键一致。
- **Configuration(配置)**：**Add or manage appliances** 按钮会跳转到"添加设备"的配置流程(品牌目录在那里);**单击 / 双击 / 三击 / 四击**下拉框把命令绑定到对应触摸手势并同步到设备；**学习单击 / 双击 / 三击 / 四击**按钮采集一段红外信号并直接存入对应手势槽位，适合库中没有的冷门遥控器。任意命令(包括库空调的有状态动作)都可以绑定到手势。
- **Diagnostic(诊断)**：**Last learned IR signal** 与 **Last transmitted IR signal** 两个传感器给出脉冲数，并在属性里展示完整波形。

学习进行期间设备才会开启红外接收。学习成功、取消或超时后，接收器会自动关闭。

## 学习信号(两次结构一致后保存)

学习会把同一个键采集两次。普通遥控器的两次波形一致时直接保存；空调等状态型遥控器会在每次按键后改变少量数据位，两次波形脉冲数量相同且至少 90% 的时序一致时，固件会识别为动态状态信号并保存第一遍的完整波形。结构差异明显的两次采集会报告失败。

1. 点某个**学习**按钮。IR Mate 状态灯变**白**，表示已就绪。
2. 用遥控器对着 IR Mate 按一次目标键。设备震一下、并短暂**再亮一次白灯**，提示你按确认键。
3. 再按**同一个**键。稳定波形一致或动态波形结构一致 → 状态灯变**绿**并把第一遍信号保存到该手势；结构差异明显(或没采到信号)→ 变**红**，该手势保持原来的绑定不变。
4. Home Assistant 通知会报告结果，成功时含脉冲数与波形预览。
5. 打开 **Last learned IR signal** 传感器可看到脉冲数和完整 `timings`；设备串口会分别打印 `IR learn pass 1` 与 `IR learn pass 2` 便于比对。
6. 选择**命令**(或该手势绑定的命令)发射一次。**Last transmitted IR signal** 传感器与串口 `IR TX` 行会显示发出的波形，应与学习到的逐点一致。

动态状态信号保存的是按键当时的一帧完整状态。例如学习“制冷 26℃、自动风速”后，每次发射都会还原这一固定状态。温度、模式和风速需要连续调节时，推荐从下方空调码库选择对应品牌与型号。

## 空调码库

插件内置了覆盖 342 个品牌型号的空调码库。在温控卡片里选好型号后，插件会按当前模式、风速和温度查出对应波形并发射，因此大多数空调无需学习即可使用。码库之外的遥控器可用学习按钮或 `remote` 学习服务兜底。

格力 YAN / YAW1F 使用固件内置码库，并驱动离线触摸默认动作。可学习设备的品牌与型号列表保存在 Home Assistant 插件的 `ir_profiles.json` 中，可以继续扩展。

学习得到的完整红外波形和码库选择都保存在 Home Assistant 的持久化存储中。绑定手势后，所需的触摸动作波形会复制到 IR Mate 的 NVS，因此设备离线时仍能按最后一次同步的配置工作。

内置格力动作和学习信号完全在设备本地执行，因此无论有没有网络都能用。库空调的有状态动作(温度步进、模式循环)由 Home Assistant 计算：在线时，物理手势会上报给 Home Assistant，由它根据当前状态解析出下一帧并发射；离线时，设备发射绑定时为该手势保存的固定兜底帧。

内置空调码库源自 MIT 许可的 [SmartIR](https://github.com/litinoveweedle/SmartIR) 项目，许可证见 `custom_components/seeed_ha_discovery/codes/LICENSE`。

## 状态反馈

- 学习进行中、等待按键：白灯亮起(确认按键前会再亮一次)。
- 红外发送成功，或两次按键结构一致的学习：绿灯亮起，短震一次。
- 红外发送失败，或两次按键结构差异明显的学习：红灯亮起，短震两次。
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
