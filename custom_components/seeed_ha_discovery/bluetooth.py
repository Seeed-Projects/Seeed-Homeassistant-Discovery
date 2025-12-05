"""
Seeed HA Discovery - 蓝牙设备解析器
Seeed HA Discovery - Bluetooth device parser.

这个文件负责：
This file is responsible for:
1. 解析 BTHome 格式的 BLE 广播数据
   Parse BTHome format BLE advertisement data
2. 提取传感器值和设备信息
   Extract sensor values and device information
3. 提供给 config_flow 和传感器实体使用
   Provide data to config_flow and sensor entities

BTHome 数据格式 | BTHome data format:
- Service UUID: 0xFCD2
- Device Info: 1 byte (包含版本号和加密信息 | contains version and encryption info)
- Sensor Data: [Object ID (1 byte)][Value (1-4 bytes)]...
"""
from __future__ import annotations

import logging
import struct
from dataclasses import dataclass, field
from typing import Any

from bluetooth_data_tools import short_address
from home_assistant_bluetooth import BluetoothServiceInfoBleak

from .const import (
    BTHOME_SERVICE_UUID,
    BTHOME_SENSOR_TYPES,
    BTHOME_BINARY_SENSOR_TYPES,
    BTHOME_EVENT_TYPES,
    SEEED_MANUFACTURER_ID,
)

_LOGGER = logging.getLogger(__name__)

# BTHome v2 设备信息标志 | BTHome v2 device info flags
BTHOME_DEVICE_INFO_ENCRYPT = 0x01  # 数据已加密 | Data is encrypted
BTHOME_DEVICE_INFO_TRIGGER = 0x04  # 触发器设备 | Trigger device
BTHOME_DEVICE_INFO_VERSION_MASK = 0xE0  # 版本位掩码 | Version bit mask
BTHOME_VERSION_2 = 0x40  # BTHome v2


@dataclass
class BTHomeSensorData:
    """
    BTHome 传感器数据
    BTHome sensor data.
    """
    object_id: int
    name: str
    value: Any
    device_class: str | None = None
    unit: str | None = None


@dataclass
class SeeedBLEDevice:
    """
    Seeed BLE 设备数据类
    Holds parsed data from a Seeed BLE device.
    """
    address: str
    name: str | None
    rssi: int
    sensors: list[BTHomeSensorData] = field(default_factory=list)
    events: list[dict[str, Any]] = field(default_factory=list)
    is_bthome_v2: bool = False
    is_encrypted: bool = False
    raw_data: bytes | None = None

    @property
    def short_address(self) -> str:
        """
        返回短地址格式
        Return short address format.
        """
        return short_address(self.address)


