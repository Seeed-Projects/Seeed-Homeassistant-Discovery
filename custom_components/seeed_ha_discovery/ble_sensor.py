"""
Seeed HA Discovery - BLE 传感器实体
BLE sensor entities for Seeed HA Discovery.

这个文件负责：
1. 创建和管理 BLE 设备的传感器实体
2. 接收蓝牙广播数据更新
3. 解析 BTHome 格式数据并更新传感器状态
"""
from __future__ import annotations

import logging
from typing import Any

from homeassistant.components import bluetooth
from homeassistant.components.bluetooth import (
    BluetoothServiceInfoBleak,
    async_register_callback,
    BluetoothChange,
    BluetoothCallbackMatcher,
)
from homeassistant.components.sensor import (
    SensorDeviceClass,
    SensorEntity,
    SensorEntityDescription,
    SensorStateClass,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import (
    DOMAIN,
    MANUFACTURER,
    CONF_BLE_ADDRESS,
    CONF_DEVICE_ID,
    CONF_MODEL,
    BTHOME_SERVICE_UUID,
)
from .bluetooth import parse_ble_advertisement, BTHomeSensorData

_LOGGER = logging.getLogger(__name__)


async def async_setup_ble_sensors(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    设置 BLE 传感器实体
    Set up BLE sensor entities.

    参数 | Args:
        hass: Home Assistant 实例
        entry: 配置入口
        async_add_entities: 添加实体的回调
    """
    ble_address = entry.data[CONF_BLE_ADDRESS]
    device_id = entry.data.get(CONF_DEVICE_ID, f"ble_{ble_address.replace(':', '_').lower()}")
    device_name = entry.title
    model = entry.data.get(CONF_MODEL, "XIAO BLE")

    _LOGGER.info("设置 BLE 传感器: %s (%s)", device_name, ble_address)

    # 存储已创建的传感器
    created_sensors: dict[str, SeeedBLESensor] = {}

    @callback
    def _async_handle_bluetooth_event(
        service_info: BluetoothServiceInfoBleak,
        change: BluetoothChange,
    ) -> None:
        """
        处理蓝牙事件
        Handle bluetooth event.
        """
        # 只处理我们的设备
        if service_info.address.upper() != ble_address.upper():
            return

        _LOGGER.debug(
            "收到 BLE 广播: %s (%s), RSSI: %d",
            service_info.name,
            service_info.address,
            service_info.rssi,
        )

        # 解析广播数据
        device = parse_ble_advertisement(service_info)
        if device is None:
            return

        # 处理传感器数据
        new_entities = []
        for sensor_data in device.sensors:
            # 创建传感器 key
            sensor_key = f"{sensor_data.object_id:02x}_{sensor_data.name.lower().replace(' ', '_')}"

            if sensor_key in created_sensors:
                # 更新现有传感器
                created_sensors[sensor_key].update_value(sensor_data.value)
            else:
                # 创建新传感器
                sensor = SeeedBLESensor(
                    entry=entry,
                    device_id=device_id,
                    device_name=device_name,
                    model=model,
                    sensor_data=sensor_data,
                    sensor_key=sensor_key,
                )
                created_sensors[sensor_key] = sensor
                new_entities.append(sensor)
                _LOGGER.info(
                    "创建 BLE 传感器: %s = %s %s",
                    sensor_data.name,
                    sensor_data.value,
                    sensor_data.unit or "",
                )

        # 添加新实体
        if new_entities:
            async_add_entities(new_entities)

        # 处理按钮事件
        if device.events:
            for event_data in device.events:
                _LOGGER.info(
                    "收到 BLE 事件: %s - %s",
                    event_data.get("type"),
                    event_data.get("event"),
                )
                # 触发 Home Assistant 事件
                hass.bus.async_fire(
                    f"{DOMAIN}_button_event",
                    {
                        "device_id": device_id,
                        "device_name": device_name,
                        "address": ble_address,
                        "event_type": event_data.get("type"),
                        "event": event_data.get("event"),
                        "value": event_data.get("value"),
                    },
                )

    # 注册蓝牙回调
    # 使用 Service Data UUID 匹配
    entry.async_on_unload(
        async_register_callback(
            hass,
            _async_handle_bluetooth_event,
            BluetoothCallbackMatcher(
                service_data_uuid=BTHOME_SERVICE_UUID,
                address=ble_address,
            ),
            bluetooth.BluetoothScanningMode.PASSIVE,
        )
    )

    _LOGGER.info("BLE 传感器设置完成，等待广播数据...")


class SeeedBLESensor(SensorEntity):
    """
    Seeed BLE 传感器实体
    Sensor entity for Seeed BLE devices.
    """

    _attr_has_entity_name = True

    def __init__(
        self,
        entry: ConfigEntry,
        device_id: str,
        device_name: str,
        model: str,
        sensor_data: BTHomeSensorData,
        sensor_key: str,
    ) -> None:
        """
        初始化 BLE 传感器
        Initialize BLE sensor.
        """
        self._entry = entry
        self._device_id = device_id
        self._device_name = device_name
        self._model = model
        self._sensor_key = sensor_key

        # 设置实体属性
        self._attr_name = sensor_data.name
        self._attr_unique_id = f"{device_id}_{sensor_key}"
        self._attr_native_value = sensor_data.value

        # 设置单位
        if sensor_data.unit:
            self._attr_native_unit_of_measurement = sensor_data.unit

        # 设置设备类别
        if sensor_data.device_class:
            try:
                self._attr_device_class = SensorDeviceClass(sensor_data.device_class)
            except ValueError:
                _LOGGER.warning("未知的设备类别: %s", sensor_data.device_class)

        # 设置状态类别
        if sensor_data.device_class:
            self._attr_state_class = SensorStateClass.MEASUREMENT

    @property
    def device_info(self) -> DeviceInfo:
        """返回设备信息"""
        return DeviceInfo(
            identifiers={(DOMAIN, self._device_id)},
            name=self._device_name,
            manufacturer=MANUFACTURER,
            model=self._model,
        )

    @callback
    def update_value(self, value: Any) -> None:
        """
        更新传感器值
        Update sensor value.
        """
        self._attr_native_value = value
        self.async_write_ha_state()
        _LOGGER.debug("更新 BLE 传感器 %s: %s", self._attr_name, value)
