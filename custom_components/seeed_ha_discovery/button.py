"""Button platform for IR Mate: learn gestures and open appliance setup."""

from __future__ import annotations

from typing import Any

from homeassistant.components.button import ButtonEntity
from homeassistant.components import persistent_notification
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import EntityCategory
from homeassistant.core import HomeAssistant, callback
from homeassistant.exceptions import HomeAssistantError
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
    ("quadruple", "Learn quadruple tap", "mdi:gesture-tap-button"),
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
        entities: list[ButtonEntity] = [
            SeeedIRAddApplianceButton(coordinator, entry)
        ]
        entities.extend(
            SeeedIRLearnButton(coordinator, entry, manager, gesture, label, icon)
            for gesture, label, icon in GESTURE_LEARN_BUTTONS
        )
        async_add_entities(entities)
        added = True

    add_if_supported()

    @callback
    def handle_discovery(_data: dict[str, Any]) -> None:
        """Create the learn buttons when IR support is reported later."""
        add_if_supported()

    coordinator.device.add_discovery_callback(handle_discovery)


class SeeedIRAddApplianceButton(CoordinatorEntity, ButtonEntity):
    """A shortcut that guides the user to the appliance setup flow."""

    _attr_has_entity_name = True
    _attr_entity_category = EntityCategory.CONFIG
    _attr_icon = "mdi:plus-box"
    _attr_name = "Add or manage appliances"

    def __init__(
        self,
        coordinator: SeeedHACoordinator,
        entry: ConfigEntry,
    ) -> None:
        """Initialize the appliance setup shortcut button."""
        super().__init__(coordinator)
        self._entry = entry
        device_id = entry.data.get(CONF_DEVICE_ID, "")
        self._attr_unique_id = f"{device_id}_ir_add_appliance"

    async def async_press(self) -> None:
        """Point the user to the brand catalog in the setup flow."""
        message = (
            "To add another brand (air conditioner, TV, fan, ...):\n\n"
            "1. Open [Seeed HA Discovery]"
            "(/config/integrations/integration/seeed_ha_discovery).\n"
            "2. On the IR Mate entry, select Configure.\n"
            "3. Choose Add appliance, pick the brand and model, then submit.\n\n"
            "Air conditioners appear as a climate card in Controls. For other "
            "remotes, use the Learn buttons to capture individual keys."
        )
        persistent_notification.async_create(
            self.hass,
            message,
            title="Add an appliance to IR Mate",
            notification_id=f"{self._attr_unique_id}_help",
        )

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


class SeeedIRLearnButton(CoordinatorEntity, ButtonEntity):
    """A button that learns an IR signal and binds it to one gesture."""

    _attr_has_entity_name = True
    _attr_entity_category = EntityCategory.CONFIG

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
        """Learn a signal, store it, and report the result to the user."""
        self._notify(
            title=f"{self._attr_name} started",
            message=(
                "IR Mate shows a white light. Point the remote at it and press "
                "the key once. When the white light returns, press the SAME key "
                "again. Two matching presses save the signal (green light); a "
                "mismatch keeps the previous binding (red light)."
            ),
        )
        try:
            await self._manager.async_learn_gesture(self._gesture, LEARN_TIMEOUT)
        except HomeAssistantError as err:
            self._notify(
                title=f"{self._attr_name} failed",
                message=(
                    f"The signal was not saved ({err}). This gesture keeps its "
                    "previous binding. Point the remote at IR Mate and, on each "
                    "white light, press the SAME key once (twice in total)."
                ),
            )
            raise
        record = self._manager.get_last_learned()
        if record:
            preview = ", ".join(str(value) for value in record["timings"][:8])
            message = (
                f"Verified with two matching presses: {record['pulse_count']} "
                f"pulses @{record['carrier_frequency']}Hz saved to this gesture. "
                f"Waveform starts with [{preview}, ...]."
            )
        else:
            message = "The signal was verified and saved to this gesture."
        self._notify(title=f"{self._attr_name} succeeded", message=message)

    def _notify(self, *, title: str, message: str) -> None:
        """Raise a persistent notification about the learning result."""
        persistent_notification.async_create(
            self.hass,
            message,
            title=title,
            notification_id=f"{self._attr_unique_id}_result",
        )

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