def parse_bthome_data(data: bytes) -> tuple[list[BTHomeSensorData], list[dict[str, Any]], bool, bool]:
    """
    解析 BTHome 格式的 Service Data
    Parse BTHome format service data.

    参数 | Args:
        data: BTHome Service Data 字节 | BTHome Service Data bytes

    返回 | Returns:
        tuple: (传感器列表, 事件列表, 是否 BTHome v2, 是否加密)
               (sensor list, event list, is BTHome v2, is encrypted)
    """
    if len(data) < 1:
        return [], [], False, False

    sensors: list[BTHomeSensorData] = []
    events: list[dict[str, Any]] = []

    # 第一个字节是设备信息 | First byte is device info
    device_info = data[0]
    is_v2 = (device_info & BTHOME_DEVICE_INFO_VERSION_MASK) == BTHOME_VERSION_2
    is_encrypted = bool(device_info & BTHOME_DEVICE_INFO_ENCRYPT)

    if is_encrypted:
        # BTHome 数据已加密 | BTHome data is encrypted
        _LOGGER.debug("BTHome data is encrypted, skipping parse")
        return [], [], is_v2, is_encrypted

    if not is_v2:
        # 不是 BTHome v2 格式 | Not BTHome v2 format
        _LOGGER.debug("Not BTHome v2 format")
        return [], [], is_v2, is_encrypted

    # 从第二个字节开始解析传感器数据 | Parse sensor data starting from second byte
    offset = 1
    while offset < len(data):
        object_id = data[offset]
        offset += 1

        # 获取传感器类型信息 | Get sensor type info
        sensor_info = None
        is_event = False
        is_binary = False

        if object_id in BTHOME_SENSOR_TYPES:
            sensor_info = BTHOME_SENSOR_TYPES[object_id]
        elif object_id in BTHOME_BINARY_SENSOR_TYPES:
            sensor_info = BTHOME_BINARY_SENSOR_TYPES[object_id]
            is_binary = True
        elif object_id in BTHOME_EVENT_TYPES:
            sensor_info = BTHOME_EVENT_TYPES[object_id]
            is_event = True

        if sensor_info is None:
            # 未知的 Object ID | Unknown Object ID
            _LOGGER.debug("Unknown Object ID: 0x%02X", object_id)
            # 尝试跳过未知数据（假设 2 字节）| Try to skip unknown data (assume 2 bytes)
            offset += 2
            continue

        # 根据 Object ID 获取数据大小 | Get data size based on Object ID
        data_size = _get_data_size(object_id)
        if offset + data_size > len(data):
            # 数据不完整 | Incomplete data
            _LOGGER.warning("Incomplete data: need %d bytes, only have %d bytes", data_size, len(data) - offset)
            break

        # 读取值 | Read value
        value_bytes = data[offset:offset + data_size]
        offset += data_size

        # 解析值 | Parse value
        value = _parse_value(object_id, value_bytes)

        # 应用缩放因子 | Apply scaling factor
        factor = sensor_info.get("factor", 1)
        if isinstance(value, (int, float)) and factor != 1:
            value = round(value * factor, 2)

        if is_event:
            # 事件类型 | Event type
            event_name = sensor_info.get("events", {}).get(value, "unknown")
            events.append({
                "type": sensor_info["name"],
                "event": event_name,
                "value": value,
            })
            _LOGGER.debug("BTHome event: %s = %s", sensor_info["name"], event_name)
        else:
            # 传感器类型 | Sensor type
            sensor = BTHomeSensorData(
                object_id=object_id,
                name=sensor_info["name"],
                value=value,
                device_class=sensor_info.get("device_class"),
                unit=sensor_info.get("unit"),
            )
            sensors.append(sensor)
            _LOGGER.debug("BTHome sensor: %s = %s %s", sensor.name, sensor.value, sensor.unit or "")

    return sensors, events, is_v2, is_encrypted


def _get_data_size(object_id: int) -> int:
    """
    根据 Object ID 获取数据大小
    Get data size based on Object ID.
    """
    # 1 字节数据 | 1-byte data
    one_byte = {
        0x01,  # Battery
        0x09,  # Count uint8
        0x0F,  # Binary Generic
        0x10,  # Binary Power
        0x11,  # Binary Opening
        0x15,  # Battery Low
        0x16,  # Battery Charging
        0x20,  # Occupancy
        0x21,  # Motion
        0x2E,  # Humidity uint8
        0x2F,  # Moisture uint8
        0x3A,  # Button
        0x46,  # UV Index
    }

    # 3 字节数据 | 3-byte data
    three_bytes = {
        0x04,  # Pressure
        0x05,  # Illuminance
        0x0A,  # Energy
        0x0B,  # Power
        0x42,  # Duration
        0x4B,  # Gas
    }

    # 4 字节数据 | 4-byte data
    four_bytes = {
        0x3E,  # Count uint32
        0x4C,  # Gas uint32
        0x4D,  # Energy uint32
        0x4E,  # Volume uint32
        0x4F,  # Water
    }

    if object_id in one_byte:
        return 1
    elif object_id in three_bytes:
        return 3
    elif object_id in four_bytes:
        return 4
    else:
        return 2  # 默认 2 字节 | Default 2 bytes


