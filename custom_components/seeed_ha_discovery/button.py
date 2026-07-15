"""Button platform to learn an IR signal straight into a touch gesture."""

from __future__ import annotations

from typing import Any

from homeassistant.components.button import ButtonEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddConfigEntryEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import (
    CONF_CONNECTION_TYPE,
    CONF_DEVICE_ID,
    CONF_MODEL,
    CONNECTION_TYPE_WIFI,
    DOMAIN,
    MANUFACTURER,
)
from .coordinator import SeeedHACoordinator
from .ir_manager import IRMateManager

# Learning timeout in seconds, matching the firmware default window.
# 学习超时(秒),与固件默认窗口一致。
LEARN_TIMEOUT = 30

# One learn button per touch gesture, with display labels and icons.
# 每个触摸手势一个学习按钮,附显示名称与图标。
GESTURE_LEARN_BUTTONS: tuple[tuple[str, str, str], ...] = (
    ("single", "Learn single tap", "mdi:gesture-tap"),
    ("double", "Learn double tap", "mdi:gesture-double-tap"),
    ("triple", "Learn triple tap", "mdi:gesture-tap-button"),
    ("long", "Learn long press", "mdi:gesture-tap-hold"),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddConfigEntryEntitiesCallback,
) -> None:
    """Set up the IR Mate gesture-learning buttons."""
    if (
        entry.data.get(CONF_CONNECTION_TYPE, CONNECTION_TYPE_WIFI)
        != CONNECTION_TYPE_WIFI
    ):
        return

    coordinator: SeeedHACoordinator = hass.data[DOMAIN][entry.entry_id]["coordinator"]
    added = False

    def add_if_supported() -> None:
        """Create the learn buttons once an IR manager is available."""
        nonlocal added
        if added:
            return
        manager: IRMateManager | None = hass.data[DOMAIN][entry.entry_id].get(
            "ir_manager"
        )
        if manager is None:
            return
        async_add_entities(
            SeeedIRLearnButton(coordinator, entry, manager, gesture, label, icon)
            for gesture, label, icon in GESTURE_LEARN_BUTTONS
        )
        added = True

    add_if_supported()

    @callback
    def handle_discovery(_data: dict[str, Any]) -> None:
        """Create the learn buttons when IR support is reported later."""
        add_if_supported()

    coordinator.device.add_discovery_callback(handle_discovery)


class SeeedIRLearnButton(CoordinatorEntity, ButtonEntity):
    """A button that learns an IR signal and binds it to one gesture."""

    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: SeeedHACoordinator,
        entry: ConfigEntry,
        manager: IRMateManager,
        gesture: str,
        label: str,
        icon: str,
    ) -> None:
        """Initialize one gesture-learning button."""
        super().__init__(coordinator)
        self._entry = entry
        self._manager = manager
        self._gesture = gesture
        self._attr_name = label
        self._attr_icon = icon
        device_id = entry.data.get(CONF_DEVICE_ID, "")
        self._attr_unique_id = f"{device_id}_ir_learn_{gesture}"

    @property
    def available(self) -> bool:
        """Learning needs an active device connection."""
        return self.coordinator.device.connected

    async def async_press(self) -> None:
        """Learn a signal and store it in this gesture's slot."""
        await self._manager.async_learn_gesture(self._gesture, LEARN_TIMEOUT)

    @property
    def device_info(self) -> DeviceInfo:
        """Return the parent device information."""
        device_data = self.coordinator.device.device_info
        entry_data = self._entry.data
        info = DeviceInfo(
            identifiers={(DOMAIN, entry_data.get(CONF_DEVICE_ID, ""))},
            name=device_data.get("name", "Seeed HA Device"),
            manufacturer=MANUFACTURER,
            model=entry_data.get(CONF_MODEL, device_data.get("model", "ESP32")),
            sw_version=device_data.get("version", "1.0.0"),
        )
        if host := entry_data.get("host"):
            info["configuration_url"] = f"http://{host}"
        return info
