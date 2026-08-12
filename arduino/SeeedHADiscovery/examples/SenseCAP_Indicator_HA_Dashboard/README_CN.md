# SenseCAP Indicator HA 看板

这是通用 SenseCAP Indicator Home Assistant 看板的 Arduino 显示与触摸
基础示例。程序会初始化 480 x 480 ST7701 屏幕，运行一套使用 LVGL 构建的
会议室仪表盘，并读取 FT5x06 触摸控制器。

## 所需硬件

- SenseCAP Indicator D1 或 D1S
- 连接原生 USB 接口的 USB-C 数据线

## Arduino 依赖

- ESP32 开发板包 3.1.2
- GFX Library for Arduino 1.5.3
- PCA95x5 0.1.3
- LVGL 9.2.2
- Seeed Home Assistant Discovery 1.6.1
- ArduinoJson 7.2.1
- WebSockets 2.7.1

这些依赖可以通过 Arduino 开发板管理器和库管理器安装。安装
Seeed Home Assistant Discovery 时，Arduino IDE 会同时安装 ArduinoJson 和
WebSockets。FT5x06 触摸驱动已经包含在本示例中。

## 开发板设置

选择 `ESP32S3 Dev Module`，并使用以下选项：

- Flash Size: `8MB (64Mb)`
- Partition Scheme: `8M with spiffs (3MB APP/1.5MB SPIFFS)`
- PSRAM: `OPI PSRAM`
- USB CDC On Boot: `Enabled`

`Partition Scheme` 必须选择上面的 3MB 应用分区。Arduino IDE 默认的
`Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)` 只给程序约 1.25MB
空间，无法容纳同时包含 LVGL、Wi-Fi 配网和 Home Assistant 通信的固件。

## 使用 Arduino CLI 编译

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi,CDCOnBoot=cdc" \
  arduino/SeeedHADiscovery/examples/SenseCAP_Indicator_HA_Dashboard
```

上传后，屏幕会显示 106 会议室仪表盘。Overview 页面包含人员状态、
Home Assistant 状态、CO2、温度、湿度、人体传感器电量、电动窗户、电视电源
和本月用电。Energy 页面还会显示今日用电和电视实时功率。

底部导航可以切换 Overview、Controls 和 Energy 页面。点击窗户与电视卡片时，
卡片会显示触摸反馈和 `HA control comes next` 提示，但设备状态只跟随
Home Assistant 下发的数据变化。点击 `Leave Room` 后会先弹出确认窗口。
控制命令将在下一阶段接入。

## Wi-Fi 热点配网

第一次启动或没有已保存的 Wi-Fi 信息时，屏幕会显示 `WiFi Setup`：

1. 使用手机或电脑连接热点 `SenseCAP_Indicator_AP`。
2. 打开 `http://192.168.4.1`；大多数手机也会自动弹出配网页面。
3. 选择目标 Wi-Fi，输入密码并保存。
4. 设备自动重启并连接 Wi-Fi。顶部状态会依次显示 `HA Waiting` 和
   `HA Online`。

Wi-Fi 信息会保存在设备中，正常重启后会自动重连。

启动网络配网前，程序会先让 LVGL 完成首帧绘制。Wi-Fi 连接和热点扫描运行在
ESP32-S3 的另一个 CPU 核心上，因此屏幕刷新和触摸处理会持续运行。
示例内置的 RGB 显示封装会让显示控制器直接读取 PSRAM 帧缓冲，使屏幕持续
显示时也能稳定完成 Wi-Fi 信息的读取与保存。

## Home Assistant 实体订阅

设备接入 Seeed HA Discovery 集成后，在 Home Assistant 的设备配置页面选择
以下实体。插件会把实体状态推送到 Indicator：

| 显示内容 | Home Assistant 实体 |
| --- | --- |
| 人员状态 | `sensor.xiaomi_03_1163_occupancy_sensor_2` |
| 人体传感器电量 | `sensor.xiaomi_03_1163_battery_level_2` |
| CO2 | `sensor.scd41_air_quality_monitor_carbon_dioxide` |
| 温度 | `sensor.scd41_air_quality_monitor_temperature` |
| 湿度 | `sensor.scd41_air_quality_monitor_humidity` |
| 电动窗户 | `cover.liyan_liyan_4d07_window_opener_2` |
| 电视电源 | `switch.cuco_v3_3244_switch_2` |
| 电视实时功率 | `sensor.cuco_v3_3244_electric_power_2` |
| 今日用电 | `sensor.cuco_v3_3244_power_cost_today_2` |
| 本月用电 | `sensor.cuco_v3_3244_power_cost_month_2` |

如果同一套固件要用于另一个会议室，只需要修改
`RoomDashboardConfig.h` 中的房间名、热点名和实体 ID，屏幕驱动、UI 与 HA
状态处理代码可以保持不变。

串口波特率为 115200。每次收到已配置的实体时会输出：

```text
Dashboard state queued: sensor.scd41_air_quality_monitor_temperature = 24.6
```

第一次按下屏幕时，串口还会显示原始坐标和方向转换后的坐标。例如：

```text
Touch pressed: raw=(292, 85), mapped=(187, 394)
```

触摸驱动会限制 I2C 等待时间，并在连续读取失败时自动复位控制器。
恢复成功时，串口会输出 `Touch controller recovered`。

## 当前范围

这一阶段完成热点配网、HA 连接状态和 106 会议室实体实时显示。窗户、电视和
`Leave Room` 的 Home Assistant 控制命令将在下一阶段加入。

## 参考资料

- [SenseCAP Indicator ESP32 Arduino 开发指南](https://wiki.seeedstudio.com/SenseCAP_Indicator_ESP32_Arduino/)
