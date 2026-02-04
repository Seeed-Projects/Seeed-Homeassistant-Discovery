# 土壤湿度传感器示例

实时向 Home Assistant 报告土壤湿度数据。支持真实土壤湿度传感器（如Seeed Studio土壤传感器）和模拟数据。

## 功能特性

- **土壤湿度传感器**（% 单位显示）
- **可配置更新间隔**（按需调整数据上报频率）
- **可选真实土壤湿度传感器支持**（电容式/电阻式）
- **模拟数据模式**（用于无硬件测试场景）
- **WiFi 配网**：网页配置WiFi（无需硬编码凭据）

## 硬件要求

- XIAO ESP32-C3/C5/C6/S3 或其他 ESP32 开发板
- 土壤湿度传感器（可选 - 可使用模拟数据，推荐Seeed Studio土壤传感器）

> **注意**：XIAO ESP32-C5 支持 2.4GHz 和 5GHz 双频 WiFi

###  传感器引脚连接

| 功能           | 连接引脚 | 说明                  |
|----------------|----------|-----------------------|
| 激励信号 (EX)  | **D3**   | 发送激励方波信号      |
| 模拟信号 (SIG) | **D1**   | 读取土壤电容响应电压  |
| VCC            | 3.3V     | 供电（请勿接5V）      |
| GND            | GND      | 接地                  |

### LED 指示灯连接（共阴极接法）

| LED颜色 | 引脚   | 状态定义                     |
|---------|--------|------------------------------|
| 绿色    | **D8** | 湿度 > 60%（湿润）           |
| 黄色    | **D10**| 湿度 30%-60%（适中）         |
| 红色    | **D9** | 湿度 < 30%（干旱）           |

## 软件依赖

### 需要安装的库

通过 Arduino 库管理器安装：

| 库名称 | 作者 | 说明 |
|-------|------|------|
| **ArduinoJson** | Benoit Blanchon | JSON 解析 |
| **WebSockets** | Markus Sattler | WebSocket 通信 |


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

### 2. 配置更新间隔

```cpp
const unsigned long UPDATE_INTERVAL = 5000;  // 5 秒
```

### 3. ESP32-C5 5GHz WiFi（可选）

在 ESP32-C5 上强制使用特定 WiFi 频段：
```cpp
#define WIFI_BAND_MODE WIFI_BAND_MODE_5G_ONLY  // 或 WIFI_BAND_MODE_2G_ONLY
```

### 4. 上传并连接

1. 选择开发板：**XIAO ESP32C6**（或你的开发板）
2. 上传程序
3. 打开串口监视器（115200 波特率）
4. 在 Home Assistant 中添加设备

## Home Assistant 设置

1. 进入 **设置** → **设备与服务** → **添加集成**
2. 搜索 **Seeed HA Discovery**
3. 输入设备 IP 地址
4. 将出现一个传感器：
   - 土壤湿度

## 创建的实体

| 实体 | 类型 | 单位 | 精度 |
| :--- | :--- | :--- | :--- |
| Soil Moisture | 传感器 | % | 整数 |

## 模拟数据模式

当未定义 USE_SOIL_SENSOR 时，设备将自动启用模拟数据模式：

土壤湿度：在 0-100% 之间波动（模拟干旱到过湿的全范围）

此模式适用于没有真实传感器的场景，方便代码测试与 Home Assistant 联动调试。

## 设备状态页面

访问设备状态：`http://<设备IP>/`

## 故障排除

### HA 中数值不更新
- 检查 WiFi 连接
- 验证设备已连接到 HA
- 查看串口监视器的错误信息

### 无法访问配网页面
- 确保已连接到设备的 AP
- 尝试手动访问 `http://192.168.4.1`

## 许可证

SeeedHADiscovery 库的一部分。
