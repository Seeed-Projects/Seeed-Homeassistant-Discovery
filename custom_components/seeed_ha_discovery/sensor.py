"""
Seeed HA Discovery - 传感器平台
Seeed HA Discovery - Sensor platform.

这个模块实现传感器实体，支持：
This module implements sensor entities, supporting:
1. WiFi 设备：通过 WebSocket 接收数据
   WiFi devices: Receive data via WebSocket
2. BLE 设备：通过蓝牙被动监听广播数据
   BLE devices: Passively listen to Bluetooth broadcast data

WiFi 传感器工作流程 | WiFi sensor workflow:
1. 设备通过 WebSocket 发送 discovery 消息，报告其传感器列表
   Device sends discovery message via WebSocket, reporting its sensor list
2. 本模块根据 discovery 创建对应的传感器实体
   This module creates corresponding sensor entities based on discovery
3. 设备持续发送 state 消息更新传感器值
   Device continuously sends state messages to update sensor values
4. 实体自动刷新 UI 显示
   Entities automatically refresh UI display

BLE 传感器工作流程 | BLE sensor workflow:
1. 设备广播 BTHome 格式数据
   Device broadcasts BTHome format data
2. HA 蓝牙适配器接收广播
   HA Bluetooth adapter receives broadcast
3. 解析数据并更新传感器状态
   Parse data and update sensor state

传感器数据格式示例（WiFi）| Sensor data format example (WiFi):
{
    "id": "temperature",
    "name": "Temperature",
    "type": "sensor",
    "device_class": "temperature",
    "unit_of_measurement": "°C",
    "state_class": "measurement",
    "precision": 1,
    "state": 25.5
}
"""
from __future__ import annotations

import logging
from typing import Any

