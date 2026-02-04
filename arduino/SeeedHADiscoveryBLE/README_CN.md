# Seeed Home Assistant Discovery BLE (蓝牙版)

[![Version](https://img.shields.io/badge/版本-1.6.1-blue.svg)](https://github.com/limengdu/Seeed-Homeassistant-Discovery)
[![License](https://img.shields.io/badge/许可证-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/平台-ESP32%20|%20nRF52840-orange.svg)](https://www.espressif.com/)

一个轻量级的 Arduino 库，用于通过蓝牙低功耗（BLE）将 ESP32 和 nRF52840 设备连接到 Home Assistant。基于 BTHome v2 协议，原生支持 HA。

## ✨ 功能特性

- **BTHome v2 协议** - 原生 Home Assistant 支持，传感器无需自定义集成
- **被动广播** - 低功耗传感器数据广播
- **GATT 控制** - 用于开关和控制的双向通信
- **HA 状态订阅** - 通过 BLE 接收 Home Assistant 实体状态变化
- **多平台支持** - 支持 ESP32（NimBLE）和 nRF52840（ArduinoBLE）
- **低功耗** - 非常适合电池供电设备

## 🔧 支持的硬件

| 开发板 | BLE 协议栈 | 备注 |
|--------|-----------|------|
| XIAO ESP32-C3 | NimBLE | 紧凑型，WiFi+BLE |
| XIAO ESP32-C5 | NimBLE | 双频 WiFi + BLE |
| XIAO ESP32-C6 | NimBLE | Thread/Zigbee + BLE |
| XIAO ESP32-S3 | NimBLE | 高性能 + BLE |
| XIAO nRF52840 | ArduinoBLE | 超低功耗 BLE |
| XIAO nRF52840 Sense | ArduinoBLE | IMU + 麦克风 + BLE |

## 📦 安装

### 方法 1：下载 ZIP

1. 前往 [GitHub 仓库](https://github.com/limengdu/Seeed-Homeassistant-Discovery)
2. 点击 **Code → Download ZIP**
3. 解压 ZIP 文件
4. 将 `arduino/SeeedHADiscoveryBLE` 文件夹复制到 Arduino 库目录：
   - Windows: `文档/Arduino/libraries/`
   - macOS: `~/Documents/Arduino/libraries/`
   - Linux: `~/Arduino/libraries/`
5. 重启 Arduino IDE

### 方法 2：Git 克隆

```bash
git clone https://github.com/limengdu/Seeed-Homeassistant-Discovery.git
```

然后将 `arduino/SeeedHADiscoveryBLE` 文件夹复制到 Arduino 库目录。

> **注意：** 此库不在 Arduino 库管理器中，请使用手动安装方式。

## 📚 依赖库

### ESP32（XIAO ESP32-C3/C5/C6/S3）

| 库名 | 版本 | 用途 |
|------|------|------|
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | ^1.4.0 | ESP32 BLE 协议栈 |

### nRF52840（XIAO nRF52840）

| 库名 | 版本 | 用途 |
|------|------|------|
| [ArduinoBLE](https://github.com/arduino-libraries/ArduinoBLE) | ^1.3.0 | nRF52840 BLE 协议栈 |

## 🚀 快速开始

### 基础传感器示例（BTHome 广播）

```cpp
#include <SeeedHADiscoveryBLE.h>

SeeedHADiscoveryBLE ble;
SeeedBLESensor* tempSensor;
SeeedBLESensor* humiSensor;

void setup() {
    Serial.begin(115200);
    
    // 初始化 BLE（仅传感器模式）
    ble.begin("房间传感器");
    
    // 添加 BTHome 传感器
    tempSensor = ble.addTemperature();
    humiSensor = ble.addHumidity();
    
    // 开始广播
    ble.advertise();
}

void loop() {
    ble.loop();
    
    // 更新传感器值
    tempSensor->setValue(25.5);  // °C
    humiSensor->setValue(60.0);  // %
    
    // 更新广播数据
    ble.updateAdvertiseData();
    
    delay(5000);
}
```

### 开关控制示例（GATT 双向通信）

```cpp
#include <SeeedHADiscoveryBLE.h>

SeeedHADiscoveryBLE ble;
SeeedBLESwitch* ledSwitch;

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    
    // 初始化 BLE 并启用控制功能（GATT 服务）
    ble.begin("LED 控制器", true);
    
    // 添加开关
    ledSwitch = ble.addSwitch("led", "LED 灯");
    ledSwitch->onStateChange([](bool state) {
        digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
        Serial.printf("LED: %s\n", state ? "开启" : "关闭");
    });
    
    ble.advertise();
}

void loop() {
    ble.loop();  // 必须调用以处理 GATT 事件
}
```

### HA 状态订阅示例

```cpp
#include <SeeedHADiscoveryBLE.h>

SeeedHADiscoveryBLE ble;

void setup() {
    Serial.begin(115200);
    
    // 订阅 HA 实体（在 begin 之前）
    ble.subscribeEntity(0, "sensor.living_room_temperature");
    ble.subscribeEntity(1, "switch.light");
    
    // 注册状态变化回调
    ble.onHAState([](uint8_t index, const char* entityId, const char* state, float value) {
        Serial.printf("HA[%d] %s = %s", index, entityId, state);
        if (value != 0) Serial.printf(" (%.2f)", value);
        Serial.println();
    });
    
    // 启用控制功能初始化
    ble.begin("HA 订阅器", true);
    ble.advertise();
}

void loop() {
    ble.loop();
    
    // 直接访问状态
    SeeedBLEHAState* temp = ble.getHAState(0);
    if (temp && temp->hasValue()) {
        Serial.printf("温度: %.1f\n", temp->getFloat());
    }
}
```

## 📂 示例程序

| 示例 | 描述 | 平台 |
|------|------|------|
| [TemperatureBLE](examples/TemperatureBLE/) | 基础传感器广播（BTHome） | ESP32 / nRF52840 |
| [ButtonBLE](examples/ButtonBLE/) | 通过 BLE 发送按钮事件 | ESP32 / nRF52840 |
| [LEDSwitchBLE](examples/LEDSwitchBLE/) | 通过 GATT 控制 LED | ESP32 / nRF52840 |
| [HAStateSubscribeBLE](examples/HAStateSubscribeBLE/) | 订阅 HA 实体状态 | ESP32 / nRF52840 |
| [XIAO_nRF52840_LowPowerMotionDetect](examples/XIAO_nRF52840_LowPowerMotionDetect/) | 超低功耗运动检测，IMU 唤醒 | nRF52840 Sense |
| [XIAO_ESP32_Series_BluetoothProxy](examples/XIAO_ESP32_Series_BluetoothProxy/) | BLE-MQTT 网关，转发 BTHome 到 MQTT | ESP32-C3/C5/C6/S3 |

## 🔌 API 参考

### SeeedHADiscoveryBLE 类

#### 配置
```cpp
void setDeviceName(const char* name);
void enableDebug(bool enable = true);
void setAdvertiseInterval(uint32_t intervalMs);
void setTxPower(int8_t power);
```

#### 初始化
```cpp
bool begin(const char* deviceName = "Seeed Sensor");
bool begin(const char* deviceName, bool enableControl);
void stop();
void loop();  // GATT 模式下必须在 loop() 中调用
```

#### 传感器管理
```cpp
SeeedBLESensor* addSensor(BTHomeObjectId objectId);
SeeedBLESensor* addTemperature();
SeeedBLESensor* addHumidity();
SeeedBLESensor* addBattery();
SeeedBLESensor* addButton();
```

#### 开关管理
```cpp
SeeedBLESwitch* addSwitch(const char* id, const char* name);
```

#### HA 状态订阅
```cpp
SeeedBLEHAState* subscribeEntity(uint8_t entityIndex, const char* entityId);
void onHAState(BLEHAStateCallback callback);
SeeedBLEHAState* getHAState(uint8_t entityIndex);
```

#### 广播
```cpp
void advertise();
void updateAdvertiseData();
```

#### 状态查询
```cpp
bool isRunning() const;
bool isConnected() const;
String getAddress();
```

### SeeedBLESensor 类

```cpp
void setValue(int32_t value);
void setValue(float value);
void setState(bool state);  // 用于二进制传感器
void triggerButton(BTHomeButtonEvent event);  // 用于按钮事件
```

### SeeedBLESwitch 类

```cpp
void setState(bool state);
void toggle();
bool getState() const;
void onStateChange(BLESwitchCallback callback);
```

## 📡 BTHome 传感器类型

| 传感器 | Object ID | 数据类型 |
|--------|-----------|----------|
| 温度 | `BTHOME_TEMPERATURE` | 0.01°C 精度 |
| 湿度 | `BTHOME_HUMIDITY` | 0.01% 精度 |
| 电量 | `BTHOME_BATTERY` | 1% 精度 |
| 气压 | `BTHOME_PRESSURE` | 0.01 hPa 精度 |
| 光照 | `BTHOME_ILLUMINANCE` | 0.01 lux 精度 |
| CO2 | `BTHOME_CO2` | 1 ppm 精度 |
| PM2.5 | `BTHOME_PM25` | 1 µg/m³ 精度 |
| 按钮 | `BTHOME_BUTTON` | 事件类型 |
| 运动 | `BTHOME_BINARY_MOTION` | 二进制状态 |
| 占用 | `BTHOME_BINARY_OCCUPANCY` | 二进制状态 |

## 🏠 Home Assistant 集成

### 传感器（BTHome）

BTHome 设备原生被 Home Assistant 支持。只需：

1. 进入 **设置 → 设备与服务**
2. 查找已发现的 BTHome 设备
3. 点击 **配置** 添加

### 开关（GATT 控制）

开关控制需要 [Seeed HA Discovery](https://github.com/limengdu/Seeed-Homeassistant-Discovery) 自定义集成：

1. 在 Home Assistant 中安装 HACS
2. 添加自定义仓库：`https://github.com/limengdu/Seeed-Homeassistant-Discovery`
3. 安装 "Seeed HA Discovery" 集成
4. 重启 Home Assistant
5. 进入 设置 → 设备与服务 → 添加集成 → Seeed HA Discovery

## ⚡ 功耗

| 模式 | 电流（典型值） |
|------|---------------|
| 广播（1秒间隔） | ~15µA |
| 已连接（空闲） | ~30µA |
| 发送中 | ~8mA |
| 深度睡眠 | ~2µA |

## 🔋 电池续航技巧

1. **增加广播间隔** 以延长电池寿命：
   ```cpp
   ble.setAdvertiseInterval(10000);  // 10 秒
   ```

2. **使用仅传感器模式** 如果不需要控制功能：
   ```cpp
   ble.begin("传感器");  // 无 GATT 服务
   ```

3. **降低发射功率** 如果距离足够：
   ```cpp
   ble.setTxPower(-8);  // 较低功率
   ```

## 📝 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

## 🤝 贡献

欢迎贡献！请随时提交 Pull Request。

## 📧 支持

- GitHub Issues：[报告问题](https://github.com/limengdu/Seeed-Homeassistant-Discovery/issues)
- Seeed 论坛：[社区支持](https://forum.seeedstudio.com/)

