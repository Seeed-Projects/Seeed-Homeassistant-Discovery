# XIAO ESP32-C3 SCD41 空气质量显示示例

本示例使用 XIAO ESP32-C3 读取 Grove SCD41 的二氧化碳、温度和湿度数据，并在 1.47 英寸 172×320 ST7789 SPI LCD 上显示横屏空气质量界面。

当前示例专注于本地传感器读取和屏幕显示。屏幕与数值完成实物验证后，可在同一代码结构上继续加入 SeeedHADiscovery WiFi 和 Home Assistant 状态显示。

## 功能

- 每约 5 秒读取一组 SCD41 数据
- 显示 CO₂、温度和相对湿度
- 按 CO₂ 浓度切换绿色、黄色、橙色和红色状态
- 采用局部区域刷新，保持界面稳定
- 显示传感器预热和接线错误状态
- 传感器恢复连接后每 5 秒自动重试初始化

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

开发板支持包使用 `esp32 by Espressif Systems` 3.3.10。

## 上传和运行

1. 完成 LCD 与 SCD41 接线。
2. 在 Arduino IDE 中选择 `XIAO_ESP32C3`。
3. 打开 `XIAO_ESP32C3_SCD41_AirQuality_Display.ino`。
4. 选择 XIAO 对应的串口并上传程序。
5. 打开串口监视器，将波特率设置为 `115200`。

也可以在仓库根目录执行编译检查：

```bash
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 arduino/SeeedHADiscovery/examples/XIAO_ESP32C3_SCD41_AirQuality_Display
```

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

### 边缘情况

- 断开 SCD41：屏幕显示 `SENSOR ERROR`，串口输出对应的 I²C 错误。
- 重新连接 SCD41：设备每 5 秒尝试初始化，恢复后重新显示 `WARMING UP` 和测量值。
- CO₂ 跨越分级阈值：状态文字与颜色同步变化。
- 测量值达到四位数：CO₂ 数字保持在主卡片范围内。

## 参考资料

- [1.47 inch LCD SPI Display Wiki](https://wiki.seeedstudio.com/1-47inch_lcd_spi_display/)
- [Grove SCD41 Wiki](https://wiki.seeedstudio.com/Grove-CO2_%26_Temperature_%26_Humidity_Sensor-SCD41/)
- [Sensirion Arduino SCD4x Library](https://github.com/Sensirion/arduino-i2c-scd4x)

