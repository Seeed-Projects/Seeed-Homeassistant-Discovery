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

这些依赖可以通过 Arduino 开发板管理器和库管理器安装。FT5x06 触摸驱动
已经包含在本示例中。

## 开发板设置

选择 `ESP32S3 Dev Module`，并使用以下选项：

- Flash Size: `8MB (64Mb)`
- PSRAM: `OPI PSRAM`
- USB CDC On Boot: `Enabled`

## 使用 Arduino CLI 编译

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi,CDCOnBoot=cdc" \
  arduino/SeeedHADiscovery/examples/SenseCAP_Indicator_HA_Dashboard
```

上传后，屏幕会显示深色会议室仪表盘，包含四张数值卡片和一张触摸测试卡片。
Home Assistant 尚未下发实体状态时，数值显示为 `--`。点击底部卡片后，计数器
会加一，标题变为 `Touch confirmed`，串口监视器会输出
`Touch test count: 1`。串口波特率为 115200，成功启动的最后一条日志为
`LVGL dashboard ready`。

第一次按下屏幕时，串口还会显示原始坐标和方向转换后的坐标。例如：

```text
Touch pressed: raw=(292, 85), mapped=(187, 394)
```

触摸驱动会限制 I2C 等待时间，并在连续读取失败时自动复位控制器。
恢复成功时，串口会输出 `Touch controller recovered`。

## 当前范围

这一阶段建立可复用的 Arduino 屏幕、LVGL 界面和触摸输入模块。后续阶段会
逐步加入热点配网、Home Assistant 状态下发、实体选择和可控实体操作。

## 参考资料

- [SenseCAP Indicator ESP32 Arduino 开发指南](https://wiki.seeedstudio.com/SenseCAP_Indicator_ESP32_Arduino/)
