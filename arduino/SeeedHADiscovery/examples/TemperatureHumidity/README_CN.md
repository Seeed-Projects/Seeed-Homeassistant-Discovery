# 温湿度传感器示例

实时向 Home Assistant 报告温度和湿度数据。支持真实 DHT22 传感器和模拟数据。

## 功能特性

- 温度传感器（°C）
- 湿度传感器（%）
- 可配置更新间隔
- 可选 DHT22 传感器支持
- 模拟数据模式用于测试
- **WiFi 配网**：网页配置 WiFi（无需硬编码凭据）

## 硬件要求

- XIAO ESP32-C3/C5/C6/S3 或其他 ESP32 开发板
- DHT22 传感器（可选 - 可使用模拟数据）

> **注意**：XIAO ESP32-C5 支持 2.4GHz 和 5GHz 双频 WiFi

### DHT22 接线

| DHT22 引脚 | 连接 |
|-----------|------|
| VCC | 3.3V |
| GND | GND |
| DATA | D2（可配置） |

## 软件依赖

### 需要安装的库

通过 Arduino 库管理器安装：

| 库名称 | 作者 | 说明 |
|-------|------|------|
| **ArduinoJson** | Benoit Blanchon | JSON 解析 |
| **WebSockets** | Markus Sattler | WebSocket 通信 |
| **DHT sensor library** | Adafruit | DHT22 支持（如果使用真实传感器） |

### SeeedHADiscovery 库

从 [GitHub](https://github.com/limengdu/SeeedHADiscovery) 手动安装。

## 快速开始

### 1. WiFi 配置

**方式 A：WiFi 配网（推荐）**

WiFi 配网默认启用。首次启动时：
1. 设备创建 AP 热点：`Seeed_TempHum_AP`
2. 将手机/电脑连接到此 AP
3. 浏览器自动打开，或手动访问 `http://192.168.4.1`
4. 选择你的 WiFi 网络并输入密码
5. 设备重启并连接到你的 WiFi

**方式 B：硬编码凭据**

如需使用硬编码凭据：
```cpp
#define USE_WIFI_PROVISIONING false
const char* WIFI_SSID = "你的WiFi名称";
const char* WIFI_PASSWORD = "你的WiFi密码";
```

### 2. 启用 DHT22（可选）

要使用真实 DHT22 传感器，取消注释：
```cpp
#include <DHT.h>
#define USE_DHT_SENSOR
```

### 3. 配置更新间隔

```cpp
const unsigned long UPDATE_INTERVAL = 5000;  // 5 秒
```

### 4. ESP32-C5 5GHz WiFi（可选）

在 ESP32-C5 上强制使用特定 WiFi 频段：
```cpp
#define WIFI_BAND_MODE WIFI_BAND_MODE_5G_ONLY  // 或 WIFI_BAND_MODE_2G_ONLY
```

### 5. 上传并连接

1. 选择开发板：**XIAO ESP32C6**（或你的开发板）
2. 上传程序
3. 打开串口监视器（115200 波特率）
4. 在 Home Assistant 中添加设备

## Home Assistant 设置

1. 进入 **设置** → **设备与服务** → **添加集成**
2. 搜索 **Seeed HA Discovery**
3. 输入设备 IP 地址
4. 将出现两个传感器：
   - 温度
   - 湿度

## 创建的实体

| 实体 | 类型 | 单位 | 精度 |
|-----|------|------|------|
| Temperature | 传感器 | °C | 1 位小数 |
| Humidity | 传感器 | % | 整数 |

## 模拟数据模式

当未定义 `USE_DHT_SENSOR` 时：
- 温度：在 20-30°C 之间波动
- 湿度：在 40-70% 之间波动

这对于没有硬件的测试非常有用。

## 配置选项

| 选项 | 默认值 | 说明 |
|-----|-------|------|
| `DHT_PIN` | D2 | DHT22 数据引脚 |
| `DHT_TYPE` | DHT22 | 传感器类型（DHT11 或 DHT22） |
| `UPDATE_INTERVAL` | 5000ms | 数据上报间隔 |

## 设备状态页面

访问设备状态：`http://<设备IP>/`

## 故障排除

### DHT22 读取失败
- 检查接线连接
- 确认传感器是 DHT22 而不是 DHT11
- 确保 3.3V 供电

### HA 中数值不更新
- 检查 WiFi 连接
- 验证设备已连接到 HA
- 查看串口监视器的错误信息

### 无法访问配网页面
- 确保已连接到设备的 AP
- 尝试手动访问 `http://192.168.4.1`

## 许可证

SeeedHADiscovery 库的一部分。
