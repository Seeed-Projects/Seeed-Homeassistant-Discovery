# XIAO ESP32-C3 SCD41 空气质量显示示例

本示例使用 XIAO ESP32-C3 读取 Grove SCD41 的二氧化碳、温度和湿度数据，在 1.47 英寸 172×320 ST7789 SPI LCD 上显示横屏空气质量界面，并通过 SeeedHADiscovery 接入 Home Assistant。

固件采用项目通用的网页配网方式：首次启动会建立热点并打开配网页面；配网成功后会保存 WiFi 信息，以后上电自动连接。运行时长按 XIAO 的 BOOT 键 6 秒，可重新进入配网模式。

## 功能

- 每约 5 秒读取一组 SCD41 数据
- 显示 CO₂、温度和相对湿度
- 按 CO₂ 浓度切换绿色、黄色、橙色和红色状态
- 采用局部区域刷新，保持界面稳定
- 显示传感器预热和接线错误状态
- 传感器恢复连接后每 5 秒自动重试初始化
- 首次启动提供 `Seeed_AirMonitor_AP` 配网热点
- 保存 WiFi 信息并在后续启动时自动连接
- 长按 BOOT 键 6 秒重新配网
- 向 Home Assistant 发布 CO₂、温度和湿度实体
- 在屏幕右上角显示配网、WiFi 和 Home Assistant 连接状态

## 硬件

- Seeed Studio XIAO ESP32-C3
- 1.47 inch LCD SPI Display，172×320，ST7789V3
- Grove - CO₂ & Temperature & Humidity Sensor (SCD41)
- Grove 转杜邦线或其他适合的连接线

## 接线

### LCD

| LCD 引脚 | XIAO ESP32-C3 | 功能 |
|---|---|---|
| VCC | 3V3 | 电源 |
| GND | GND | 地线 |
| DIN | D10 | SPI MOSI |
| CLK | D8 | SPI 时钟 |
| CS | D1 | 片选 |
| DC | D3 | 数据/命令选择 |
| RST | D0 | 屏幕复位 |
| BL | D6 | 背光控制 |

### SCD41

| SCD41 引脚 | XIAO ESP32-C3 | 功能 |
|---|---|---|
| VCC | 3V3 | 电源 |
| GND | GND | 地线 |
| SDA | D4 | I²C 数据 |
| SCL | D5 | I²C 时钟 |

## 软件依赖

在 Arduino IDE 的“库管理器”中安装：

| 库 | 已验证版本 |
|---|---|
| Adafruit GFX Library | 1.12.6 |
| Adafruit ST7735 and ST7789 Library | 1.11.0 |
| Sensirion I2C SCD4x | 1.1.0 |
| Sensirion Core | 0.7.3 |
| ArduinoJson | 7.4.2 |
| WebSockets | 2.7.1 |

`SeeedHADiscovery` 直接使用本仓库 `arduino/SeeedHADiscovery` 目录中的版本。

开发板支持包使用 `esp32 by Espressif Systems` 3.3.10。

## 上传和运行

1. 完成 LCD 与 SCD41 接线。
2. 在 Arduino IDE 中选择 `XIAO_ESP32C3`。
3. 打开 `XIAO_ESP32C3_SCD41_AirQuality_Display.ino`。
4. 选择 XIAO 对应的串口并上传程序。
5. 打开串口监视器，将波特率设置为 `115200`。
6. 首次启动时，使用手机或电脑连接 `Seeed_AirMonitor_AP`。
7. 浏览器访问 `http://192.168.4.1`，选择 2.4 GHz WiFi 并输入密码。
8. 设备保存配置并重启，随后自动连接 WiFi。

也可以在仓库根目录执行编译检查：

```bash
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 arduino/SeeedHADiscovery/examples/XIAO_ESP32C3_SCD41_AirQuality_Display
```

