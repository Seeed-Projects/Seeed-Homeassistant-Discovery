"""
Seeed HA Discovery - 常量定义文件
Seeed HA Discovery - Constants definition file.

这个文件定义了集成中使用的所有常量，包括：
This file defines all constants used in the integration, including:
- 域名和制造商信息 | Domain and manufacturer information
- 配置相关的键名 | Configuration key names
- 默认端口设置 | Default port settings
- mDNS 服务类型 | mDNS service type
- WebSocket 消息类型 | WebSocket message types
- BLE 相关配置 | BLE related configurations
"""
from typing import Final

# =============================================================================
# 基本信息 | Basic Information
# =============================================================================

# 集成域名，Home Assistant 用这个来识别集成
# Integration domain, used by Home Assistant to identify the integration
DOMAIN: Final = "seeed_ha_discovery"

# 制造商名称，显示在设备信息中
# Manufacturer name, shown in device info
MANUFACTURER: Final = "Seeed Studio"

# =============================================================================
# 配置键名 | Configuration Keys
# =============================================================================

# 设备唯一标识符
# Unique device identifier
CONF_DEVICE_ID: Final = "device_id"

# 设备主机地址（IP 或主机名）
# Device host address (IP or hostname)
CONF_HOST: Final = "host"

# WebSocket 端口
# WebSocket port
CONF_PORT: Final = "port"

# 设备型号
# Device model
CONF_MODEL: Final = "model"

# =============================================================================
# 默认值 | Default Values
# =============================================================================

# HTTP 服务器默认端口
# Default HTTP server port
DEFAULT_HTTP_PORT: Final = 80

# WebSocket 服务器默认端口
# Default WebSocket server port
DEFAULT_WS_PORT: Final = 81

# 重连间隔（秒）
# Reconnect interval in seconds
RECONNECT_INTERVAL: Final = 30

# 心跳间隔（秒）
# Heartbeat interval in seconds
HEARTBEAT_INTERVAL: Final = 15

# =============================================================================
# mDNS 配置 | mDNS Configuration
# =============================================================================

# mDNS 服务类型，用于自动发现设备
# mDNS service type for auto-discovery
# 设备会广播这个服务类型，HA 会自动发现
# Device broadcasts this service type, HA will auto-discover
ZEROCONF_SERVICE_TYPE: Final = "_seeed_ha._tcp.local."

# =============================================================================
# WebSocket 消息类型 | WebSocket Message Types
# =============================================================================

# 心跳检测 - 设备发送
# Heartbeat ping - sent by device
MSG_TYPE_PING: Final = "ping"

# 心跳响应 - HA 回复
# Heartbeat response - sent by HA
MSG_TYPE_PONG: Final = "pong"

# 状态更新 - 设备上报传感器数据
# State update - device reports sensor data
MSG_TYPE_STATE: Final = "state"

# 设备发现 - 设备上报自己支持的实体
# Device discovery - device reports its entities
MSG_TYPE_DISCOVERY: Final = "discovery"

# 控制命令 - HA 发送到设备
# Control command - sent from HA to device
MSG_TYPE_COMMAND: Final = "command"

# =============================================================================
# 支持的平台 | Supported Platforms
# =============================================================================

# 支持传感器和开关平台
# Supports sensor and switch platforms
PLATFORMS: Final = [
    "sensor",
    "switch",
]

# =============================================================================
# BLE 配置 | BLE Configuration
# =============================================================================

# Seeed Manufacturer ID (0x5EED = 24301)
SEEED_MANUFACTURER_ID: Final = 0x5EED

# BTHome Service UUID
BTHOME_SERVICE_UUID: Final = "0000fcd2-0000-1000-8000-00805f9b34fb"

# Seeed HA Control Service UUID (用于双向通信 | for bidirectional communication)
SEEED_CONTROL_SERVICE_UUID: Final = "5eed0001-b5a3-f393-e0a9-e50e24dcca9e"
SEEED_CONTROL_COMMAND_CHAR_UUID: Final = "5eed0002-b5a3-f393-e0a9-e50e24dcca9e"
SEEED_CONTROL_STATE_CHAR_UUID: Final = "5eed0003-b5a3-f393-e0a9-e50e24dcca9e"

