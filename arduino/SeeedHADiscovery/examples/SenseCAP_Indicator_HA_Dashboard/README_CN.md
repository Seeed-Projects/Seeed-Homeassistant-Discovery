# SenseCAP Indicator HA 看板

这是通用 SenseCAP Indicator Home Assistant 看板的第一阶段 Arduino
硬件验证示例。程序会初始化 480 x 480 ST7701 屏幕，并显示一个
静态测试界面。

## 所需硬件

- SenseCAP Indicator D1 或 D1S
- 连接原生 USB 接口的 USB-C 数据线

## Arduino 依赖

- ESP32 开发板包 3.1.2
- GFX Library for Arduino 1.5.3
- PCA95x5 0.1.3

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

上传后，屏幕会显示 `SenseCAP Indicator`、`Display Ready`、屏幕分辨率
和四个彩色方块。串口监视器使用 115200 波特率，成功时会输出
`Display test screen ready`。

## 当前范围

这一阶段先建立可复用的 Arduino 屏幕基础。后续阶段会逐步加入热点配网、
Home Assistant 状态下发、实体显示、触摸输入和设备控制。

## 参考资料

- [SenseCAP Indicator ESP32 Arduino 开发指南](https://wiki.seeedstudio.com/SenseCAP_Indicator_ESP32_Arduino/)
