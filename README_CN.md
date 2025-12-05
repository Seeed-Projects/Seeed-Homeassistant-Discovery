# Seeed Home Assistant Discovery

<p align="center">
  <img src="custom_components/seeed_ha_discovery/icon.png" width="128" alt="Seeed HA Discovery">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/ESP32-C3%20%7C%20C6%20%7C%20S3-blue" alt="ESP32 Support">
  <img src="https://img.shields.io/badge/nRF52840-BLE-purple" alt="nRF52840 Support">
  <img src="https://img.shields.io/badge/Home%20Assistant-2025.12+-green" alt="Home Assistant">
  <img src="https://img.shields.io/badge/Arduino-IDE%20%7C%20PlatformIO-orange" alt="Arduino">
  <img src="https://img.shields.io/badge/HACS-Custom-41BDF5" alt="HACS Custom">
</p>

**Seeed HA Discovery** 是一个让 ESP32/nRF52840 设备轻松连接 Home Assistant 的完整解决方案，由 [Seeed Studio](https://www.seeedstudio.com/) 提供。

### 🎯 它能做什么？

只需在 **Arduino IDE** 或 **PlatformIO** 中为你的 **XIAO** 系列开发板编写几行代码，就可以通过 **WiFi** 或 **蓝牙 (BLE)** 连接到 Home Assistant：

| 连接方式 | 支持设备 | 特点 |
|----------|----------|------|
| 📶 **WiFi** | XIAO ESP32-C3/C6/S3 | 双向通信、WebSocket 实时更新 |
| 📡 **蓝牙 (BLE)** | XIAO ESP32-C3/C6/S3, **XIAO nRF52840** | 超低功耗、BTHome v2 协议、被动广播 |

| 功能 | 方向 | WiFi | BLE |
|------|------|------|-----|
| 📤 **上报传感器数据** | 设备 → HA | ✅ | ✅ |
| 📥 **接收控制命令** | HA → 设备 | ✅ | ✅ (GATT) |
| 📷 **摄像头推流** | 设备 → HA | ✅ (ESP32-S3) | ❌ |
| 🔄 **获取 HA 状态** | HA → 设备 | ✅ (v2.3 新增) | ❌ |
| 🔋 **超低功耗** | - | ❌ | ✅ (广播模式) |

### 💡 无需复杂配置

- ✅ **无需 MQTT** - 不需要搭建 MQTT 服务器
- ✅ **无需云服务** - 纯局域网通信，数据不出家门
- ✅ **自动发现** - 设备上线后 Home Assistant 自动识别
- ✅ **即插即用** - 复制示例代码，修改配置即可运行

## ⚡ 一键安装

点击下方按钮，将此集成添加到你的 Home Assistant：

[![Open your Home Assistant instance and open a repository inside the Home Assistant Community Store.](https://my.home-assistant.io/badges/hacs_repository.svg)](https://my.home-assistant.io/redirect/hacs_repository/?owner=limengdu&repository=Seeed-Homeassistant-Discovery&category=integration)

> **前提条件**：你的 Home Assistant 必须已安装 [HACS](https://hacs.xyz/)

## ✨ 特点

### WiFi 版本
- 🔍 **自动发现** - 设备连接 WiFi 后自动被 Home Assistant 发现
- 📡 **实时通信** - 使用 WebSocket 双向实时通信
- 🎯 **简单易用** - 几行代码即可将传感器接入 HA
- 🌡️ **传感器支持** - 支持温度、湿度等各类传感器（上行数据）
- 💡 **开关控制** - 支持 LED、继电器等开关控制（下行命令）
- 📷 **摄像头推流** - 支持 XIAO ESP32-S3 Sense 摄像头实时画面 (v2.2 新增)
- 🔄 **HA 状态订阅** - 设备可订阅 HA 实体状态，适合显示屏应用 (v2.3 新增)
- 📱 **状态页面** - 内置 Web 页面查看设备状态

### BLE 版本 (v2.0 新增)
- 🔋 **超低功耗** - 被动广播模式，适合电池供电设备
- 📡 **BTHome v2** - 使用 Home Assistant 原生支持的 BTHome 协议
- 🎯 **零配置** - 无需安装额外集成，HA 自动识别 BTHome 设备
- 📱 **支持 nRF52840** - 不仅限于 ESP32，也支持 XIAO nRF52840
- 🔘 **事件支持** - 支持按钮单击、双击、长按等事件
- 🔄 **双向控制** - 支持 GATT 双向通信，可远程控制开关

## 🤔 为什么不用 ESPHome？

ESPHome 是一个优秀的项目，但它并不适合所有人。如果你有以下需求，**Seeed HA Discovery** 可能更适合你：

### 1. 🎓 更熟悉 Arduino 编程

> *"我习惯用 Arduino IDE 写代码，不想学 YAML 配置语法"*

| ESPHome | Seeed HA Discovery |
|---------|-------------------|
| 使用 YAML 配置文件 | 使用标准 **C/C++ 代码** |
| 默认基于 ESP-IDF 框架（可选 Arduino） | 基于 **Arduino 框架** |
| 需要学习新语法 | 沿用你已有的 Arduino 技能 |

```cpp
// Seeed HA Discovery - 就是你熟悉的 Arduino 代码
void setup() {
    ha.begin("WiFi", "password");
    tempSensor = ha.addSensor("temp", "温度", "temperature", "°C");
}

void loop() {
    ha.handle();
    tempSensor->setValue(25.5);
}
```

### 2. 📚 Arduino 生态系统更丰富

> *"我想用某个 Arduino 库，但 ESPHome 不支持"*

- ✅ **直接使用任何 Arduino 库** - 传感器驱动、显示屏、通信模块...
- ✅ **深度睡眠、低功耗模式** - 完全控制 ESP32 的电源管理
- ✅ **复杂业务逻辑** - 用代码实现任何你想要的功能
- ✅ **自定义通信协议** - 不受框架限制

### 3. 🔄 ESPHome 更新太频繁

> *"上个月还能用的配置，这个月就报错了"*

- ESPHome 的**破坏性更新**频繁，历史教程容易失效
- 组件 API 经常变化，旧代码需要不断修改
- **Seeed HA Discovery** 使用稳定的 Arduino API，向后兼容性更好

### 4. ⏱️ 编译速度

> *"ESPHome 编译一次要好几分钟"*

- ESPHome 功能越来越多，编译时间越来越长
- Arduino 项目编译速度更快，迭代效率更高
- 增量编译更有效，修改代码后秒级重新编译

### 5. 🚀 无需等待官方审核

> *"我想添加一个新传感器，但 ESPHome 官方审核太慢"*

- ESPHome 添加新组件需要提交 PR，审核周期长、标准严格
- **Seeed HA Discovery** 让你自由编写代码，无需等待任何人
- 你的传感器、你的代码、你的节奏

### 📊 适用场景对比

| 场景 | 推荐方案 |
|------|----------|
| 快速部署标准传感器 | ESPHome ✅ |
| 需要自定义 Arduino 代码 | **Seeed HA Discovery** ✅ |
| 不想学习新语法 | **Seeed HA Discovery** ✅ |
| 使用冷门传感器/模块 | **Seeed HA Discovery** ✅ |
| 需要低功耗/深度睡眠 | **Seeed HA Discovery** ✅ |
| 纯 GUI 配置，零代码 | ESPHome ✅ |

---

## 📦 组件

本项目包含三部分：

1. **Home Assistant 集成** (`custom_components/seeed_ha_discovery/`)
   - 自动发现局域网内的 WiFi 设备
   - 接收并显示传感器数据
   - 发送控制命令到设备

2. **WiFi Arduino 库** (`arduino/SeeedHADiscovery/`)
   - 用于 ESP32 设备 WiFi 编程
   - 支持传感器上报和开关控制
   - WebSocket 双向通信

3. **BLE Arduino 库** (`arduino/SeeedHADiscoveryBLE/`) - **v2.0 新增**
   - 用于 ESP32/nRF52840 蓝牙编程
   - 基于 BTHome v2 协议
   - 超低功耗被动广播

## 🚀 快速开始

### 1. 安装 Home Assistant 集成

**方法 A: 通过 HACS 一键安装（推荐）**

点击上方的 "一键安装" 按钮，或者手动添加：

1. 打开 HACS → 集成
2. 点击右上角 "⋮" → "自定义存储库"
3. 输入 `https://github.com/limengdu/Seeed-Homeassistant-Discovery`
4. 类别选择 "Integration"
5. 点击添加，然后搜索 "Seeed HA Discovery" 并安装
6. 重启 Home Assistant

**方法 B: 手动安装**

将 `custom_components/seeed_ha_discovery` 文件夹复制到 Home Assistant 的 `config/custom_components/` 目录，然后重启 Home Assistant。

### 2. 安装 Arduino 库

根据你的连接方式选择对应的库：

#### WiFi 版本 (SeeedHADiscovery)

**Arduino IDE:**
1. 下载 `arduino/SeeedHADiscovery` 文件夹
2. 复制到 `文档/Arduino/libraries/`
3. 安装依赖库（通过库管理器）：
   - ArduinoJson (作者: Benoit Blanchon)
   - WebSockets (作者: Markus Sattler)

**PlatformIO:**
```ini
lib_deps =
    bblanchon/ArduinoJson@^7.0.0
    links2004/WebSockets@^2.4.0
```

#### BLE 版本 (SeeedHADiscoveryBLE)

**Arduino IDE:**
1. 下载 `arduino/SeeedHADiscoveryBLE` 文件夹
2. 复制到 `文档/Arduino/libraries/`
3. 根据你的开发板安装对应的 BLE 依赖库：

| 开发板 | 依赖库 | 安装方式 |
|--------|--------|----------|
| **ESP32 系列** (C3/C6/S3) | NimBLE-Arduino | Arduino 库管理器搜索 "NimBLE-Arduino" |

> ⚠️ **ESP32 必须安装 NimBLE-Arduino 库**，否则编译会报错！
>
> NimBLE 比 ESP32 官方的蓝牙库更轻量、更稳定，是 ESP32 BLE 开发的首选。

**PlatformIO:**
```ini
; ESP32 系列
lib_deps =
    h2zero/NimBLE-Arduino@^1.4.0

; nRF52840 (mbed)
; ArduinoBLE 已内置于 Seeed mbed 核心，无需额外安装
```

### 3. 编写 Arduino 程序

#### WiFi 示例 - 温湿度传感器

```cpp
#include <SeeedHADiscovery.h>

const char* WIFI_SSID = "你的WiFi名称";
const char* WIFI_PASSWORD = "你的WiFi密码";

SeeedHADiscovery ha;
SeeedHASensor* tempSensor;
SeeedHASensor* humiditySensor;

void setup() {
    Serial.begin(115200);
    ha.setDeviceInfo("客厅传感器", "ESP32-C3", "1.0.0");
    ha.enableDebug(true);

    if (!ha.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("WiFi 连接失败!");
        while (1) delay(1000);
    }

    tempSensor = ha.addSensor("temperature", "温度", "temperature", "°C");
    tempSensor->setPrecision(1);

    humiditySensor = ha.addSensor("humidity", "湿度", "humidity", "%");
    humiditySensor->setPrecision(0);
}

void loop() {
    ha.handle();

    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        lastUpdate = millis();
        tempSensor->setValue(25.5);
        humiditySensor->setValue(55);
    }
}
```

#### WiFi 示例 - 摄像头推流 (XIAO ESP32-S3 Sense)

```cpp
#include <SeeedHADiscovery.h>
#include "esp_camera.h"

const char* WIFI_SSID = "Your_WiFi_SSID";
const char* WIFI_PASSWORD = "Your_WiFi_Password";

SeeedHADiscovery ha;

void setup() {
    Serial.begin(115200);
    
    // Initialize camera (XIAO ESP32-S3 Sense specific pins)
    camera_config_t config;
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.pin_xclk = 10;
    config.pin_sccb_sda = 40;
    config.pin_sccb_scl = 39;
    config.pin_d7 = 48;
    config.pin_d6 = 11;
    config.pin_d5 = 12;
    config.pin_d4 = 14;
    config.pin_d3 = 16;
    config.pin_d2 = 18;
    config.pin_d1 = 17;
    config.pin_d0 = 15;
    config.pin_vsync = 38;
    config.pin_href = 47;
    config.pin_pclk = 13;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    
    esp_camera_init(&config);
    
    ha.setDeviceInfo("XIAO Camera", "XIAO ESP32-S3 Sense", "1.0.0");
    ha.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Start camera server on port 82
    // Still image: http://<IP>:82/camera
    // MJPEG stream: http://<IP>:82/stream
    startCameraServer();
}

void loop() {
    ha.handle();
}
```

> **Note**: Camera example requires XIAO ESP32-S3 Sense with OV2640 camera module.
> Make sure to enable PSRAM in Arduino IDE: Tools → PSRAM → OPI PSRAM

#### BLE 示例 - 温湿度传感器 (超低功耗)

```cpp
#include <SeeedHADiscoveryBLE.h>

SeeedHADiscoveryBLE ble;
SeeedBLESensor* tempSensor;
SeeedBLESensor* humiditySensor;
SeeedBLESensor* batterySensor;

void setup() {
    Serial.begin(115200);
    ble.enableDebug(true);

    if (!ble.begin("XIAO 温湿度传感器")) {
        Serial.println("BLE 初始化失败!");
        while (1) delay(1000);
    }

    // 使用 BTHome 标准传感器类型
    tempSensor = ble.addTemperature();
    humiditySensor = ble.addHumidity();
    batterySensor = ble.addBattery();
}

void loop() {
    // 设置传感器值
    tempSensor->setValue(25.5);      // 温度 25.5°C
    humiditySensor->setValue(55.0);  // 湿度 55%
    batterySensor->setValue(100);    // 电池 100%

    // 发送 BLE 广播
    ble.advertise();

    // 等待 10 秒（BLE 适合低频率更新）
    delay(10000);
}
```

#### BLE 示例 - LED 开关控制 (双向通信)

```cpp
#include <SeeedHADiscoveryBLE.h>

SeeedHADiscoveryBLE ble;
SeeedBLESwitch* ledSwitch;

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    
    ble.enableDebug(true);
    
    // 启用 GATT 服务器 (第二个参数为 true)
    if (!ble.begin("XIAO LED 控制器", true)) {
        Serial.println("BLE 初始化失败!");
        while (1) delay(1000);
    }

    // 添加 LED 开关
    ledSwitch = ble.addSwitch("led", "板载 LED");
    
    // 注册回调：当 HA 发送控制命令时执行
    ledSwitch->onStateChange([](bool state) {
        digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
        Serial.printf("LED: %s\n", state ? "开" : "关");
    });
}

void loop() {
    ble.loop();  // 必须调用！处理 GATT 事件
    delay(10);
}
```

### 4. 在 Home Assistant 中添加设备

**WiFi 设备：** 会被自动发现！或者手动添加：
1. 进入 **设置** → **设备与服务**
2. 点击 **添加集成**
3. 搜索 **Seeed HA Discovery**
4. 输入 ESP32 的 IP 地址

**BLE 设备：** 使用 BTHome 协议，会被 Home Assistant 自动发现！
1. 确保 HA 有蓝牙适配器或 ESP32 蓝牙代理
2. 设备会自动出现在 **设置** → **设备与服务** → **BTHome**

### 5. 配置 HA 状态订阅 (v2.3 新增)

WiFi 设备可以订阅 Home Assistant 中的实体状态，这对于显示屏设备特别有用。

**配置步骤：**
1. 进入 **设置** → **设备与服务** → **Seeed HA Discovery**
2. 找到你的设备，点击 **配置**
3. 在下拉列表中选择要订阅的实体
4. 点击 **提交**

**支持的实体类型：**
- sensor (传感器)
- binary_sensor (二进制传感器)
- switch (开关)
- light (灯)
- climate (空调/温控)

**工作原理：**
- 实体状态变化时，HA 会**自动推送**到设备
- 设备重连后，订阅会**自动恢复**
- 修改订阅配置后，设备会**立即收到**新的实体状态

---

## 📖 API 参考

### WiFi 库 - SeeedHADiscovery 类

| 方法 | 说明 |
|------|------|
| `setDeviceInfo(name, model, version)` | 设置设备信息 |
| `enableDebug(enable)` | 启用调试输出 |
| `begin(ssid, password)` | 连接 WiFi 并启动服务 |
| `addSensor(id, name, deviceClass, unit)` | 添加传感器（上行数据）|
| `addSwitch(id, name, icon)` | 添加开关（下行控制）|
| `handle()` | 处理网络事件（必须在 loop 中调用）|
| `isWiFiConnected()` | 检查 WiFi 连接 |
| `isHAConnected()` | 检查 HA 连接 |
| `getLocalIP()` | 获取 IP 地址 |
| `onHAState(callback)` | 注册 HA 状态变化回调 |
| `getHAState(entityId)` | 获取指定实体的状态对象 |
| `getHAStates()` | 获取所有已订阅实体的状态 |
| `clearHAStates()` | 清除所有 HA 状态缓存 |

### WiFi 库 - SeeedHASensor 类

| 方法 | 说明 |
|------|------|
| `setValue(value)` | 设置传感器值（自动推送到 HA）|
| `setStateClass(stateClass)` | 设置状态类别 |
| `setPrecision(precision)` | 设置小数精度 |
| `setIcon(icon)` | 设置图标（mdi:xxx 格式）|

### WiFi 库 - SeeedHASwitch 类

| 方法 | 说明 |
|------|------|
| `onStateChange(callback)` | 注册状态变化回调（接收 HA 命令）|
| `setState(state)` | 设置开关状态（同步到 HA）|
| `toggle()` | 切换开关状态 |
| `getState()` | 获取当前状态 |
| `setIcon(icon)` | 设置图标（mdi:xxx 格式）|

### WiFi 库 - SeeedHAState 类 (v2.3 新增)

| 方法 | 说明 |
|------|------|
| `getEntityId()` | 获取实体 ID |
| `getFriendlyName()` | 获取友好名称 |
| `getString()` | 获取状态字符串 |
| `getFloat()` | 获取状态浮点数值 |
| `getBool()` | 获取状态布尔值 |
| `getUnit()` | 获取单位 |
| `getDeviceClass()` | 获取设备类型 |
| `hasValue()` | 检查是否有有效值 |

### BLE 库 - SeeedHADiscoveryBLE 类

| 方法 | 说明 |
|------|------|
| `begin(deviceName, enableGattServer)` | 初始化 BLE（第二个参数启用双向控制）|
| `enableDebug(enable)` | 启用调试输出 |
| `addSensor(objectId)` | 添加 BTHome 传感器 |
| `addTemperature()` | 添加温度传感器（便捷方法）|
| `addHumidity()` | 添加湿度传感器（便捷方法）|
| `addBattery()` | 添加电池传感器（便捷方法）|
| `addButton()` | 添加按钮事件（便捷方法）|
| `addSwitch(id, name)` | 添加开关（用于双向控制）|
| `advertise()` | 发送 BLE 广播 |
| `loop()` | 处理 GATT 事件（启用 GATT 时必须调用）|
| `stop()` | 停止 BLE |

### BLE 库 - SeeedBLESensor 类

| 方法 | 说明 |
|------|------|
| `setValue(value)` | 设置传感器值（整数或浮点数）|
| `setState(state)` | 设置二进制状态 |
| `triggerButton(event)` | 触发按钮事件 |

### BLE 库 - SeeedBLESwitch 类

| 方法 | 说明 |
|------|------|
| `onStateChange(callback)` | 注册状态变化回调（接收 HA 命令）|
| `setState(state)` | 设置开关状态（同步到 HA）|
| `getState()` | 获取当前状态 |

### BLE 按钮事件类型

| 事件 | 说明 |
|------|------|
| `BTHOME_BUTTON_PRESS` | 单击 |
| `BTHOME_BUTTON_DOUBLE` | 双击 |
| `BTHOME_BUTTON_TRIPLE` | 三击 |
| `BTHOME_BUTTON_LONG_PRESS` | 长按 |

### 常用 BTHome 传感器类型

| 类型 | 说明 | 精度 |
|------|------|------|
| `BTHOME_TEMPERATURE` | 温度 | 0.01°C |
| `BTHOME_HUMIDITY` | 湿度 | 0.01% |
| `BTHOME_PRESSURE` | 气压 | 0.01 hPa |
| `BTHOME_ILLUMINANCE` | 光照 | 0.01 lux |
| `BTHOME_BATTERY` | 电池 | 1% |
| `BTHOME_VOLTAGE` | 电压 | 0.001 V |
| `BTHOME_PM25` | PM2.5 | 1 μg/m³ |
| `BTHOME_CO2` | CO2 | 1 ppm |
| `BTHOME_BUTTON` | 按钮事件 | - |

---

## 📁 项目结构

```
seeed-ha-discovery/
├── custom_components/
│   └── seeed_ha_discovery/       # Home Assistant 集成
│       ├── __init__.py           # 主入口
│       ├── manifest.json         # 集成清单 (v2.2.0)
│       ├── config_flow.py        # 配置流程
│       ├── const.py              # 常量定义
│       ├── coordinator.py        # 数据协调器
│       ├── device.py             # 设备通信
│       ├── sensor.py             # 传感器平台
│       ├── switch.py             # 开关平台
│       ├── camera.py             # 摄像头平台 (v2.2 新增)
│       ├── strings.json          # 字符串
│       └── translations/         # 翻译文件
├── arduino/
│   ├── SeeedHADiscovery/         # WiFi Arduino 库
│   │   ├── src/
│   │   │   ├── SeeedHADiscovery.h
│   │   │   └── SeeedHADiscovery.cpp
│   │   ├── examples/
│   │   │   ├── TemperatureHumidity/  # 温湿度传感器示例
│   │   │   ├── LEDSwitch/            # LED 开关示例
│   │   │   ├── ButtonSwitch/         # 按钮开关示例 (v1.1)
│   │   │   └── CameraStream/         # 摄像头推流示例 (v1.3)
│   │   ├── library.json
│   │   └── library.properties
│   └── SeeedHADiscoveryBLE/      # BLE Arduino 库 (v2.0 新增)
│       ├── src/
│       │   ├── SeeedHADiscoveryBLE.h
│       │   └── SeeedHADiscoveryBLE.cpp
│       ├── examples/
│       │   ├── TemperatureBLE/       # 温湿度传感器示例 (被动广播)
│       │   ├── ButtonBLE/            # 按钮开关示例 (GATT 双向)
│       │   └── LEDSwitchBLE/         # LED 开关示例 (GATT 双向)
│       ├── library.json
│       └── library.properties
├── hacs.json
└── README.md
```

## 🔧 支持的硬件

| 开发板 | WiFi | BLE | 摄像头 | 状态 |
|--------|------|-----|--------|------|
| XIAO ESP32-C3 | ✅ | ✅ | ❌ | 已测试 |
| XIAO ESP32-C6 | ✅ | ✅ | ❌ | 已测试 |
| XIAO ESP32-S3 | ✅ | ✅ | ❌ | 已测试 |
| **XIAO ESP32-S3 Sense** | ✅ | ✅ | ✅ | 已测试 |
| XIAO nRF52840 | ❌ | ✅ | ❌ | 已测试 |
| ESP32 (原版) | ✅ | ✅ | ❌ | 已测试 |

> 📷 **摄像头功能**仅支持带 OV2640 摄像头模块的 **XIAO ESP32-S3 Sense**

## 📝 通信协议

### WiFi 协议 (WebSocket JSON)

**发现消息** (设备 → HA):
```json
{
  "type": "discovery",
  "entities": [
    {
      "id": "temperature",
      "name": "温度",
      "type": "sensor",
      "device_class": "temperature",
      "unit_of_measurement": "°C"
    }
  ]
}
```

**状态更新** (设备 → HA):
```json
{
  "type": "state",
  "entity_id": "temperature",
  "state": 26.0
}
```

**控制命令** (HA → 设备):
```json
{
  "type": "command",
  "entity_id": "led",
  "command": "turn_on"
}
```

### BLE 协议 (BTHome v2)

使用 [BTHome v2](https://bthome.io/) 标准协议，Home Assistant 原生支持自动发现。

**广播数据格式：**
```
[Flags][Service Data: UUID=0xFCD2][Device Info][Sensor Data...]
```

**Manufacturer ID:** `0x5EED` (24301)

---

## ❓ 常见问题 (FAQ)

### Q1: WiFi 和 BLE 有什么区别？该用哪个？

| 特性 | WiFi | BLE |
|------|------|-----|
| 通信方向 | 双向 (WebSocket) | 双向（广播 + GATT）|
| 功耗 | 较高 (~80mA) | 超低（广播 <1mA，GATT ~15mA）|
| 传输速度 | 快 | 慢 |
| 连接距离 | 较远（50m+） | 较近（~10m）|
| 适合场景 | 需要快速响应、实时性要求高 | 电池供电、低功耗优先 |
| 支持设备 | 仅 ESP32 | ESP32 + nRF52840 |

**推荐选择：**
- **选 WiFi**：需要实时控制（如灯光、风扇）、有稳定电源
- **选 BLE**：电池供电、传感器定期上报、低功耗优先

### Q2: BLE 有两种工作模式？

**是的！** BLE 库支持两种模式：

| 模式 | 说明 | 功耗 | 适用场景 |
|------|------|------|----------|
| **被动广播模式** | 只发送数据，不接收命令 | 超低（<1mA）| 电池供电传感器 |
| **GATT 双向模式** | 可发送数据，也可接收控制命令 | 较低（~15mA）| 需要远程控制的设备 |

```cpp
// 被动广播模式（默认）
ble.begin("设备名称");  // 只上报数据

// GATT 双向模式
ble.begin("设备名称", true);  // 第二个参数 true 启用双向通信
ble.addSwitch("led", "LED");  // 可以添加开关等可控实体
```

### Q3: BLE 设备没有被 Home Assistant 发现？

1. 确保 Home Assistant 有蓝牙适配器
2. 或者配置 [ESP32 蓝牙代理](https://esphome.io/components/bluetooth_proxy.html)
3. BTHome 设备会自动出现，无需手动添加

### Q4: 传感器数量有限制吗？

**没有硬编码限制**。理论上只受设备内存限制。

### Q5: 单位可以自定义吗？

- **WiFi 版本**: 单位完全由 Arduino 端定义，是纯字符串
- **BLE 版本**: 单位由 BTHome 协议定义，自动匹配

### Q6: 支持哪些 device_class？

参考 [Home Assistant 传感器文档](https://www.home-assistant.io/integrations/sensor/#device-class)。

### Q7: 如何使用摄像头功能？

**硬件要求：**
- XIAO ESP32-S3 Sense（带 OV2640 摄像头模块）

**软件配置：**
1. 在 Arduino IDE 中选择开发板 "XIAO_ESP32S3"
2. 启用 PSRAM: Tools → PSRAM → OPI PSRAM
3. 上传 `CameraStream` 示例

**访问方式：**
- 静态图片: `http://<设备IP>:82/camera`
- MJPEG 视频流: `http://<设备IP>:82/stream`

**在 Home Assistant 中：**
设备被发现后，会自动添加一个摄像头实体，以 4 FPS 刷新率显示画面。

### Q8: 多个设备使用相同代码，HA 能区分吗？

**可以！** Home Assistant 通过每个设备的**唯一标识**来区分：

| 连接方式 | 唯一标识 | 示例 |
|----------|----------|------|
| WiFi | MAC 地址 + mDNS ID | `seeed_ha_a1b2c3` |
| BLE | 蓝牙 MAC 地址 | `0B:76:DD:33:FA:21` |

即使 10 个设备烧录完全相同的代码，HA 也会将它们识别为 10 个独立设备。

⚠️ **但设备名称会相同**，可能造成混淆。建议：

**方法 1: 为每个设备设置不同名称（推荐）**

```cpp
// WiFi 设备
ha.setDeviceInfo("温湿度-客厅", "ESP32-C3", "1.0.0");  // 设备 1
ha.setDeviceInfo("温湿度-卧室", "ESP32-C3", "1.0.0");  // 设备 2

// BLE 设备
ble.begin("传感器-客厅");  // 设备 1
ble.begin("传感器-卧室");  // 设备 2
```

**方法 2: 添加后在 HA 中重命名**

在 Home Assistant 的 **设置 → 设备与服务** 中找到设备，点击设备名称即可修改。

---

## 📄 许可证

本项目采用**双重许可**：

| 组件 | 许可证 | 说明 |
|------|--------|------|
| **Home Assistant 集成** | CC BY-NC-SA 4.0 | 非商业使用，需署名，相同方式共享 |
| **Arduino 库 (WiFi/BLE)** | MIT | 自由使用，包括商业用途 |

### CC BY-NC-SA 4.0 (集成)

**您可以自由地：**
- ✅ 分享 — 在任何媒介以任何形式复制、发行本作品
- ✅ 演绎 — 修改、转换或以本作品为基础进行创作

**但需遵守以下条款：**
- 📝 **署名** — 您必须注明原始出处
- 🚫 **非商业性** — 您不得将本作品用于商业目的
- 🔄 **相同方式共享** — 修改后必须使用相同的许可协议

### MIT (Arduino 库)

Arduino 库采用 MIT 许可证，您可以自由使用、修改和分发，包括商业用途。

详见 [LICENSE](LICENSE) 文件。

---

## 🏢 关于 Seeed Studio

[Seeed Studio](https://www.seeedstudio.com/) 是一家专注于物联网和边缘计算的公司，提供各种开发板、传感器和模块。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

- GitHub: [limengdu/Seeed-Homeassistant-Discovery](https://github.com/limengdu/Seeed-Homeassistant-Discovery)
