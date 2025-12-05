"""
Seeed HA Discovery - 开关平台
Seeed HA Discovery - Switch platform.

这个模块实现开关实体，支持：
This module implements switch entities, supporting:
1. WiFi 设备：通过 WebSocket 发送控制命令
   WiFi devices: Send control commands via WebSocket
2. BLE 设备：通过 GATT 发送控制命令
   BLE devices: Send control commands via GATT

WiFi 工作流程 | WiFi workflow:
1. 设备通过 WebSocket 发送 discovery 消息，报告其开关列表
   Device sends discovery message via WebSocket, reporting its switch list
2. 本模块根据 discovery 创建对应的开关实体
   This module creates corresponding switch entities based on discovery
3. 用户在 HA 界面操作开关时，发送 command 消息到设备
   When user operates switch in HA interface, send command message to device
4. 设备执行操作后，发送 state 消息确认新状态
   After device executes operation, send state message to confirm new state

BLE 工作流程 | BLE workflow:
1. 设备广播 GATT 控制服务
   Device broadcasts GATT control service
2. HA 连接设备并发现开关
   HA connects to device and discovers switches
3. 用户操作时，HA 通过 GATT 写入命令
   When user operates, HA writes command via GATT
4. 设备执行后通过 GATT 通知返回状态
   After device executes, return state via GATT notification
"""
from __future__ import annotations

import logging
from typing import Any

from homeassistant.components.switch import SwitchEntity
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
    CONF_BLE_CONTROL,
    CONNECTION_TYPE_BLE,
    CONNECTION_TYPE_WIFI,
)

_LOGGER = logging.getLogger(__name__)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    设置开关平台
    Set up Seeed HA switches.

    根据连接类型选择不同的设置方式。
    """
    connection_type = entry.data.get(CONF_CONNECTION_TYPE, CONNECTION_TYPE_WIFI)

    if connection_type == CONNECTION_TYPE_BLE:
        # BLE 设备开关设置
        ble_control = entry.data.get(CONF_BLE_CONTROL, False)
        if ble_control:
            await _async_setup_ble_switches(hass, entry, async_add_entities)
    else:
        # WiFi 设备开关设置
        await _async_setup_wifi_switches(hass, entry, async_add_entities)


async def _async_setup_ble_switches(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    设置 BLE 设备开关
    Set up BLE device switches.
    """
    from .ble_switch import async_setup_ble_switches

    manager = await async_setup_ble_switches(hass, entry, async_add_entities)

    # 保存管理器引用
    if manager:
        hass.data[DOMAIN][entry.entry_id]["ble_manager"] = manager


async def _async_setup_wifi_switches(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    设置 WiFi 设备开关
    Set up WiFi device switches.
    """
    from .coordinator import SeeedHACoordinator

    data = hass.data[DOMAIN][entry.entry_id]
    coordinator: SeeedHACoordinator = data["coordinator"]

    # 设置 WiFi 开关平台 | Setting up WiFi switch platform
    _LOGGER.info("Setting up WiFi switch platform, device: %s", entry.data.get(CONF_DEVICE_ID))

    entities = []
    for entity_id, entity_config in coordinator.device.entities.items():
        if entity_config.get("type") == "switch":
            _LOGGER.info("Creating switch: %s (%s)", entity_id, entity_config.get("name"))
            entities.append(SeeedHASwitch(coordinator, entity_config, entry))

    if entities:
        async_add_entities(entities)
        _LOGGER.info("Added %d switches", len(entities))

    def handle_discovery(data: dict[str, Any]) -> None:
        """
        处理新发现的开关
        Handle newly discovered switches.
        """
        new_entities = []

        for entity_id, entity_config in coordinator.device.entities.items():
            if entity_config.get("type") == "switch":
                existing_ids = [e._entity_id for e in entities]
                if entity_id not in existing_ids:
                    _LOGGER.info("Discovered new switch: %s", entity_id)
                    new_entity = SeeedHASwitch(coordinator, entity_config, entry)
                    entities.append(new_entity)
                    new_entities.append(new_entity)

        if new_entities:
            async_add_entities(new_entities)
            _LOGGER.info("Dynamically added %d new switches", len(new_entities))

    coordinator.device.add_discovery_callback(handle_discovery)


class SeeedHASwitch(CoordinatorEntity, SwitchEntity):
    """
    Seeed HA WiFi 开关实体
    Representation of a Seeed HA WiFi switch.
    """

    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator,
        entity_config: dict[str, Any],
        entry: ConfigEntry,
    ) -> None:
        """
        初始化开关
        Initialize switch.
        """
        super().__init__(coordinator)

        self._entry = entry
        self._entity_config = entity_config
        self._entity_id = entity_config.get("id", "")

        self._attr_name = entity_config.get("name", self._entity_id)
        device_id = entry.data.get(CONF_DEVICE_ID, "")
        self._attr_unique_id = f"{device_id}_{self._entity_id}"

        if icon := entity_config.get("icon"):
            self._attr_icon = icon

        # 开关初始化完成 | Switch initialization complete
        _LOGGER.info("Switch initialized: %s (icon=%s)", self._attr_name, entity_config.get("icon"))

    @property
    def device_info(self) -> DeviceInfo:
        """
        返回设备信息
        Return device info.
        """
        device_data = self.coordinator.device.device_info
        entry_data = self._entry.data

        return DeviceInfo(
            identifiers={(DOMAIN, entry_data.get(CONF_DEVICE_ID, ""))},
            name=device_data.get("name", "Seeed HA Device"),
            manufacturer=MANUFACTURER,
            model=entry_data.get(CONF_MODEL, device_data.get("model", "ESP32")),
            sw_version=device_data.get("version", "1.0.0"),
        )

    @property
    def available(self) -> bool:
        """
        返回实体是否可用
        Return if entity is available.
        """
        return self.coordinator.device.connected

    @property
    def is_on(self) -> bool:
        """
        返回开关的当前状态
        Return current switch state.
        """
        entities = self.coordinator.device.entities

        if self._entity_id in entities:
            state = entities[self._entity_id].get("state")
            # 开关当前状态 | Switch current state
            _LOGGER.debug("Switch %s current state: %s", self._entity_id, state)
            return bool(state)

        return False

    async def async_turn_on(self, **kwargs: Any) -> None:
        """
        打开开关
        Turn on the switch.
        """
        _LOGGER.info("Sending switch command: %s -> turn_on", self._entity_id)
        await self.coordinator.device.async_send_command(
            self._entity_id,
            command="turn_on"
        )

    async def async_turn_off(self, **kwargs: Any) -> None:
        """
        关闭开关
        Turn off the switch.
        """
        _LOGGER.info("Sending switch command: %s -> turn_off", self._entity_id)
        await self.coordinator.device.async_send_command(
            self._entity_id,
            command="turn_off"
        )

    async def async_toggle(self, **kwargs: Any) -> None:
        """
        切换开关状态
        Toggle the switch.
        """
        _LOGGER.info("Sending switch command: %s -> toggle", self._entity_id)
        await self.coordinator.device.async_send_command(
            self._entity_id,
            command="toggle"
        )

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """
        返回额外的状态属性
        Return extra state attributes.
        """
        entities = self.coordinator.device.entities

        if self._entity_id in entities:
            return entities[self._entity_id].get("attributes", {})

        return {}
