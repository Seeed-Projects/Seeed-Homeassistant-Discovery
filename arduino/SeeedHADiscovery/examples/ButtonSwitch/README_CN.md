# 按钮开关示例

多击按钮检测示例，在 Home Assistant 中创建三个独立开关，分别由不同的按键模式触发。

## 功能特性

- **单击**：切换开关 1
- **双击**：切换开关 2
- **长按（>1秒）**：切换开关 3
- 双向控制（按钮 ↔ Home Assistant）
- 实时状态同步
- **WiFi 配网**：网页配置 WiFi（无需硬编码凭据）

## 硬件要求

- XIAO ESP32-C3/C5/C6/S3 或其他 ESP32 开发板
- 按钮（内置上拉电阻或外接上拉电阻）

> **注意**：XIAO ESP32-C5 支持 2.4GHz 和 5GHz 双频 WiFi

### 按钮接线

| 按钮引脚 | 连接 |
|---------|------|
| 一端 | GPIO D1（默认） |
| 另一端 | GND |

默认已启用内部上拉电阻。

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
1. 设备创建 AP 热点：`Seeed_Button_AP`
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

### 2. 配置按钮引脚（可选）

```cpp
#define BUTTON_PIN D1  // 根据需要修改
```

### 3. ESP32-C5 5GHz WiFi（可选）

在 ESP32-C5 上强制使用特定 WiFi 频段：
```cpp
#define WIFI_BAND_MODE WIFI_BAND_MODE_5G_ONLY  // 或 WIFI_BAND_MODE_2G_ONLY
```

### 4. 上传并连接

1. 选择开发板：**XIAO ESP32C6**（或你的开发板）
2. 上传程序
3. 打开串口监视器（115200 波特率）查看 IP 地址
4. 在 Home Assistant 中添加设备

## Home Assistant 设置

1. 进入 **设置** → **设备与服务** → **添加集成**
2. 搜索 **Seeed HA Discovery**
3. 输入设备 IP 地址
4. 将出现三个开关：
   - 单击开关
   - 双击开关
   - 长按开关

## 按钮时序参数

| 参数 | 默认值 | 说明 |
|-----|-------|------|
| `LONG_PRESS_TIME` | 1000ms | 长按阈值 |
| `DOUBLE_CLICK_TIME` | 300ms | 双击间隔 |

## 创建的实体

| 实体 | 类型 | 图标 |
|-----|------|------|
| Single Click | 开关 | `mdi:gesture-tap` |
| Double Click | 开关 | `mdi:gesture-double-tap` |
| Long Press | 开关 | `mdi:gesture-tap-hold` |

## 工作原理

1. 按下按钮 → 检测按键类型（单击/双击/长按）
2. 切换对应开关状态
3. 状态自动同步到 Home Assistant
4. Home Assistant 也可以远程控制开关

## 故障排除

### 按钮无响应
- 检查接线（按钮应连接 GPIO 到 GND）
- 确认上拉电阻已启用
- 查看串口监视器的调试输出

### WiFi 连接失败
- 验证 SSID 和密码
- 检查 WiFi 信号强度
- 如使用配网，确保设备处于 AP 模式

### 无法访问配网页面
- 确保已连接到设备的 AP
- 尝试手动访问 `http://192.168.4.1`

## 许可证

SeeedHADiscovery 库的一部分。