from homeassistant.components.sensor import (
    SensorEntity,
    SensorDeviceClass,
    SensorStateClass,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import (
    DOMAIN,
    MANUFACTURER,
    CONF_DEVICE_ID,
    CONF_MODEL,
    CONF_CONNECTION_TYPE,
    CONF_BLE_ADDRESS,
    CONNECTION_TYPE_BLE,
    CONNECTION_TYPE_WIFI,
)

# 创建日志记录器
_LOGGER = logging.getLogger(__name__)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    设置传感器平台
    Set up Seeed HA sensors.

    这个函数在集成加载时被调用，根据连接类型选择不同的设置方式。

    参数 | Args:
        hass: Home Assistant 实例
        entry: 配置入口
        async_add_entities: 添加实体的回调函数
    """
    # 获取连接类型
    connection_type = entry.data.get(CONF_CONNECTION_TYPE, CONNECTION_TYPE_WIFI)

    if connection_type == CONNECTION_TYPE_BLE:
        # BLE 设备传感器设置
        await _async_setup_ble_sensors(hass, entry, async_add_entities)
    else:
        # WiFi 设备传感器设置
        await _async_setup_wifi_sensors(hass, entry, async_add_entities)


async def _async_setup_ble_sensors(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    设置 BLE 设备传感器
    Set up BLE device sensors.
    """
    from .ble_sensor import async_setup_ble_sensors

    await async_setup_ble_sensors(hass, entry, async_add_entities)


async def _async_setup_wifi_sensors(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    设置 WiFi 设备传感器
    Set up WiFi device sensors.
    """
    from .coordinator import SeeedHACoordinator

    # 获取设备数据
    data = hass.data[DOMAIN][entry.entry_id]
    coordinator: SeeedHACoordinator = data["coordinator"]

    # 设置 WiFi 传感器平台 | Setting up WiFi sensor platform
    _LOGGER.info("Setting up WiFi sensor platform, device: %s", entry.data.get(CONF_DEVICE_ID))

    # 创建已发现的传感器实体
    entities = []
    for entity_id, entity_config in coordinator.device.entities.items():
        # 只处理 sensor 类型的实体 | Only process sensor type entities
        if entity_config.get("type") == "sensor":
            _LOGGER.info("Creating sensor: %s (%s)", entity_id, entity_config.get("name"))
            entities.append(SeeedHASensor(coordinator, entity_config, entry))

    # 添加实体到 HA | Add entities to HA
    if entities:
        async_add_entities(entities)
        _LOGGER.info("Added %d sensors", len(entities))

    # 注册发现回调，处理后续发现的传感器 | Register discovery callback
    def handle_discovery(data: dict[str, Any]) -> None:
        """
        处理新发现的传感器
        Handle newly discovered sensors.

        当设备报告新的传感器时，动态创建实体。
        Dynamically create entities when device reports new sensors.
        """
        new_entities = []

        for entity_id, entity_config in coordinator.device.entities.items():
            if entity_config.get("type") == "sensor":
                # 检查实体是否已存在 | Check if entity already exists
                existing_ids = [e._entity_id for e in entities]
                if entity_id not in existing_ids:
                    _LOGGER.info("Discovered new sensor: %s", entity_id)
                    new_entity = SeeedHASensor(coordinator, entity_config, entry)
                    entities.append(new_entity)
                    new_entities.append(new_entity)

        if new_entities:
            async_add_entities(new_entities)
            _LOGGER.info("Dynamically added %d new sensors", len(new_entities))

    # 注册回调
    coordinator.device.add_discovery_callback(handle_discovery)


class SeeedHASensor(CoordinatorEntity, SensorEntity):
    """
    Seeed HA WiFi 传感器实体
    Representation of a Seeed HA WiFi sensor.

    这个类代表一个 WiFi 设备的传感器实体（如温度、湿度）。
    继承自：
    - CoordinatorEntity: 自动监听协调器的数据更新
    - SensorEntity: HA 传感器实体基类

    主要功能：
    - 自动更新传感器值
    - 显示设备信息
    - 支持设备类别、单位、精度等属性
    """

    # 使用实体名称而不是完整 ID
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator,
        entity_config: dict[str, Any],
        entry: ConfigEntry,
    ) -> None:
        """
        初始化传感器
        Initialize the sensor.

        参数 | Args:
            coordinator: 数据协调器
            entity_config: 实体配置（来自设备发现）
                          包含: id, name, device_class, unit, state_class, precision, state
            entry: 配置入口
        """
        # 调用父类初始化，注册到协调器
        super().__init__(coordinator)

        self._entry = entry
        self._entity_config = entity_config

        # 实体 ID（设备上报的 ID）
        self._entity_id = entity_config.get("id", "")

        # =====================================================================
        # 设置实体属性 | Set entity attributes
        # =====================================================================

        # 实体名称 - 显示在 UI 上
        self._attr_name = entity_config.get("name", self._entity_id)

        # 唯一 ID - 用于 HA 内部识别
        # 格式: {device_id}_{entity_id}
        device_id = entry.data.get(CONF_DEVICE_ID, "")
        self._attr_unique_id = f"{device_id}_{self._entity_id}"

        # =====================================================================
        # 设置传感器特定属性 | Set sensor-specific attributes
        # =====================================================================

        # 设备类别 - 如 temperature, humidity | Device class
        # 影响 UI 显示的图标和格式 | Affects UI icon and formatting
        device_class = entity_config.get("device_class")
        if device_class:
            try:
                self._attr_device_class = SensorDeviceClass(device_class)
                _LOGGER.debug("Set device class: %s", device_class)
            except ValueError:
                _LOGGER.warning("Unknown device class: %s", device_class)

        # 状态类别 - 如 measurement, total, total_increasing
        # 影响历史记录和统计
        state_class = entity_config.get("state_class", "measurement")
        if state_class:
            try:
                self._attr_state_class = SensorStateClass(state_class)
            except ValueError:
                self._attr_state_class = SensorStateClass.MEASUREMENT

        # 单位 - 如 °C, % | Unit
        if unit := entity_config.get("unit_of_measurement"):
            self._attr_native_unit_of_measurement = unit
            _LOGGER.debug("Set unit: %s", unit)

        # 精度 - 小数位数 | Precision - decimal places
        if precision := entity_config.get("precision"):
            self._attr_suggested_display_precision = precision
            _LOGGER.debug("Set precision: %d", precision)

        # 图标 - 如 mdi:thermometer
        if icon := entity_config.get("icon"):
            self._attr_icon = icon

        # 传感器初始化完成 | Sensor initialization complete
        _LOGGER.info(
            "Sensor initialized: %s (class=%s, unit=%s)",
            self._attr_name,
            device_class,
            entity_config.get("unit_of_measurement"),
        )

    @property
    def device_info(self) -> DeviceInfo:
        """
        返回设备信息
        Return device info.

        这个属性定义了设备在 HA 设备页面的显示信息。
        同一设备的所有实体共享相同的设备信息。
        """
        device_data = self.coordinator.device.device_info
        entry_data = self._entry.data

        return DeviceInfo(
            # 设备标识符 - 用于关联实体到设备 | Device identifier
            identifiers={(DOMAIN, entry_data.get(CONF_DEVICE_ID, ""))},
            # 设备名称 | Device name
            name=device_data.get("name", "Seeed HA Device"),
            # 制造商 | Manufacturer
            manufacturer=MANUFACTURER,
            # 设备型号 | Device model
            model=entry_data.get(CONF_MODEL, device_data.get("model", "ESP32")),
            # 固件版本 | Firmware version
            sw_version=device_data.get("version", "1.0.0"),
        )

    @property
    def available(self) -> bool:
        """
        返回实体是否可用
        Return if entity is available.

        当设备断开连接时，实体显示为不可用。
        """
        return self.coordinator.device.connected

    @property
    def native_value(self) -> Any:
        """
        返回传感器的当前值
        Return the state of the sensor.

        这是传感器最重要的属性，返回当前测量值。
        值从协调器的设备数据中获取。
        """
        entities = self.coordinator.device.entities

        if self._entity_id in entities:
            value = entities[self._entity_id].get("state")
            # 传感器当前值 | Sensor current value
            _LOGGER.debug("Sensor %s current value: %s", self._entity_id, value)
            return value

        return None

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """
        返回额外的状态属性
        Return extra state attributes.

        这些属性会显示在实体的属性面板中。
        可以包含如最后更新时间、原始数据等信息。
        """
        entities = self.coordinator.device.entities

        if self._entity_id in entities:
            return entities[self._entity_id].get("attributes", {})

        return {}
