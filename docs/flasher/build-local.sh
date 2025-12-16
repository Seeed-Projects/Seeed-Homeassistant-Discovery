#!/bin/bash
# ============================================================================
# Local Firmware Build Script | 本地固件构建脚本
# ============================================================================
#
# This script builds firmware locally for testing before pushing to GitHub.
# 此脚本在推送到 GitHub 之前本地构建固件进行测试。
#
# Prerequisites | 前提条件:
# - Arduino CLI installed (brew install arduino-cli / sudo apt install arduino-cli)
#   已安装 Arduino CLI
# - ESP32 board package installed | 已安装 ESP32 开发板包
#
# Usage | 用法:
#   ./build-local.sh                    # Build all firmware | 构建所有固件
#   ./build-local.sh IoTButtonV2        # Build specific firmware | 构建指定固件
#
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUTPUT_DIR="$SCRIPT_DIR/firmware"

# Colors for output | 输出颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}"
echo "=============================================="
echo "  Seeed HA Discovery - Local Firmware Builder"
echo "=============================================="
echo -e "${NC}"

# Check if arduino-cli is installed | 检查是否安装了 arduino-cli
if ! command -v arduino-cli &> /dev/null; then
    echo -e "${RED}Error: arduino-cli is not installed${NC}"
    echo "Please install it first:"
    echo "  macOS: brew install arduino-cli"
    echo "  Linux: sudo apt install arduino-cli"
    echo "  Or: curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
    exit 1
fi

echo -e "${GREEN}✓ arduino-cli found${NC}"

# Initialize arduino-cli config if needed | 如果需要，初始化 arduino-cli 配置
if [ ! -f "$HOME/.arduino15/arduino-cli.yaml" ]; then
    echo "Initializing arduino-cli config..."
    arduino-cli config init
fi

# Add ESP32 board manager URL | 添加 ESP32 开发板管理器 URL
echo "Checking ESP32 board package..."
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json 2>/dev/null || true
arduino-cli core update-index

# Install ESP32 platform if not installed | 如果未安装则安装 ESP32 平台
if ! arduino-cli core list | grep -q "esp32:esp32"; then
    echo "Installing ESP32 platform..."
    arduino-cli core install esp32:esp32
fi
echo -e "${GREEN}✓ ESP32 platform ready${NC}"

# Install required libraries | 安装所需库
echo "Checking required libraries..."
arduino-cli lib install "ArduinoJson" 2>/dev/null || true
arduino-cli lib install "WebSockets" 2>/dev/null || true
arduino-cli lib install "Adafruit NeoPixel" 2>/dev/null || true
echo -e "${GREEN}✓ Libraries ready${NC}"

# Install local SeeedHADiscovery library | 安装本地 SeeedHADiscovery 库
ARDUINO_LIB_DIR="$HOME/Arduino/libraries"
mkdir -p "$ARDUINO_LIB_DIR"
if [ -d "$ARDUINO_LIB_DIR/SeeedHADiscovery" ]; then
    rm -rf "$ARDUINO_LIB_DIR/SeeedHADiscovery"
fi
cp -r "$PROJECT_ROOT/arduino/SeeedHADiscovery" "$ARDUINO_LIB_DIR/"
echo -e "${GREEN}✓ SeeedHADiscovery library installed${NC}"

# Define firmware configurations | 定义固件配置
# Format: "NAME|SKETCH_PATH|BOARD|OPTIONS|CHIP_FAMILY"
FIRMWARE_CONFIGS=(
    "IoTButtonV2_DeepSleep|arduino/SeeedHADiscovery/examples/IoTButtonV2_DeepSleep/IoTButtonV2_DeepSleep.ino|esp32:esp32:esp32c6|CDCOnBoot=cdc,PartitionScheme=huge_app,CPUFreq=80,FlashMode=qio,FlashSize=4M|ESP32-C6"
    "WiFiProvisioning|arduino/SeeedHADiscovery/examples/WiFiProvisioning/WiFiProvisioning.ino|esp32:esp32:esp32c6|CDCOnBoot=cdc,PartitionScheme=huge_app,CPUFreq=80,FlashMode=qio,FlashSize=4M|ESP32-C6"
    # Add more firmware here | 在此添加更多固件
    # "YourFirmware|path/to/sketch.ino|esp32:esp32:esp32c6|options|ESP32-C6"
)

