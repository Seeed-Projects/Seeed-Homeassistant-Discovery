# Seeed HA Discovery

<p align="center">
  <img src="https://img.shields.io/badge/ESP32-C3%20%7C%20C6%20%7C%20S3-blue" alt="ESP32 Support">
  <img src="https://img.shields.io/badge/Home%20Assistant-2025.11+-green" alt="Home Assistant">
  <img src="https://img.shields.io/badge/Arduino-IDE%20%7C%20PlatformIO-orange" alt="Arduino">
  <img src="https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-yellow" alt="License">
  <img src="https://img.shields.io/badge/HACS-Custom-41BDF5" alt="HACS Custom">
</p>

**Seeed HA Discovery** 是一个让 ESP32 设备轻松连接 Home Assistant 的解决方案，由 [Seeed Studio](https://www.seeedstudio.com/) 提供。

只要你的 ESP32 设备和 Home Assistant 在同一局域网内，设备就能被自动发现并添加！

## ⚡ 一键安装

点击下方按钮，将此集成添加到你的 Home Assistant：

[![Open your Home Assistant instance and open a repository inside the Home Assistant Community Store.](https://my.home-assistant.io/badges/hacs_repository.svg)](https://my.home-assistant.io/redirect/hacs_repository/?owner=limengdu&repository=Seeed-Homeassistant-Discovery&category=integration)

> **前提条件**：你的 Home Assistant 必须已安装 [HACS](https://hacs.xyz/)

## ✨ 特点

- 🔍 **自动发现** - 设备连接 WiFi 后自动被 Home Assistant 发现
- 📡 **实时通信** - 使用 WebSocket 实时推送传感器数据
- 🎯 **简单易用** - 几行代码即可将传感器接入 HA
- 🌡️ **传感器支持** - 支持温度、湿度等各类传感器
- 📱 **状态页面** - 内置 Web 页面查看设备状态

## 📦 组件

本项目包含两部分：

1. **Home Assistant 集成** (`custom_components/seeed_ha_discovery/`)
   - 自动发现局域网内的设备
   - 接收并显示传感器数据

2. **Arduino 库** (`arduino/SeeedHADiscovery/`)
   - 用于 ESP32 设备编程
   - 简单的 API 接口

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

**方法 A: Arduino IDE**

1. 下载 `arduino/SeeedHADiscovery` 文件夹
2. 复制到 `文档/Arduino/libraries/`
3. 安装依赖库（通过库管理器）：
   - ArduinoJson (作者: Benoit Blanchon)
   - WebSockets (作者: Markus Sattler)

**方法 B: PlatformIO**

在 `platformio.ini` 中添加：

```ini
lib_deps =
    bblanchon/ArduinoJson@^7.0.0
    links2004/WebSockets@^2.4.0
```

### 3. 编写 Arduino 程序

```cpp
#include <SeeedHADiscovery.h>

// ========== 配置 ==========
const char* WIFI_SSID = "你的WiFi名称";
const char* WIFI_PASSWORD = "你的WiFi密码";

// ========== 全局变量 ==========
SeeedHADiscovery ha;
SeeedHASensor* tempSensor;
SeeedHASensor* humiditySensor;

void setup() {
    Serial.begin(115200);

    // 设置设备信息
    ha.setDeviceInfo("客厅传感器", "ESP32-C3", "1.0.0");
    ha.enableDebug(true);

    // 连接 WiFi
    if (!ha.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("WiFi 连接失败!");
        while (1) delay(1000);
    }

    // 创建温度传感器
    // 参数: ID, 名称, 设备类别, 单位
    tempSensor = ha.addSensor("temperature", "温度", "temperature", "°C");
    tempSensor->setPrecision(1);  // 1 位小数

    // 创建湿度传感器
    humiditySensor = ha.addSensor("humidity", "湿度", "humidity", "%");
    humiditySensor->setPrecision(0);  // 整数

    Serial.println("设备已就绪!");
    Serial.print("IP 地址: ");
    Serial.println(ha.getLocalIP());
}

void loop() {
    // 必须调用！处理网络事件
    ha.handle();

    // 每 5 秒更新一次数据
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        lastUpdate = millis();

        // 读取传感器（这里使用模拟数据）
        float temp = 25.0 + random(-10, 11) / 10.0;
        float humidity = 55 + random(-5, 6);

        // 更新传感器值（自动推送到 HA）
        tempSensor->setValue(temp);
        humiditySensor->setValue(humidity);

        Serial.printf("温度: %.1f°C, 湿度: %.0f%%\n", temp, humidity);
    }
}
```

### 4. 在 Home Assistant 中添加设备

设备会被自动发现！或者手动添加：

1. 进入 **设置** → **设备与服务**
2. 点击 **添加集成**
3. 搜索 **Seeed HA Discovery**
4. 输入 ESP32 的 IP 地址

## 📖 API 参考

### SeeedHADiscovery 类

| 方法 | 说明 |
|------|------|
| `setDeviceInfo(name, model, version)` | 设置设备信息 |
| `enableDebug(enable)` | 启用调试输出 |
| `begin(ssid, password)` | 连接 WiFi 并启动服务 |
| `addSensor(id, name, deviceClass, unit)` | 添加传感器 |
| `handle()` | 处理网络事件（必须在 loop 中调用）|
| `isWiFiConnected()` | 检查 WiFi 连接 |
| `isHAConnected()` | 检查 HA 连接 |
| `getLocalIP()` | 获取 IP 地址 |

### SeeedHASensor 类

| 方法 | 说明 |
|------|------|
| `setValue(value)` | 设置传感器值（自动推送到 HA）|
| `setStateClass(stateClass)` | 设置状态类别 |
| `setPrecision(precision)` | 设置小数精度 |
| `setIcon(icon)` | 设置图标（mdi:xxx 格式）|

### 常用设备类别 (deviceClass)

| 类别 | 说明 | 常用单位 |
|------|------|----------|
| `temperature` | 温度 | °C, °F |
| `humidity` | 湿度 | % |
| `pressure` | 气压 | hPa, mbar |
| `illuminance` | 光照 | lx |
| `battery` | 电池 | % |
| `voltage` | 电压 | V |
| `current` | 电流 | A |
| `power` | 功率 | W |

## 📁 项目结构

```
seeed-ha-discovery/
├── custom_components/
│   └── seeed_ha_discovery/    # Home Assistant 集成
│       ├── __init__.py        # 主入口
│       ├── manifest.json      # 集成清单
│       ├── config_flow.py     # 配置流程
│       ├── const.py           # 常量定义
│       ├── coordinator.py     # 数据协调器
│       ├── device.py          # 设备通信
│       ├── sensor.py          # 传感器平台
│       ├── strings.json       # 字符串
│       └── translations/      # 翻译文件
├── arduino/
│   └── SeeedHADiscovery/      # Arduino 库
│       ├── src/
│       │   ├── SeeedHADiscovery.h
│       │   └── SeeedHADiscovery.cpp
│       ├── examples/
│       │   └── TemperatureHumidity/
│       ├── library.json
│       └── library.properties
├── hacs.json
└── README.md
```

## 🔧 支持的硬件

| 开发板 | 状态 |
|--------|------|
| ESP32-C3 | ✅ 已测试 |
| ESP32-C6 | ✅ 已测试 |
| ESP32-S3 | ✅ 已测试 |
| ESP32 (原版) | ✅ 已测试 |

## 📝 通信协议

设备与 Home Assistant 之间使用 JSON 格式通过 WebSocket 通信：

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
      "unit_of_measurement": "°C",
      "state_class": "measurement",
      "precision": 1,
      "state": 25.5
    }
  ]
}
```

**状态更新** (设备 → HA):
```json
{
  "type": "state",
  "entity_id": "temperature",
  "state": 26.0,
  "attributes": {
    "unit_of_measurement": "°C"
  }
}
```

## ❓ 常见问题 (FAQ)

### Q1: 传感器数量有限制吗？

**没有硬编码限制**。Arduino 端使用动态数组存储传感器列表，理论上只受 ESP32 内存限制。你可以添加任意数量的传感器：

```cpp
ha.addSensor("temp1", "温度1", "temperature", "°C");
ha.addSensor("temp2", "温度2", "temperature", "°C");
ha.addSensor("humidity", "湿度", "humidity", "%");
ha.addSensor("pressure", "气压", "pressure", "hPa");
ha.addSensor("light", "光照", "illuminance", "lx");
// ... 继续添加更多
```

### Q2: 一个传感器可以发送多个数值吗？

**当前框架不支持**。每个 `SeeedHASensor` 对应**一个数值**。

如果你的传感器有多个数值，需要创建多个传感器实例：

```cpp
// 例如：环境传感器有 PM2.5, PM10, CO2, TVOC, 温度
SeeedHASensor* pm25 = ha.addSensor("pm25", "PM2.5", "pm25", "μg/m³");
SeeedHASensor* pm10 = ha.addSensor("pm10", "PM10", "pm10", "μg/m³");
SeeedHASensor* co2 = ha.addSensor("co2", "CO2", "carbon_dioxide", "ppm");
SeeedHASensor* tvoc = ha.addSensor("tvoc", "TVOC", "volatile_organic_compounds", "ppb");
SeeedHASensor* temp = ha.addSensor("temperature", "温度", "temperature", "°C");
```

### Q3: 单位可以自定义吗？有什么限制？

**单位完全由 Arduino 端定义，是纯字符串，没有任何验证**。你可以使用任何字符串作为单位，包括特殊符号：

```cpp
ha.addSensor("pm25", "PM2.5", "pm25", "μg/m³");     // ✅ 微克每立方米
ha.addSensor("temp", "温度", "temperature", "°C");  // ✅ 摄氏度
ha.addSensor("custom", "自定义", "", "个/秒");      // ✅ 任意字符串
```

单位在 Arduino 代码中设置后，会原样传递给 Home Assistant 显示。

### Q4: device_class 必须使用 Home Assistant 支持的值吗？

**建议使用，但非强制**。

- 如果使用 HA 支持的 `device_class`（如 `temperature`, `humidity`），会自动显示对应图标和格式
- 如果使用不支持的值，只会打印警告，传感器仍然可以正常创建和使用
- 你也可以留空 `device_class`，然后用 `setIcon()` 自定义图标

```cpp
// 方式1: 使用标准 device_class（推荐）
ha.addSensor("temp", "温度", "temperature", "°C");

// 方式2: 不使用 device_class，自定义图标
SeeedHASensor* custom = ha.addSensor("custom", "自定义传感器", "", "单位");
custom->setIcon("mdi:gauge");
```

### Q5: 支持哪些 device_class？

Home Assistant 传感器支持的常用 `device_class`：

| device_class | 说明 | 常用单位 |
|--------------|------|----------|
| `temperature` | 温度 | °C, °F, K |
| `humidity` | 湿度 | % |
| `pressure` | 气压 | hPa, mbar, Pa |
| `illuminance` | 光照 | lx |
| `battery` | 电池电量 | % |
| `voltage` | 电压 | V, mV |
| `current` | 电流 | A, mA |
| `power` | 功率 | W, kW |
| `energy` | 能量 | Wh, kWh |
| `carbon_dioxide` | CO2 浓度 | ppm |
| `carbon_monoxide` | CO 浓度 | ppm |
| `pm25` | PM2.5 | μg/m³ |
| `pm10` | PM10 | μg/m³ |
| `volatile_organic_compounds` | TVOC | ppb |
| `distance` | 距离 | m, cm, mm |
| `speed` | 速度 | m/s, km/h |
| `weight` | 重量 | kg, g, lb |

完整列表参考 [Home Assistant 传感器文档](https://www.home-assistant.io/integrations/sensor/#device-class)。

## 📄 许可证

本项目采用 **CC BY-NC-SA 4.0** 协议开源。

**您可以自由地：**
- ✅ 分享 — 在任何媒介以任何形式复制、发行本作品
- ✅ 演绎 — 修改、转换或以本作品为基础进行创作

**但需遵守以下条款：**
- 📝 **署名** — 您必须注明原始出处（作者、项目名、链接）
- 🚫 **非商业性** — 您不得将本作品用于商业目的
- 🔄 **相同方式共享** — 如果您修改或基于本作品创作，必须使用相同的许可协议

详见 [LICENSE](LICENSE) 文件。

## 🏢 关于 Seeed Studio

[Seeed Studio](https://www.seeedstudio.com/) 是一家专注于物联网和边缘计算的公司，提供各种开发板、传感器和模块。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

- GitHub: [limengdu/Seeed-Homeassistant-Discovery](https://github.com/limengdu/Seeed-Homeassistant-Discovery)