# 连接类型 | Connection type
CONF_CONNECTION_TYPE: Final = "connection_type"
CONNECTION_TYPE_WIFI: Final = "wifi"
CONNECTION_TYPE_BLE: Final = "ble"

# BLE 设备地址 | BLE device address
CONF_BLE_ADDRESS: Final = "ble_address"

# BLE 设备是否支持控制 | Whether BLE device supports control
CONF_BLE_CONTROL: Final = "ble_control"

# =============================================================================
# BTHome 传感器类型映射 | BTHome Sensor Type Mapping
# =============================================================================

# BTHome Object ID 到 Home Assistant 传感器类型的映射
# BTHome Object ID to Home Assistant sensor type mapping
BTHOME_SENSOR_TYPES: Final = {
    0x01: {"name": "Battery", "device_class": "battery", "unit": "%"},
    0x02: {"name": "Temperature", "device_class": "temperature", "unit": "°C", "factor": 0.01},
    0x03: {"name": "Humidity", "device_class": "humidity", "unit": "%", "factor": 0.01},
    0x04: {"name": "Pressure", "device_class": "pressure", "unit": "hPa", "factor": 0.01},
    0x05: {"name": "Illuminance", "device_class": "illuminance", "unit": "lx", "factor": 0.01},
    0x06: {"name": "Mass", "device_class": "weight", "unit": "kg", "factor": 0.01},
    0x08: {"name": "Dewpoint", "device_class": "temperature", "unit": "°C", "factor": 0.01},
    0x09: {"name": "Count", "device_class": None, "unit": None},
    0x0A: {"name": "Energy", "device_class": "energy", "unit": "kWh", "factor": 0.001},
    0x0B: {"name": "Power", "device_class": "power", "unit": "W", "factor": 0.01},
    0x0C: {"name": "Voltage", "device_class": "voltage", "unit": "V", "factor": 0.001},
    0x0D: {"name": "PM2.5", "device_class": "pm25", "unit": "µg/m³"},
    0x0E: {"name": "PM10", "device_class": "pm10", "unit": "µg/m³"},
    0x12: {"name": "CO2", "device_class": "carbon_dioxide", "unit": "ppm"},
    0x13: {"name": "TVOC", "device_class": "volatile_organic_compounds", "unit": "µg/m³"},
    0x14: {"name": "Moisture", "device_class": "moisture", "unit": "%", "factor": 0.01},
    0x2E: {"name": "Humidity", "device_class": "humidity", "unit": "%"},
    0x45: {"name": "Temperature", "device_class": "temperature", "unit": "°C", "factor": 0.1},
    0x46: {"name": "UV Index", "device_class": None, "unit": "UV index"},
}

# BTHome 二进制传感器类型映射
# BTHome binary sensor type mapping
# 注意：这些作为普通 sensor 创建时，device_class 需要设为 None
# Note: When created as regular sensor, device_class needs to be None
# 因为 HA 的 sensor 和 binary_sensor 的 device_class 不同
# Because HA's sensor and binary_sensor have different device_class values
BTHOME_BINARY_SENSOR_TYPES: Final = {
    0x0F: {"name": "Generic", "device_class": None},
    0x10: {"name": "Power State", "device_class": None},  # 二进制：电源开/关 | Binary: power on/off
    0x11: {"name": "Opening", "device_class": None},      # 二进制：开门/关门 | Binary: door open/closed
    0x15: {"name": "Battery Low", "device_class": None},
    0x16: {"name": "Battery Charging", "device_class": None},
    0x20: {"name": "Occupancy", "device_class": None},
    0x21: {"name": "Motion", "device_class": None},
}

# BTHome 事件类型（如按钮）| BTHome event types (e.g., button)
BTHOME_EVENT_TYPES: Final = {
    0x3A: {"name": "Button", "events": {
        0x00: "none",
        0x01: "press",
        0x02: "double_press",
        0x03: "triple_press",
        0x04: "long_press",
        0x05: "long_double_press",
        0x06: "long_triple_press",
    }},
}
