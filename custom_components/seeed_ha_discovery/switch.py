"""
Seeed HA Discovery - 开关平台
Switch platform for Seeed HA Discovery.

这个模块实现开关实体，支持：
1. WiFi 设备：通过 WebSocket 发送控制命令
2. BLE 设备：通过 GATT 发送控制命令

WiFi 工作流程：
1. 设备通过 WebSocket 发送 discovery 消息，报告其开关列表
2. 本模块根据 discovery 创建对应的开关实体
3. 用户在 HA 界面操作开关时，发送 command 消息到设备
4. 设备执行操作后，发送 state 消息确认新状态

BLE 工作流程：
1. 设备广播 GATT 控制服务
2. HA 连接设备并发现开关
3. 用户操作时，HA 通过 GATT 写入命令
4. 设备执行后通过 GATT 通知返回状态
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
    """设置 BLE 设备开关"""
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
    """设置 WiFi 设备开关"""
    from .coordinator import SeeedHACoordinator

    data = hass.data[DOMAIN][entry.entry_id]
    coordinator: SeeedHACoordinator = data["coordinator"]

    _LOGGER.info("设置 WiFi 开关平台，设备: %s", entry.data.get(CONF_DEVICE_ID))

    entities = []
    for entity_id, entity_config in coordinator.device.entities.items():
        if entity_config.get("type") == "switch":
            _LOGGER.info("创建开关: %s (%s)", entity_id, entity_config.get("name"))
            entities.append(SeeedHASwitch(coordinator, entity_config, entry))

    if entities:
        async_add_entities(entities)
        _LOGGER.info("已添加 %d 个开关", len(entities))

    def handle_discovery(data: dict[str, Any]) -> None:
        """处理新发现的开关"""
        new_entities = []

        for entity_id, entity_config in coordinator.device.entities.items():
            if entity_config.get("type") == "switch":
                existing_ids = [e._entity_id for e in entities]
                if entity_id not in existing_ids:
                    _LOGGER.info("发现新开关: %s", entity_id)
                    new_entity = SeeedHASwitch(coordinator, entity_config, entry)
                    entities.append(new_entity)
                    new_entities.append(new_entity)

        if new_entities:
            async_add_entities(new_entities)
            _LOGGER.info("动态添加 %d 个新开关", len(new_entities))

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
        """初始化开关"""
        super().__init__(coordinator)

        self._entry = entry
        self._entity_config = entity_config
        self._entity_id = entity_config.get("id", "")

        self._attr_name = entity_config.get("name", self._entity_id)
        device_id = entry.data.get(CONF_DEVICE_ID, "")
        self._attr_unique_id = f"{device_id}_{self._entity_id}"

        if icon := entity_config.get("icon"):
            self._attr_icon = icon

        _LOGGER.info("开关初始化完成: %s (图标=%s)", self._attr_name, entity_config.get("icon"))

    @property
    def device_info(self) -> DeviceInfo:
        """返回设备信息"""
        device_data = self.coordinator.device.device_info
        entry_data = self._entry.data

        return DeviceInfo(
            identifiers={(DOMAIN, entry_data.get(CONF_DEVICE_ID, ""))},
            name=device_data.get("name", "Seeed HA 设备"),
            manufacturer=MANUFACTURER,
            model=entry_data.get(CONF_MODEL, device_data.get("model", "ESP32")),
            sw_version=device_data.get("version", "1.0.0"),
        )

    @property
    def available(self) -> bool:
        """返回实体是否可用"""
        return self.coordinator.device.connected

    @property
    def is_on(self) -> bool:
        """返回开关的当前状态"""
        entities = self.coordinator.device.entities

        if self._entity_id in entities:
            state = entities[self._entity_id].get("state")
            _LOGGER.debug("开关 %s 当前状态: %s", self._entity_id, state)
            return bool(state)

        return False

    async def async_turn_on(self, **kwargs: Any) -> None:
        """打开开关"""
        _LOGGER.info("发送开关命令: %s -> turn_on", self._entity_id)
        await self.coordinator.device.async_send_command(
            self._entity_id,
            command="turn_on"
        )

    async def async_turn_off(self, **kwargs: Any) -> None:
        """关闭开关"""
        _LOGGER.info("发送开关命令: %s -> turn_off", self._entity_id)
        await self.coordinator.device.async_send_command(
            self._entity_id,
            command="turn_off"
        )

    async def async_toggle(self, **kwargs: Any) -> None:
        """切换开关状态"""
        _LOGGER.info("发送开关命令: %s -> toggle", self._entity_id)
        await self.coordinator.device.async_send_command(
            self._entity_id,
            command="toggle"
        )

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """返回额外的状态属性"""
        entities = self.coordinator.device.entities

        if self._entity_id in entities:
            return entities[self._entity_id].get("attributes", {})

        return {}