也可以使用项目的[网页烧录器](https://seeed-projects.github.io/Seeed-Homeassistant-Discovery/flasher/)，选择 `SCD41 空气质量显示` 卡片直接安装预编译固件。

## 接入 Home Assistant

1. 确认设备和 Home Assistant 连接到同一个局域网。
2. 在 Home Assistant 中打开 `设置 → 设备与服务 → 添加集成`。
3. 搜索并选择 `Seeed HA Discovery`。
4. 输入串口监视器中显示的设备 IP 地址。
5. 完成配置后，设备页会出现下列 3 个实体：

| 实体 | 单位 | 说明 |
|---|---|---|
| Carbon Dioxide | ppm | SCD41 二氧化碳浓度 |
| Temperature | °C | SCD41 温度 |
| Humidity | % | SCD41 相对湿度 |

屏幕右上角状态说明：

| 状态 | 含义 |
|---|---|
| `SETUP` | 配网热点和网页正在运行 |
| `OFFLINE` | WiFi 当前未连接 |
| `WIFI` | WiFi 已连接，等待 Home Assistant 连接 |
| `HA ONLINE` | Home Assistant 已连接 |

## 正常现象

启动后，屏幕会依次显示：

1. `INITIALIZING SCD41`
2. `WARMING UP`
3. CO₂ 大号数值、空气质量等级、温度和湿度

串口监视器会看到类似输出：

```text
Air quality display starting
SCD41 serial number: 0x...
SCD41 periodic measurement started
CO2: 650 ppm, Temperature: 24.3 C, Humidity: 48.1 %
```

首次配网时还会看到：

```text
WiFi provisioning is active
Connect to access point: Seeed_AirMonitor_AP
Open: http://192.168.4.1
```

SCD41 的第一组周期测量通常约 5 秒后出现。持续运行约 2 小时后，CO₂ 测量值更适合用于稳定性和准确度评估。

## 界面分级

| CO₂ 浓度 | 显示状态 | 颜色 |
|---|---|---|
| ≤ 800 ppm | GOOD | 绿色 |
| 801–1000 ppm | FAIR | 黄色 |
| 1001–1500 ppm | POOR | 橙色 |
| > 1500 ppm | BAD | 红色 |

这些阈值用于示例界面分级，可以在 `classifyAirQuality()` 中按应用场景调整。

## 验证项目

### 主流程

1. 在正常室内环境启动设备。
2. 等待屏幕从 `WARMING UP` 切换到数值界面。
3. 对照串口输出检查 CO₂、温度和湿度是否与屏幕一致。
4. 保持运行，确认界面约每 5 秒更新且没有整屏闪烁。
5. 完成热点配网，确认右上角从 `SETUP` 变为 `WIFI`。
6. 在 Home Assistant 添加设备，确认状态变为 `HA ONLINE`，3 个实体数值与屏幕一致。

### 边缘情况

- 断开 SCD41：屏幕显示 `SENSOR ERROR`，串口输出对应的 I²C 错误。
- 重新连接 SCD41：设备每 5 秒尝试初始化，恢复后重新显示 `WARMING UP` 和测量值。
- CO₂ 跨越分级阈值：状态文字与颜色同步变化。
- 测量值达到四位数：CO₂ 数字保持在主卡片范围内。
- WiFi 不可用：屏幕继续显示本地传感器数据，右上角显示 `OFFLINE`。
- Home Assistant 未添加或停止运行：设备保持 WiFi 连接，右上角显示 `WIFI`。
- 需要更换 WiFi：设备正常运行时长按 BOOT 键 6 秒，重启后连接 `Seeed_AirMonitor_AP` 重新配置。

## 参考资料

- [1.47 inch LCD SPI Display Wiki](https://wiki.seeedstudio.com/1-47inch_lcd_spi_display/)
- [Grove SCD41 Wiki](https://wiki.seeedstudio.com/Grove-CO2_%26_Temperature_%26_Humidity_Sensor-SCD41/)
- [Sensirion Arduino SCD4x Library](https://github.com/Sensirion/arduino-i2c-scd4x)
