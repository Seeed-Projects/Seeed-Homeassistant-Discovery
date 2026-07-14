"""Universal remote platform with persistent raw infrared learning."""

from __future__ import annotations

from collections.abc import Iterable
from typing import Any

from homeassistant.components.remote import (
    ATTR_ALTERNATIVE,
    ATTR_COMMAND_TYPE,
    ATTR_DELAY_SECS,
    ATTR_DEVICE,
    ATTR_NUM_REPEATS,
    ATTR_TIMEOUT,
    DEFAULT_DELAY_SECS,
    DEFAULT_NUM_REPEATS,
    RemoteEntity,
    RemoteEntityFeature,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import ATTR_COMMAND
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
from .ir_manager import DEFAULT_APPLIANCE_ID, IRMateManager

DEFAULT_LEARNING_TIMEOUT = 30


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddConfigEntryEntitiesCallback,
) -> None:
    """Set up one learning remote for a full infrared transceiver."""
    if (
        entry.data.get(CONF_CONNECTION_TYPE, CONNECTION_TYPE_WIFI)
        != CONNECTION_TYPE_WIFI
    ):
        return

    coordinator: SeeedHACoordinator = hass.data[DOMAIN][entry.entry_id]["coordinator"]
    remote_added = False

    def add_remote_if_supported() -> None:
        """Create the learning remote after both IR roles are reported."""
        nonlocal remote_added
        if remote_added:
            return

        roles = {
            config.get("role")
            for config in coordinator.device.entities.values()
            if config.get("type") == "infrared"
        }
        if not {"emitter", "receiver"}.issubset(roles):
            return

        manager: IRMateManager = hass.data[DOMAIN][entry.entry_id]["ir_manager"]
        async_add_entities([SeeedHAUniversalRemote(coordinator, entry, manager)])
        remote_added = True

    add_remote_if_supported()

    @callback
    def handle_discovery(_data: dict[str, Any]) -> None:
        """Create the remote when delayed discovery reports IR support."""
        add_remote_if_supported()

    coordinator.device.add_discovery_callback(handle_discovery)


class SeeedHAUniversalRemote(CoordinatorEntity, RemoteEntity):
    """Universal remote that stores learned raw infrared timings."""

    _attr_has_entity_name = True
    _attr_name = "Universal remote"
    _attr_supported_features = (
        RemoteEntityFeature.LEARN_COMMAND | RemoteEntityFeature.DELETE_COMMAND
    )

    def __init__(
        self,
        coordinator: SeeedHACoordinator,
        entry: ConfigEntry,
        manager: IRMateManager,
    ) -> None:
        """Initialize the universal remote."""
        super().__init__(coordinator)
        self._entry = entry
        self._manager = manager

        device_id = entry.data.get(CONF_DEVICE_ID, "")
        self._attr_unique_id = f"{device_id}_universal_remote"
        self._attr_is_on = True

    @property
    def available(self) -> bool:
        """Return whether the device connection is available."""
        return self.coordinator.device.connected

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

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Enable remote commands."""
        self._attr_is_on = True
        self.async_write_ha_state()

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Disable remote commands."""
        self._attr_is_on = False
        self.async_write_ha_state()

    async def async_send_command(
        self,
        command: Iterable[str],
        **kwargs: Any,
    ) -> None:
        """Send stored raw infrared commands."""
        if not self._attr_is_on:
            return

        commands = list(command)
        subdevice = kwargs.get(ATTR_DEVICE) or DEFAULT_APPLIANCE_ID
        repeat_count = kwargs.get(ATTR_NUM_REPEATS, DEFAULT_NUM_REPEATS)
        delay = kwargs.get(ATTR_DELAY_SECS, DEFAULT_DELAY_SECS)
        if not commands:
            raise HomeAssistantError("At least one command is required")
        if repeat_count < 1:
            raise HomeAssistantError("Repeat count must be at least one")

        await self._manager.async_remote_send(
            subdevice,
            commands,
            repeat_count,
            delay,
        )

    async def async_learn_command(self, **kwargs: Any) -> None:
        """Learn and persist one or more raw infrared commands."""
        if not self._attr_is_on:
            return

        command_type = kwargs.get(ATTR_COMMAND_TYPE, "ir")
        if command_type != "ir":
            raise HomeAssistantError("Only infrared command learning is supported")

        commands = list(kwargs.get(ATTR_COMMAND) or [])
        if not commands:
            raise HomeAssistantError("At least one command is required")

        subdevice = kwargs.get(ATTR_DEVICE) or DEFAULT_APPLIANCE_ID
        alternative = bool(kwargs.get(ATTR_ALTERNATIVE, False))
        timeout = int(kwargs.get(ATTR_TIMEOUT, DEFAULT_LEARNING_TIMEOUT))
        await self._manager.async_remote_learn(
            subdevice,
            commands,
            timeout,
            alternative,
        )

    async def async_delete_command(self, **kwargs: Any) -> None:
        """Delete learned commands from persistent storage."""
        commands = list(kwargs.get(ATTR_COMMAND) or [])
        subdevice = kwargs.get(ATTR_DEVICE) or DEFAULT_APPLIANCE_ID
        await self._manager.async_remote_delete(subdevice, commands)
