# Seeed HA Discovery - Web Firmware Flasher

[English](#english) | [中文](#chinese)

---

<a name="english"></a>
## 🇬🇧 English

### Overview

This is a web-based firmware flasher that allows users to flash pre-compiled firmware directly to their ESP32 devices without needing to install Arduino IDE or any development tools.

### How It Works

1. **GitHub Actions** automatically compiles all Arduino sketches when code is pushed
2. **Compiled binaries** are stored as GitHub release artifacts
3. **ESP Web Tools** provides browser-based flashing via Web Serial API

### Live Demo

Visit: `https://seeed-projects.github.io/Seeed-Homeassistant-Discovery/flasher/`

### Supported Browsers

| Browser | Support |
|---------|---------|
| Chrome (Desktop) | ✅ Full Support |
| Edge (Desktop) | ✅ Full Support |
| Opera (Desktop) | ✅ Full Support |
| Chrome (Android) | ✅ Full Support |
| Safari | ❌ Not Supported |
| Firefox | ❌ Not Supported |

### Adding New Firmware

1. Create your Arduino sketch in `arduino/SeeedHADiscovery/examples/YourProduct/`
2. Add configuration to `firmware-config.yml`:
   ```yaml
   - id: YourProduct
     name: "Your Product Name"
     sketch: "arduino/SeeedHADiscovery/examples/YourProduct/YourProduct.ino"
     board: "esp32:esp32:esp32c6"
     # ... other options
   ```
3. Update `.github/workflows/build-firmware.yml` matrix
4. Add a card to `index.html`
5. Push to GitHub - firmware will be compiled automatically

### SenseCAP Indicator Room Dashboard

The SenseCAP Indicator D1/D1S firmware uses an ESP32-S3 build with 8MB flash,
the 3MB application partition, and OPI PSRAM. After installation, connect to
the `SenseCAP_Indicator_AP` hotspot to configure Wi-Fi, then select the Room 106
entities in the Seeed HA Discovery integration.

### File Structure

```
docs/flasher/
├── index.html              # Main flasher web page
├── firmware-config.yml     # Firmware configuration
├── README.md              # This file
└── firmware/              # Pre-compiled firmware files
    ├── IoTButtonV2_DeepSleep/
    │   ├── manifest.json   # ESP Web Tools manifest
    │   ├── bootloader.bin  # Bootloader binary
    │   ├── partitions.bin  # Partition table
    │   └── firmware.bin    # Application firmware
    └── WiFiProvisioning/
        └── ...
```

### Local Development

To test locally:

1. Compile firmware using Arduino IDE or arduino-cli
2. Copy `.bin` files to appropriate `firmware/` subdirectory
3. Serve the `docs/flasher/` directory with a local HTTP server:
   ```bash
   cd docs/flasher
   python -m http.server 8080
   ```
4. Open `http://localhost:8080` in Chrome

### Technical Details

**ESP Web Tools** uses the Web Serial API to communicate with ESP devices. The `manifest.json` file tells ESP Web Tools:

- Which chip family (ESP32, ESP32-S3, ESP32-C6, etc.)
- Which binary files to flash and at what offsets
- Whether to erase flash before writing

**Flash Memory Layout (ESP32-C6):**

| Offset | Content |
|--------|---------|
| 0x0000 | Bootloader |
| 0x8000 | Partition Table |
| 0x10000 | Application Firmware |

---

<a name="chinese"></a>
## 🇨🇳 中文

### 概述

这是一个基于网页的固件烧录器，允许用户直接在浏览器中将预编译的固件烧录到 ESP32 设备，无需安装 Arduino IDE 或任何开发工具。

### 工作原理

1. **GitHub Actions** 在代码推送时自动编译所有 Arduino 代码
2. **编译好的二进制文件** 作为 GitHub Release 的附件存储
3. **ESP Web Tools** 通过 Web Serial API 提供浏览器端烧录功能

### 在线演示

访问：`https://seeed-projects.github.io/Seeed-Homeassistant-Discovery/flasher/`

### 支持的浏览器

| 浏览器 | 支持情况 |
|--------|---------|
| Chrome（桌面版） | ✅ 完全支持 |
| Edge（桌面版） | ✅ 完全支持 |
| Opera（桌面版） | ✅ 完全支持 |
| Chrome（Android） | ✅ 完全支持 |
| Safari | ❌ 不支持 |
| Firefox | ❌ 不支持 |

### 添加新固件

1. 在 `arduino/SeeedHADiscovery/examples/YourProduct/` 创建 Arduino 代码
2. 在 `firmware-config.yml` 中添加配置：
   ```yaml
   - id: YourProduct
     name: "您的产品名称"
     sketch: "arduino/SeeedHADiscovery/examples/YourProduct/YourProduct.ino"
     board: "esp32:esp32:esp32c6"
     # ... 其他选项
   ```
3. 更新 `.github/workflows/build-firmware.yml` 的 matrix
4. 在 `index.html` 中添加卡片
5. 推送到 GitHub - 固件将自动编译

### SenseCAP Indicator 会议室看板

SenseCAP Indicator D1/D1S 固件使用 ESP32-S3、8MB Flash、3MB 应用分区和
OPI PSRAM。安装完成后，连接 `SenseCAP_Indicator_AP` 热点配置 Wi-Fi，随后
在 Seeed HA Discovery 集成中选择 106 会议室实体。

### 文件结构

```
docs/flasher/
├── index.html              # 主烧录器网页
├── firmware-config.yml     # 固件配置
├── README.md              # 本文件
└── firmware/              # 预编译的固件文件
    ├── IoTButtonV2_DeepSleep/
    │   ├── manifest.json   # ESP Web Tools 清单文件
    │   ├── bootloader.bin  # 引导加载程序
    │   ├── partitions.bin  # 分区表
    │   └── firmware.bin    # 应用固件
    └── WiFiProvisioning/
        └── ...
```

### 本地开发

本地测试步骤：

1. 使用 Arduino IDE 或 arduino-cli 编译固件
2. 将 `.bin` 文件复制到相应的 `firmware/` 子目录
3. 使用本地 HTTP 服务器提供 `docs/flasher/` 目录：
   ```bash
   cd docs/flasher
   python -m http.server 8080
   ```
4. 在 Chrome 中打开 `http://localhost:8080`

### 技术细节

**ESP Web Tools** 使用 Web Serial API 与 ESP 设备通信。`manifest.json` 文件告诉 ESP Web Tools：

- 芯片系列（ESP32、ESP32-S3、ESP32-C6 等）
- 要烧录的二进制文件及其偏移地址
- 是否在写入前擦除 Flash

**Flash 内存布局（ESP32-C6）：**

| 偏移地址 | 内容 |
|---------|------|
| 0x0000 | 引导加载程序 |
| 0x8000 | 分区表 |
| 0x10000 | 应用固件 |

---

## References | 参考资料

- [ESP Web Tools Documentation](https://esphome.github.io/esp-web-tools/)
- [Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)
- [Arduino CLI](https://arduino.github.io/arduino-cli/)
- [GitHub Actions for Arduino](https://github.com/arduino/compile-sketches)
