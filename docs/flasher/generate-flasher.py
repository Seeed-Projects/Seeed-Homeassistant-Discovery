#!/usr/bin/env python3
"""
Firmware Flasher Page Generator | 固件烧录页面生成器

This script reads firmware-config.yml and generates:
1. Updated index.html with firmware cards | 带有固件卡片的 index.html
2. manifest.json for each firmware | 每个固件的 manifest.json
3. GitHub Actions matrix configuration | GitHub Actions matrix 配置

Usage | 用法:
    python generate-flasher.py

This helps keep index.html in sync with firmware-config.yml automatically.
这有助于自动保持 index.html 与 firmware-config.yml 同步。
"""

import yaml
import json
import os
from pathlib import Path

# ESP32 chip family flash offsets | ESP32 芯片系列的 Flash 偏移地址
# Different chips have different memory layouts | 不同芯片有不同的内存布局
# boot_app0.bin at 0xe000 is required for OTA boot selection | 0xe000 处的 boot_app0.bin 是 OTA 启动选择所需
CHIP_OFFSETS = {
    "ESP32": {
        "bootloader": 0x1000,
        "partitions": 0x8000,
        "boot_app0": 0xe000,
        "firmware": 0x10000
    },
    "ESP32-S2": {
        "bootloader": 0x1000,
        "partitions": 0x8000,
        "boot_app0": 0xe000,
        "firmware": 0x10000
    },
    "ESP32-S3": {
        "bootloader": 0x0,
        "partitions": 0x8000,
        "boot_app0": 0xe000,
        "firmware": 0x10000
    },
    "ESP32-C3": {
        "bootloader": 0x0,
        "partitions": 0x8000,
        "boot_app0": 0xe000,
        "firmware": 0x10000
    },
    "ESP32-C5": {
        "bootloader": 0x2000,  # ESP32-C5 uses 0x2000 | ESP32-C5 使用 0x2000
        "partitions": 0x8000,
        "boot_app0": 0xe000,
        "firmware": 0x10000
    },
    "ESP32-C6": {
        "bootloader": 0x0,
        "partitions": 0x8000,
        "boot_app0": 0xe000,
        "firmware": 0x10000
    },
    "ESP32-H2": {
        "bootloader": 0x0,
        "partitions": 0x8000,
        "boot_app0": 0xe000,
        "firmware": 0x10000
    }
}


def load_config(config_path: str) -> dict:
    """Load firmware configuration from YAML file | 从 YAML 文件加载固件配置"""
    with open(config_path, 'r', encoding='utf-8') as f:
        return yaml.safe_load(f)


def generate_manifest(firmware: dict, output_dir: str) -> None:
    """Generate manifest.json for ESP Web Tools | 为 ESP Web Tools 生成 manifest.json"""
    chip_family = firmware.get('chip_family', 'ESP32-C6')
    offsets = CHIP_OFFSETS.get(chip_family, CHIP_OFFSETS['ESP32-C6'])
    
    manifest = {
        "name": firmware['name'],
        "version": firmware.get('version', '1.0.0'),
        "home_assistant_domain": "seeed_ha_discovery",
        "funding_url": "https://www.seeedstudio.com/",
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": 0,
        "builds": [
            {
                "chipFamily": chip_family,
                "parts": [
                    {"path": "bootloader.bin", "offset": offsets['bootloader']},
                    {"path": "partitions.bin", "offset": offsets['partitions']},
                    {"path": "boot_app0.bin", "offset": offsets['boot_app0']},
                    {"path": "firmware.bin", "offset": offsets['firmware']}
                ]
            }
        ]
    }
    
    # Create output directory | 创建输出目录
    firmware_dir = os.path.join(output_dir, firmware['id'])
    os.makedirs(firmware_dir, exist_ok=True)
    
    # Write manifest.json | 写入 manifest.json
    manifest_path = os.path.join(firmware_dir, 'manifest.json')
    with open(manifest_path, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)
    
    print(f"  ✓ Generated: {manifest_path}")


