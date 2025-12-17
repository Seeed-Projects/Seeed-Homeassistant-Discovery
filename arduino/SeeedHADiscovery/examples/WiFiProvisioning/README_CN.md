# WiFi 配网示例

本示例展示 SeeedHADiscovery 库的网页配网功能，提供强制门户用于 WiFi 配置。

## 功能特点

- **网页配置**: 通过任何设备的浏览器配置 WiFi
- **强制门户**: 连接到 AP 时自动打开配置页面
- **网络扫描**: 扫描并显示可用的 WiFi 网络
- **凭据持久化**: 保存的凭据在重启后保持有效
- **刷新网络**: 点击刷新按钮重新扫描网络
- **现代 UI**: 美观、响应式的网页界面
- **重置按钮**: 长按按钮 6 秒清除凭据并重新进入配网模式
- **ESP32-C5 5GHz 支持**: 配置双频 WiFi 的频段模式

## 工作原理

1. **首次启动**（无保存的凭据）:
   - 设备创建 AP 热点（默认：`Seeed_HA_Device_AP`）
   - 红色 LED 指示灯表示配网模式

2. **WiFi 配置**:
   - 将手机/电脑连接到 AP
   - 浏览器自动打开强制门户，或手动访问 `http://192.168.4.1`
   - 从列表中选择您的 WiFi 网络
   - 输入密码并点击连接
   - 设备保存凭据并重启

3. **后续启动**:
   - 设备使用保存的凭据自动连接
   - 如果连接失败，回退到 AP 模式重新配置

## 硬件要求

- 任何 ESP32 开发板（ESP32、ESP32-C3、ESP32-C5、ESP32-C6、ESP32-S3 等）

> **注意**：XIAO ESP32-C5 支持 2.4GHz 和 5GHz 双频 WiFi

## 软件依赖

- SeeedHADiscovery 库
- ArduinoJson（作者：Benoit Blanchon）
- WebSockets（作者：Markus Sattler）

## 使用方法

### 基本用法

```cpp
#include <SeeedHADiscovery.h>

SeeedHADiscovery ha;

void setup() {
    Serial.begin(115200);
    
    ha.setDeviceInfo("我的设备", "ESP32", "1.0.0");
    ha.enableDebug(true);
    
    // 使用 WiFi 配网启动
    bool connected = ha.beginWithProvisioning("我的设备_AP");
    
    if (!connected) {
        // 处于 AP 模式，等待配置
        return;
    }
    
    // WiFi 已连接，继续设置...
}

void loop() {
    ha.handle();  // 必须调用！
    
    if (ha.isProvisioningActive()) {
        // 仍处于配网模式
        delay(10);
        return;
    }
    
    // 正常操作...
}
```

### 自定义 AP 名称

```cpp
// 使用自定义 AP 名称
ha.beginWithProvisioning("我的自定义_AP_名称");
```

### ESP32-C5 5GHz WiFi 配置

```cpp
// 在 ESP32-C5 上强制使用特定频段模式
#define WIFI_BAND_MODE WIFI_BAND_MODE_5G_ONLY  // 或 WIFI_BAND_MODE_2G_ONLY 或 WIFI_BAND_MODE_AUTO

// 在 setup() 中，beginWithProvisioning 之前：
#if defined(WIFI_BAND_MODE) && defined(CONFIG_SOC_WIFI_SUPPORT_5G)
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 2)
        WiFi.setBandMode(WIFI_BAND_MODE);
    #endif
#endif
```

### 清除保存的凭据

```cpp
// 要重新配置 WiFi，清除保存的凭据
ha.clearWiFiCredentials();
// 设备将在下次启动时进入 AP 模式
```

### 启用重置按钮

```cpp
// 在 GPIO0（大多数 ESP32 开发板上的 BOOT 按钮）上启用重置按钮
// 长按 6 秒清除凭据并启动 AP 模式
ha.enableResetButton(0);  // GPIO0, 低电平有效（默认）

// 对于高电平有效的按钮：
ha.enableResetButton(5, false);  // GPIO5, 高电平有效
```