# Function to build firmware | 构建固件的函数
build_firmware() {
    local config="$1"
    local name=$(echo "$config" | cut -d'|' -f1)
    local sketch=$(echo "$config" | cut -d'|' -f2)
    local board=$(echo "$config" | cut -d'|' -f3)
    local options=$(echo "$config" | cut -d'|' -f4)
    local chip_family=$(echo "$config" | cut -d'|' -f5)
    
    echo ""
    echo -e "${BLUE}Building: $name${NC}"
    echo "  Sketch: $sketch"
    echo "  Board: $board"
    echo "  Chip: $chip_family"
    
    local firmware_dir="$OUTPUT_DIR/$name"
    mkdir -p "$firmware_dir"
    
    # Compile | 编译
    echo "  Compiling..."
    if arduino-cli compile \
        --fqbn "$board:$options" \
        --output-dir "$firmware_dir" \
        --export-binaries \
        "$PROJECT_ROOT/$sketch"; then
        echo -e "${GREEN}  ✓ Compilation successful${NC}"
    else
        echo -e "${RED}  ✗ Compilation failed${NC}"
        return 1
    fi
    
    # Rename files | 重命名文件
    cd "$firmware_dir"
    for f in *.bin; do
        if [[ "$f" == *".ino.bin" ]]; then
            mv "$f" "firmware.bin"
        elif [[ "$f" == *".bootloader.bin" ]]; then
            mv "$f" "bootloader.bin"
        elif [[ "$f" == *".partitions.bin" ]]; then
            mv "$f" "partitions.bin"
        fi
    done
    
    # Clean up extra files | 清理多余文件
    rm -f *.elf *.map 2>/dev/null || true
    
    # Copy boot_app0.bin from ESP32 Core (required for OTA boot selection)
    # 从 ESP32 Core 复制 boot_app0.bin（OTA 启动选择所需）
    local boot_app0_src=$(find "$HOME/Library/Arduino15/packages/esp32/hardware/esp32" -name "boot_app0.bin" -type f 2>/dev/null | head -1)
    if [ -z "$boot_app0_src" ]; then
        # Try Linux path | 尝试 Linux 路径
        boot_app0_src=$(find "$HOME/.arduino15/packages/esp32/hardware/esp32" -name "boot_app0.bin" -type f 2>/dev/null | head -1)
    fi
    
    if [ -n "$boot_app0_src" ]; then
        cp "$boot_app0_src" ./boot_app0.bin
        echo -e "${GREEN}  ✓ Copied boot_app0.bin${NC}"
    else
        echo -e "${YELLOW}  ⚠ Warning: boot_app0.bin not found${NC}"
    fi
    
    # Determine bootloader offset | 确定 bootloader 偏移
    local bootloader_offset=0
    case "$chip_family" in
        "ESP32"|"ESP32-S2")
            bootloader_offset=4096   # 0x1000
            ;;
        "ESP32-C5")
            bootloader_offset=8192   # 0x2000 - ESP32-C5 specific
            ;;
        *)
            bootloader_offset=0
            ;;
    esac
    
    # Create manifest.json with boot_app0.bin | 创建包含 boot_app0.bin 的 manifest.json
    cat > manifest.json << EOF
{
  "name": "$name",
  "version": "1.0.0",
  "home_assistant_domain": "seeed_ha_discovery",
  "funding_url": "https://www.seeedstudio.com/",
  "new_install_prompt_erase": true,
  "new_install_improv_wait_time": 0,
  "builds": [
    {
      "chipFamily": "$chip_family",
      "parts": [
        { "path": "bootloader.bin", "offset": $bootloader_offset },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "boot_app0.bin", "offset": 57344 },
        { "path": "firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
EOF
    
    echo -e "${GREEN}  ✓ manifest.json created${NC}"
    
    # Show file sizes | 显示文件大小
    echo "  Files:"
    ls -lh *.bin | while read line; do
        echo "    $line"
    done
    
    cd - > /dev/null
}

# Build firmware | 构建固件
echo ""
echo -e "${YELLOW}Starting firmware builds...${NC}"

FILTER="${1:-}"  # Optional filter argument | 可选的过滤参数

for config in "${FIRMWARE_CONFIGS[@]}"; do
    name=$(echo "$config" | cut -d'|' -f1)
    
    # Skip if filter is provided and doesn't match | 如果提供了过滤器且不匹配则跳过
    if [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]]; then
        continue
    fi
    
    build_firmware "$config"
done

echo ""
echo -e "${GREEN}=============================================="
echo "  Build Complete!"
echo "==============================================${NC}"
echo ""
echo "Firmware files are in: $OUTPUT_DIR"
echo ""
echo "To test locally:"
echo "  cd $SCRIPT_DIR"
echo "  python3 -m http.server 8080"
echo "  Then open: http://localhost:8080"
echo ""
echo "Note: Web Serial requires HTTPS or localhost"
echo "      Use Chrome, Edge, or Opera browser"