def generate_firmware_card(firmware: dict, lang: str = 'en') -> str:
    """Generate HTML card for a firmware | 为固件生成 HTML 卡片"""
    enabled = firmware.get('enabled', True)
    opacity_style = '' if enabled else ' style="opacity: 0.6;"'
    
    # Get localized strings | 获取本地化字符串
    name = firmware.get('name_zh' if lang == 'zh' else 'name', firmware['name'])
    desc = firmware.get('description_zh' if lang == 'zh' else 'description', firmware.get('description', ''))
    features = firmware.get('features_zh' if lang == 'zh' else 'features', [])
    
    # Generate feature tags | 生成功能标签
    features_html = '\n'.join([
        f'                        <span class="feature-tag" data-en="{f}" data-zh="{fz}">{f}</span>'
        for f, fz in zip(
            firmware.get('features', []), 
            firmware.get('features_zh', firmware.get('features', []))
        )
    ])
    
    # Generate button or disabled state | 生成按钮或禁用状态
    if enabled:
        button_html = f'''
                    <esp-web-install-button manifest="./firmware/{firmware['id']}/manifest.json">
                        <button slot="activate" data-en="Install {firmware['name']} Firmware" data-zh="安装 {firmware.get('name_zh', firmware['name'])} 固件">
                            Install {firmware['name']} Firmware
                        </button>
                        <span slot="unsupported" data-en="Your browser doesn't support Web Serial" data-zh="您的浏览器不支持 Web Serial">
                            Your browser doesn't support Web Serial
                        </span>
                        <span slot="not-allowed" data-en="Not allowed to use Web Serial" data-zh="不允许使用 Web Serial">
                            Not allowed to use Web Serial
                        </span>
                    </esp-web-install-button>
                    <div class="unsupported" data-en="Please use Chrome, Edge, or Opera browser" data-zh="请使用 Chrome、Edge 或 Opera 浏览器">
                        Please use Chrome, Edge, or Opera browser
                    </div>'''
    else:
        button_html = '''
                    <button disabled style="width: 100%; padding: 0.875rem; background: var(--text-muted); border: none; border-radius: 10px; color: var(--bg-primary); font-weight: 600; cursor: not-allowed;" data-en="Coming Soon" data-zh="即将推出">
                        Coming Soon
                    </button>'''
    
    card_html = f'''
                <!-- {firmware['name']} -->
                <div class="firmware-card"{opacity_style}>
                    <div class="card-header">
                        <div class="card-icon">{firmware.get('icon', '📦')}</div>
                        <div>
                            <div class="card-title">{firmware['name']}</div>
                            <span class="card-chip">{firmware.get('chip_family', 'ESP32')}</span>
                        </div>
                    </div>
                    <p class="card-description" data-en="{firmware.get('description', '')}" data-zh="{firmware.get('description_zh', firmware.get('description', ''))}">
                        {firmware.get('description', '')}
                    </p>
                    <div class="card-features">
{features_html}
                    </div>
                    {button_html}
                </div>'''
    
    return card_html


def generate_github_actions_matrix(config: dict) -> str:
    """Generate GitHub Actions matrix configuration | 生成 GitHub Actions matrix 配置"""
    matrix_entries = []
    
    for fw in config.get('firmware', []):
        if fw.get('enabled', True) is False:
            continue
            
        entry = f'''          - firmware: "{fw['id']}"
            sketch: "{fw['sketch']}"
            board: "{fw['board']}"
            board_options: "{fw.get('board_options', '')}"
            platform: "{fw['platform']}"
            platform_url: "{fw['platform_url']}"'''
        matrix_entries.append(entry)
    
    return '\n            \n'.join(matrix_entries)


def main():
    """Main function | 主函数"""
    script_dir = Path(__file__).parent
    config_path = script_dir / 'firmware-config.yml'
    firmware_dir = script_dir / 'firmware'
    
    print("=" * 60)
    print("  Firmware Flasher Generator | 固件烧录器生成器")
    print("=" * 60)
    print()
    
    # Load configuration | 加载配置
    print(f"Loading config from: {config_path}")
    config = load_config(config_path)
    
    firmware_list = config.get('firmware', [])
    print(f"Found {len(firmware_list)} firmware entries")
    print()
    
    # Generate manifest.json for each firmware | 为每个固件生成 manifest.json
    print("Generating manifest.json files:")
    for fw in firmware_list:
        generate_manifest(fw, str(firmware_dir))
    print()
    
    # Generate firmware cards HTML | 生成固件卡片 HTML
    print("Generating firmware cards HTML:")
    cards_html = '\n'.join([generate_firmware_card(fw) for fw in firmware_list])
    print("  ✓ Generated firmware cards")
    print()
    
    # Generate GitHub Actions matrix | 生成 GitHub Actions matrix
    print("GitHub Actions Matrix Configuration:")
    print("-" * 40)
    matrix_yaml = generate_github_actions_matrix(config)
    print(matrix_yaml)
    print("-" * 40)
    print()
    
    # Output the cards HTML for manual insertion | 输出卡片 HTML 供手动插入
    print("Firmware Cards HTML (copy to index.html):")
    print("-" * 40)
    print(cards_html)
    print("-" * 40)
    print()
    
    print("=" * 60)
    print("  Done! | 完成!")
    print("=" * 60)
    print()
    print("Next steps | 下一步:")
    print("1. Copy the firmware cards HTML to docs/flasher/index.html")
    print("   将固件卡片 HTML 复制到 docs/flasher/index.html")
    print("2. Copy the matrix configuration to .github/workflows/build-firmware.yml")
    print("   将 matrix 配置复制到 .github/workflows/build-firmware.yml")
    print("3. Push to GitHub to trigger automatic build")
    print("   推送到 GitHub 触发自动构建")


if __name__ == '__main__':
    main()

