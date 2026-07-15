"""Select platform exposing IR Mate appliance and command dropdowns."""

from __future__ import annotations

from typing import Any

from homeassistant.components.select import SelectEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import EntityCategory
from homeassistant.core import CALLBACK_TYPE, HomeAssistant, callback
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

# Placeholder option shown while the send dropdown is idle.
# 发送下拉框空闲时显示的占位选项。
IDLE_OPTION = "—"

# Touch gestures exposed as configurable bindings, with display labels.
# 作为可配置绑定暴露的触摸手势，及其显示名称。
GESTURE_LABELS: tuple[tuple[str, str, str], ...] = (
    ("single", "Single tap", "mdi:gesture-tap"),
    ("double", "Double tap", "mdi:gesture-double-tap"),
    ("triple", "Triple tap", "mdi:gesture-tap-button"),
    ("long", "Long press", "mdi:gesture-tap-hold"),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddConfigEntryEntitiesCallback,
) -> None:
    """Set up the IR Mate appliance and command select entities."""
    if (
        entry.data.get(CONF_CONNECTION_TYPE, CONNECTION_TYPE_WIFI)
        != CONNECTION_TYPE_WIFI
    ):
        return

    coordinator: SeeedHACoordinator = hass.data[DOMAIN][entry.entry_id]["coordinator"]
    added = False

    def add_if_supported() -> None:
        """Create the select entities once an IR manager is available."""
        nonlocal added
        if added:
            return
        manager: IRMateManager | None = hass.data[DOMAIN][entry.entry_id].get(
            "ir_manager"
        )
        if manager is None:
            return
        entities: list[_SeeedIRSelect] = [
            SeeedIRApplianceSelect(coordinator, entry, manager),
            SeeedIRCommandSelect(coordinator, entry, manager),
        ]
        entities.extend(
            SeeedIRGestureSelect(coordinator, entry, manager, gesture, label, icon)
            for gesture, label, icon in GESTURE_LABELS
        )
        async_add_entities(entities)
        added = True

    add_if_supported()

    @callback
    def handle_discovery(_data: dict[str, Any]) -> None:
        """Create the select entities when IR support is reported later."""
        add_if_supported()

    coordinator.device.add_discovery_callback(handle_discovery)


class _SeeedIRSelect(CoordinatorEntity, SelectEntity):
    """Common wiring for IR Mate select entities."""

    _attr_has_entity_name = True
    _remove_listener: CALLBACK_TYPE | None = None

    def __init__(
        self,
        coordinator: SeeedHACoordinator,
        entry: ConfigEntry,
        manager: IRMateManager,
    ) -> None:
        """Store the coordinator, config entry, and IR manager."""
        super().__init__(coordinator)
        self._entry = entry
        self._manager = manager

    async def async_added_to_hass(self) -> None:
        """Refresh whenever the manager reports an appliance change."""
        await super().async_added_to_hass()
        self._remove_listener = self._manager.add_update_listener(
            self._handle_manager_update
        )

    async def async_will_remove_from_hass(self) -> None:
        """Detach the manager update listener."""
        if self._remove_listener is not None:
            self._remove_listener()
            self._remove_listener = None
        await super().async_will_remove_from_hass()

    @callback
    def _handle_manager_update(self) -> None:
        """Write the latest options and selection to Home Assistant."""
        self.async_write_ha_state()

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


