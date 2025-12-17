# LED 开关示例

通过 WiFi 从 Home Assistant 控制 LED。本示例演示基本的开关实体创建和双向控制。

## 功能特性

- 从 Home Assistant 控制 LED 开关
- 实时状态反馈
- 支持板载和外接 LED
- 可配置 LED 极性（高电平/低电平有效）
- **WiFi 配网**：网页配置 WiFi（无需硬编码凭据）

## 硬件要求

- XIAO ESP32-C3/C5/C6/S3 或其他 ESP32 开发板
- LED（板载或外接）

> **注意**：XIAO ESP32-C5 支持 2.4GHz 和 5GHz 双频 WiFi

### LED 引脚参考

| 开发板 | 板载 LED 引脚 | 说明 |
|-------|--------------|------|
| XIAO ESP32-S3 | GPIO21 | 有用户 LED |
| XIAO ESP32-C6 | GPIO15 | 有用户 LED |
| XIAO ESP32-C3 | 无 | **没有用户 LED** - 需要外接 LED |
| XIAO ESP32-C5 | GPIO27 | 有用户 LED，支持 5GHz WiFi |

### 外接 LED 接线

如果使用外接 LED：

```
GPIO 引脚 ---[220Ω]--- LED (+) --- LED (-) --- GND
```

| 组件 | 连接 |
|-----|------|
| LED 正极（长脚） | 通过 220Ω 电阻连接 GPIO |
| LED 负极（短脚） | GND |

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
1. 设备创建 AP 热点：`Seeed_LED_AP`
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

### 2. 配置 LED（如果使用外接）

对于 XIAO ESP32-C3 或外接 LED：
```cpp
#undef LED_BUILTIN
#define LED_BUILTIN D0  // 你的外接 LED 引脚
```

### 3. 配置 LED 极性

```cpp
// true = 低电平有效（XIAO 系列 - LOW 点亮 LED）
// false = 高电平有效（外接 LED - HIGH 点亮 LED）
#define LED_ACTIVE_LOW true
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
4. 使用显示的 IP 在 Home Assistant 中添加设备

## Home Assistant 设置

1. 进入 **设置** → **设备与服务** → **添加集成**
2. 搜索 **Seeed HA Discovery**
3. 输入设备 IP 地址
4. 将出现一个 "LED" 开关实体

## 创建的实体

| 实体 | 类型 | 图标 |
|-----|------|------|
| LED | 开关 | `mdi:led-on` |

## 设备状态页面

访问设备状态：`http://<设备IP>/`

## 故障排除

### LED 开关不正常
- 检查 `LED_ACTIVE_LOW` 设置是否与你的 LED 电路匹配
- 如果使用外接 LED，验证接线
- 确保 GPIO 引脚对应你的开发板

### WiFi 连接失败
- 验证 SSID 和密码
- 检查 WiFi 信号强度
- 如果连接失败，LED 会闪烁

### 无法访问配网页面
- 确保已连接到设备的 AP
- 尝试手动访问 `http://192.168.4.1`

## 许可证

SeeedHADiscovery 库的一部分。
