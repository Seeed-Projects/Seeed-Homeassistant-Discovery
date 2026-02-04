# 低功耗运动检测 BLE 示例

基于 XIAO nRF52840 的超低功耗运动检测设备，使用 BTHome 协议与 Home Assistant 集成。

## 功能特性

- 通过 LSM6DS3 IMU Z 轴加速度检测运动
- BTHome v2 协议自动发现
- 超低功耗 System OFF 深度睡眠（< 5 µA）
- 检测到运动时自动唤醒
- LED 状态指示

## 硬件要求

- Seeed XIAO nRF52840（内置 LSM6DS3 IMU）
- USB-C 数据线（用于烧录）

## 软件依赖

### Arduino IDE 设置

1. **开发板管理器**：添加 Seeed nRF52 开发板
   - URL：`https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json`
   - 安装：**Seeed nRF52 Boards (Adafruit BSP)**

2. **依赖库**：
   - `Seeed Arduino LSM6DS3` - IMU 驱动
   - `Adafruit Bluefruit nRF52` - BLE 支持（随开发板包附带）

### 开发板选择

```
工具 → 开发板 → Seeed nRF52 Boards → Seeed XIAO nRF52840
```

## 工作原理

### 状态机

```
┌─────────────────────────────────────────────────────────────┐
│                        启动 / 唤醒                           │
└─────────────────────┬───────────────────────────────────────┘
                      ▼
┌─────────────────────────────────────────────────────────────┐
│  广播 motion=1（检测到运动）                                  │
│  - 每 100ms 刷新广播                                         │
│  - 绿灯闪烁                                                  │
└─────────────────────┬───────────────────────────────────────┘
                      │ 静止 6 秒
                      ▼
┌─────────────────────────────────────────────────────────────┐
│  广播 motion=0（运动停止）                                    │
│  - 每 100ms 刷新广播                                         │
│  - 持续 5 秒                                                 │
└─────────────────────┬───────────────────────────────────────┘
                      │ 5 秒后
                      ▼
┌─────────────────────────────────────────────────────────────┐
│  深度睡眠（System OFF）                                       │
│  - 蓝灯闪 3 次后进入                                         │
│  - 功耗 < 5 µA                                               │
│  - IMU 中断唤醒                                              │
└─────────────────────────────────────────────────────────────┘
```

### BTHome 数据格式

| 字段 | 值 | 说明 |
|------|-----|------|
| UUID | 0xFCD2 | BTHome 服务 UUID |
| 设备信息 | 0x44 | BTHome v2，触发型 |
| 对象 ID | 0x21 | 二进制运动 |
| 值 | 0x01/0x00 | 检测到/清除 |

## LED 指示

| LED | 模式 | 含义 |
|-----|------|------|
| 绿灯 | 快闪 2 次 | 从睡眠唤醒 |
| 绿灯 | 慢闪 3 次 | 正常上电 |
| 绿灯 | 短闪 | 检测到运动 |
| 蓝灯 | 闪 3 次 | 即将进入睡眠 |
| 蓝灯 | 持续闪烁 | IMU 初始化失败 |

## Home Assistant 集成

### 自动发现

设备会被 Home Assistant 通过 BTHome 集成自动发现：

1. 进入 **设置 → 设备与服务 → 集成**
2. 找到 **BTHome** 集成
3. 点击 **配置** 添加设备
4. 设备名称格式：`SeeedUA-XXXX`（MAC 后 4 位）

### 使用 ESP32 蓝牙代理

如果 XIAO 距离 Home Assistant 较远，可使用 ESP32 作为蓝牙代理：

1. 在 ESP32 上安装 ESPHome
2. 配置蓝牙代理：

```yaml
bluetooth_proxy:
  active: true
```

### 创建的实体

设备会创建一个二进制传感器：

| 实体 | 类型 | 设备类 |
|------|------|--------|
| `binary_sensor.seeedua_xxxx_motion` | 二进制传感器 | motion |

状态：`Detected`（检测到）/ `Clear`（清除）

## 配置参数

可在代码中调整的参数：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `ACCEL_THRESHOLD_Z` | 1.30 | Z 轴加速度阈值（g） |
| `DEBOUNCE_TIME` | 6000 | 运动→静止防抖时间（ms） |
| `CLEAR_DURATION` | 5000 | 睡眠前广播 clear 的持续时间（ms） |
| `ADVERTISE_INTERVAL` | 100 | 广播刷新间隔（ms） |

## 串口输出示例

```
========================================
  XIAO nRF52840 BTHome Motion Detect
========================================

Reset reason: 0x10000
>>> Woke from sleep (motion triggered) <<<

MAC Address: C1:F9:3E:1D:93:7C
Device Name: SeeedUA-937C

IMU initialization successful!
Motion wake! Starting BTHome advertising...
..... OK

========================================
  Initialization Complete!
========================================

Motion: broadcast motion=1
Still 6s: switch to motion=0
Still 11s: enter deep sleep

IMPORTANT: Disconnect USB to test power consumption!

>>> Motion=1
>>> Motion=0, sleep in 5s
>>> Entering sleep

>>> Entering deep sleep <<<
Waiting 3 seconds before sleep...
```

## 功耗测试

**重要**：测试功耗时必须断开 USB 连接！

| 状态 | 功耗 |
|------|------|
| 深度睡眠（System OFF） | < 5 µA |
| 广播中 | ~3 mA |

## 故障排除

### IMU 初始化失败
- 蓝灯持续闪烁表示 IMU 初始化失败
- 检查 I2C 连接
- 确认开发板选择正确

### Home Assistant 收不到状态
- 确保 BTHome 集成已安装
- 检查蓝牙代理是否正常工作
- 查看 HA 日志中的蓝牙相关信息

### 设备不进入睡眠
- 确保设备放置在水平稳定的表面
- 根据需要调整 Z 轴阈值
- 检查串口输出的状态转换

### 设备无法唤醒
- 确认 IMU 中断配置正确
- 检查运动是否足以触发唤醒
- 确保设备确实进入了睡眠模式

## 许可证

SeeedHADiscoveryBLE 库的一部分。MIT 许可证。