def _parse_value(object_id: int, value_bytes: bytes) -> int | float:
    """
    解析传感器值
    Parse sensor value from bytes.
    """
    size = len(value_bytes)

    # 有符号类型 | Signed types
    signed_types = {0x02, 0x08, 0x3F, 0x45}  # Temperature, Dewpoint, Rotation, Temperature_tenth

    if size == 1:
        if object_id in signed_types:
            return struct.unpack("<b", value_bytes)[0]
        return value_bytes[0]
    elif size == 2:
        if object_id in signed_types:
            return struct.unpack("<h", value_bytes)[0]
        return struct.unpack("<H", value_bytes)[0]
    elif size == 3:
        # 24 位无符号整数 | 24-bit unsigned integer
        return value_bytes[0] | (value_bytes[1] << 8) | (value_bytes[2] << 16)
    elif size == 4:
        return struct.unpack("<I", value_bytes)[0]
    else:
        return 0


def parse_ble_advertisement(
    service_info: BluetoothServiceInfoBleak,
) -> SeeedBLEDevice | None:
    """
    解析 BLE 广播数据
    Parse BLE advertisement data.

    参数 | Args:
        service_info: Home Assistant 蓝牙服务信息 | Home Assistant Bluetooth service info

    返回 | Returns:
        SeeedBLEDevice: 解析后的设备数据，如果不是有效的 Seeed/BTHome 设备则返回 None
                       Parsed device data, or None if not a valid Seeed/BTHome device
    """
    # 解析 BLE 广播 | Parsing BLE advertisement
    _LOGGER.debug(
        "Parsing BLE advertisement: %s (%s), RSSI: %d",
        service_info.name,
        service_info.address,
        service_info.rssi,
    )

    # 检查是否有 BTHome Service Data | Check for BTHome Service Data
    service_data = service_info.service_data
    bthome_data = service_data.get(BTHOME_SERVICE_UUID)

    if bthome_data is None:
        # 没有 BTHome Service Data | No BTHome Service Data
        _LOGGER.debug("No BTHome Service Data")
        return None

    _LOGGER.debug("BTHome Service Data: %s", bthome_data.hex())

    # 解析 BTHome 数据 | Parse BTHome data
    sensors, events, is_v2, is_encrypted = parse_bthome_data(bthome_data)

    # 检查是否有控制服务（即使没有传感器也可以发现）| Check for control service (even without sensors)
    has_control_service = False
    if service_info.service_uuids:
        from .const import SEEED_CONTROL_SERVICE_UUID
        for uuid in service_info.service_uuids:
            if SEEED_CONTROL_SERVICE_UUID.lower() in uuid.lower():
                has_control_service = True
                break

    # 如果没有传感器/事件，也没有控制服务，则不是有效设备
    # If no sensors/events and no control service, not a valid device
    if not sensors and not events and not has_control_service and not is_v2:
        _LOGGER.debug("No sensors, events, or control service found")
        return None

    # 创建设备对象 | Create device object
    device = SeeedBLEDevice(
        address=service_info.address,
        name=service_info.name or f"Seeed BLE {short_address(service_info.address)}",
        rssi=service_info.rssi,
        sensors=sensors,
        events=events,
        is_bthome_v2=is_v2,
        is_encrypted=is_encrypted,
        raw_data=bthome_data,
    )

    # 发现 Seeed BLE 设备 | Found Seeed BLE device
    _LOGGER.info(
        "Found Seeed BLE device: %s (%s), sensors: %d, events: %d, control_service: %s",
        device.name,
        device.short_address,
        len(device.sensors),
        len(device.events),
        has_control_service,
    )

    return device


def is_seeed_ble_device(service_info: BluetoothServiceInfoBleak) -> bool:
    """
    检查是否是 Seeed BLE 设备
    Check if this is a Seeed BLE device.

    通过以下方式判断：
    1. 包含 BTHome Service UUID (0xFCD2) 的 Service Data
    2. 或者包含 Seeed Manufacturer ID (0x5EED) 的 Manufacturer Data

    参数 | Args:
        service_info: 蓝牙服务信息

    返回 | Returns:
        bool: 是否是 Seeed BLE 设备
    """
    # 检查 BTHome Service Data
    if BTHOME_SERVICE_UUID in service_info.service_data:
        return True

    # 检查 Manufacturer Data
    if SEEED_MANUFACTURER_ID in service_info.manufacturer_data:
        return True

    return False