### 直接使用 SeeedWiFiProvisioning

如需更多控制，可以直接使用 `SeeedWiFiProvisioning` 类：

```cpp
#include <SeeedWiFiProvisioning.h>

SeeedWiFiProvisioning provisioning;

void setup() {
    provisioning.setAPSSID("我的_AP");
    provisioning.setConnectTimeout(20000);  // 20 秒
    provisioning.enableDebug(true);
    
    // 注册回调
    provisioning.onWiFiConnected([]() {
        Serial.println("WiFi 已连接！");
    });
    
    provisioning.onAPStarted([]() {
        Serial.println("AP 模式已启动！");
    });
    
    provisioning.begin();
}

void loop() {
    provisioning.handle();
}
```

## 网页界面

强制门户提供现代、响应式的界面：

- **网络列表**: 显示可用的 WiFi 网络及信号强度
- **安全指示器**: 密码保护网络显示锁图标
- **信号条**: 可视化信号强度指示器
- **加密类型**: 显示 WPA2、WPA3 等
- **刷新按钮**: 重新扫描网络
- **重置按钮**: 清除保存的凭据

## API 参考

### SeeedHADiscovery 方法

| 方法 | 描述 |
|------|------|
| `beginWithProvisioning(apSSID)` | 使用 WiFi 配网支持启动 |
| `isProvisioningActive()` | 检查 AP 模式是否激活 |
| `clearWiFiCredentials()` | 清除保存的 WiFi 凭据 |
| `enableResetButton(pin, activeLow)` | 启用重置按钮（长按 6 秒重置 WiFi） |
| `disableResetButton()` | 禁用重置按钮 |
| `getProvisioning()` | 获取 SeeedWiFiProvisioning 实例 |

### SeeedWiFiProvisioning 方法

| 方法 | 描述 |
|------|------|
| `setAPSSID(ssid)` | 设置 AP 热点名称 |
| `setAPPassword(password)` | 设置 AP 密码（空 = 开放） |
| `setConnectTimeout(ms)` | 设置连接超时 |
| `enableDebug(enable)` | 启用/禁用调试输出 |
| `begin()` | 使用保存的凭据或 AP 模式启动 |
| `begin(ssid, password)` | 使用指定凭据启动 |
| `startAPMode()` | 强制启动 AP 模式 |
| `stopAPMode()` | 停止 AP 模式 |
| `handle()` | 处理配网任务（在 loop 中调用） |
| `isWiFiConnected()` | 检查 WiFi 连接状态 |
| `isAPModeActive()` | 检查 AP 模式是否激活 |
| `hasCredentials()` | 检查是否有保存的凭据 |
| `getSavedSSID()` | 获取保存的 SSID |
| `clearCredentials()` | 清除保存的凭据 |
| `scanNetworks()` | 扫描 WiFi 网络 |
| `enableResetButton(pin, activeLow)` | 启用重置按钮（长按 6 秒） |
| `disableResetButton()` | 禁用重置按钮 |

## 故障排除

### 设备不创建 AP

- 检查是否已保存 WiFi 凭据
- 尝试清除凭据：`ha.clearWiFiCredentials()`

### 无法连接到 AP

- 确保距离设备足够近
- 尝试关闭手机的移动数据
- 某些设备可能需要手动连接到 AP

### 强制门户不自动打开

- 手动访问 `http://192.168.4.1`
- 尝试清除浏览器缓存

### 配置后连接失败

- 验证密码是否正确
- 检查路由器是否可达
- 尝试靠近路由器

### ESP32-C5 找不到 5GHz 网络

- 确保正确设置了 `WIFI_BAND_MODE`
- 验证路由器在 5GHz 频段广播
- 检查 ESP-IDF 版本是否支持 5GHz（需要 5.4.2+）

## 许可证

MIT 许可证 - 详见 LICENSE 文件。
