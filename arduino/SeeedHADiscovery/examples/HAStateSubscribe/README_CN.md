# HA 状态订阅示例

在设备上接收和显示来自 Home Assistant 的实体状态。非常适合创建仪表板显示或响应其他 HA 实体的变化。

## 功能特性

- 订阅任意 Home Assistant 实体
- 两种工作模式：推送模式（事件驱动）和轮询模式（定时器）
- 实时状态更新
- 访问实体属性（友好名称、单位、设备类别）
- 通过 HA 界面动态配置实体
- **WiFi 配网**：网页配置 WiFi（无需硬编码凭据）

## 使用场景

- 在屏幕上显示传感器值
- 响应其他 HA 实体的变化
- 创建仪表板设备
- 构建自动化触发器

## 硬件要求

- XIAO ESP32-C3/C5/C6/S3 或其他 ESP32 开发板
- 可选：用于显示状态的屏幕

> **注意**：XIAO ESP32-C5 支持 2.4GHz 和 5GHz 双频 WiFi

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
1. 设备创建 AP 热点：`Seeed_StateDisplay_AP`
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

### 2. ESP32-C5 5GHz WiFi（可选）

在 ESP32-C5 上强制使用特定 WiFi 频段：
```cpp
#define WIFI_BAND_MODE WIFI_BAND_MODE_5G_ONLY  // 或 WIFI_BAND_MODE_2G_ONLY
```

### 3. 上传并连接

1. 上传程序到设备
2. 打开串口监视器（115200 波特率）
3. 在 Home Assistant 中添加设备

### 4. 在 HA 中配置订阅

1. 在 **设置** → **设备与服务** 中找到你的设备
2. 点击设备上的 **配置**
3. 选择你想订阅的实体
4. 保存配置

## 两种工作模式

### 模式 1：推送模式（事件驱动）- 推荐

```cpp
ha.onHAState([](const char* entityId, const char* state, JsonObject& attrs) {
    Serial.print("实体: ");
    Serial.println(entityId);
    Serial.print("状态: ");
    Serial.println(state);
});
```

- HA 在实体变化时自动推送更新
- 适用于：实时响应、日志记录、警报

### 模式 2：轮询模式（定时器）

```cpp
SeeedHAState* temp = ha.getHAState("sensor.temperature");
if (temp && temp->hasValue()) {
    float value = temp->getFloat();
    // 使用该值
}
```

- 按自己的时间表读取存储的状态
- 适用于：屏幕刷新、定期显示更新

## SeeedHAState 可用方法

| 方法 | 返回类型 | 说明 |
|-----|---------|------|
| `getString()` | String | 字符串形式的状态 |
| `getFloat()` | float | 数值形式的状态 |
| `getBool()` | bool | 布尔形式的状态 |
| `hasValue()` | bool | 检查是否有值 |
| `getEntityId()` | String | 实体 ID |
| `getFriendlyName()` | String | 显示名称 |
| `getUnit()` | String | 测量单位 |
| `getDeviceClass()` | String | 设备类别 |

## 示例：仪表板显示

```cpp
void refreshScreen() {
    for (const auto& pair : ha.getHAStates()) {
        SeeedHAState* state = pair.second;
        if (state->hasValue()) {
            String line = state->getFriendlyName() + ": " + state->getString();
            // 绘制到屏幕
        }
    }
}
```

## 示例：条件逻辑

```cpp
SeeedHAState* temp = ha.getHAState("sensor.temperature");
if (temp && temp->hasValue() && temp->getFloat() > 28.0) {
    Serial.println("警告：温度过高！");
}
```

## 配置选项

| 选项 | 默认值 | 说明 |
|-----|-------|------|
| `PRINT_INTERVAL` | 5000ms | 状态打印间隔 |

## 故障排除

### 没有实体出现
- 在 HA 设备设置中配置订阅
- 查看串口监视器的连接状态

### 状态不更新
- 验证 HA WebSocket 连接
- 检查实体是否存在于 HA 中

### 无法访问配网页面
- 确保已连接到设备的 AP
- 尝试手动访问 `http://192.168.4.1`

## 许可证

SeeedHADiscovery 库的一部分。