class SeeedIRApplianceSelect(_SeeedIRSelect):
    """Dropdown that selects the active appliance / infrared protocol."""

    _attr_name = "Appliance"
    _attr_icon = "mdi:remote"

    def __init__(
        self,
        coordinator: SeeedHACoordinator,
        entry: ConfigEntry,
        manager: IRMateManager,
    ) -> None:
        """Initialize the appliance selector."""
        super().__init__(coordinator, entry, manager)
        device_id = entry.data.get(CONF_DEVICE_ID, "")
        self._attr_unique_id = f"{device_id}_ir_appliance"

    @property
    def available(self) -> bool:
        """Allow selecting the target appliance even while offline."""
        return True

    @property
    def options(self) -> list[str]:
        """Return every configured appliance name."""
        names = [name for _id, name in self._manager.list_appliances()]
        return names or [IDLE_OPTION]

    @property
    def current_option(self) -> str | None:
        """Return the name of the currently active appliance."""
        active_id = self._manager.get_active_appliance_id()
        for appliance_id, name in self._manager.list_appliances():
            if appliance_id == active_id:
                return name
        return None

    async def async_select_option(self, option: str) -> None:
        """Set the active appliance from the selected name."""
        for appliance_id, name in self._manager.list_appliances():
            if name == option:
                await self._manager.async_set_active_appliance(appliance_id)
                self.async_write_ha_state()
                return
        raise HomeAssistantError("The selected appliance is no longer available")


class SeeedIRCommandSelect(_SeeedIRSelect):
    """Dropdown that transmits one command of the active appliance."""

    _attr_name = "Send command"
    _attr_icon = "mdi:send"

    def __init__(
        self,
        coordinator: SeeedHACoordinator,
        entry: ConfigEntry,
        manager: IRMateManager,
    ) -> None:
        """Initialize the send-command selector."""
        super().__init__(coordinator, entry, manager)
        device_id = entry.data.get(CONF_DEVICE_ID, "")
        self._attr_unique_id = f"{device_id}_ir_command"

    @property
    def available(self) -> bool:
        """Sending requires an active device connection."""
        return self.coordinator.device.connected

    @property
    def options(self) -> list[str]:
        """Return an idle placeholder plus the active appliance commands."""
        active_id = self._manager.get_active_appliance_id()
        return [IDLE_OPTION] + [
            name for _id, name in self._manager.list_sendable_commands(active_id)
        ]

    @property
    def current_option(self) -> str:
        """Always rest on the idle placeholder between transmissions."""
        return IDLE_OPTION

    async def async_select_option(self, option: str) -> None:
        """Transmit the selected command, then return to the idle option."""
        if option == IDLE_OPTION:
            return
        active_id = self._manager.get_active_appliance_id()
        for command_id, name in self._manager.list_sendable_commands(active_id):
            if name == option:
                try:
                    await self._manager.async_test_command(active_id, command_id)
                finally:
                    self.async_write_ha_state()
                return
        raise HomeAssistantError("The selected command is no longer available")


class SeeedIRGestureSelect(_SeeedIRSelect):
    """Dropdown that binds one touch gesture and syncs it to device NVS."""

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
        """Initialize the gesture binding selector."""
        super().__init__(coordinator, entry, manager)
        self._gesture = gesture
        self._attr_name = label
        self._attr_icon = icon
        device_id = entry.data.get(CONF_DEVICE_ID, "")
        self._attr_unique_id = f"{device_id}_ir_gesture_{gesture}"

    @property
    def available(self) -> bool:
        """Binding changes must reach the device, so require a connection."""
        return self.coordinator.device.connected

    @property
    def options(self) -> list[str]:
        """Return the idle placeholder plus every bindable command label."""
        return [IDLE_OPTION] + [
            label for _appliance, _command, label in self._manager.list_bindable_commands()
        ]

    @property
    def current_option(self) -> str:
        """Return the command currently bound to this gesture."""
        return self._manager.get_binding_label(self._gesture) or IDLE_OPTION

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """Expose the stored signal so the binding is not a black box."""
        return self._manager.get_binding_detail(self._gesture) or {}

    async def async_select_option(self, option: str) -> None:
        """Bind the gesture to the chosen command, or clear it."""
        if option == IDLE_OPTION:
            await self._manager.async_set_gesture_binding(self._gesture, None, None)
            self.async_write_ha_state()
            return
        for appliance_id, command_id, label in self._manager.list_bindable_commands():
            if label == option:
                await self._manager.async_set_gesture_binding(
                    self._gesture, appliance_id, command_id
                )
                self.async_write_ha_state()
                return
        raise HomeAssistantError("The selected command is no longer available")
