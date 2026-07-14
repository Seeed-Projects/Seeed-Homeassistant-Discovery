"""Infrared emitter and receiver platform for Seeed HA devices."""

from __future__ import annotations

import logging
from typing import Any

from homeassistant.components.infrared import (
    InfraredCommand,
    InfraredEmitterEntity,
    InfraredReceivedSignal,
    InfraredReceiverEntity,
)
from homeassistant.config_entries import ConfigEntry
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

_LOGGER = logging.getLogger(__name__)

PARALLEL_UPDATES = 0


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddConfigEntryEntitiesCallback,
) -> None:
    """Set up infrared entities reported by a Wi-Fi device."""
    if (
        entry.data.get(CONF_CONNECTION_TYPE, CONNECTION_TYPE_WIFI)
        != CONNECTION_TYPE_WIFI
    ):
        return

    coordinator: SeeedHACoordinator = hass.data[DOMAIN][entry.entry_id]["coordinator"]
    entities: list[_SeeedHAInfraredEntity] = []
    known_ids: set[str] = set()

    def add_discovered_entities() -> None:
        """Create entities for newly reported infrared capabilities."""
        new_entities: list[_SeeedHAInfraredEntity] = []
        for entity_id, entity_config in coordinator.device.entities.items():
            if entity_config.get("type") != "infrared" or entity_id in known_ids:
                continue

            role = entity_config.get("role")
            if role == "emitter":
                entity = SeeedHAInfraredEmitter(coordinator, entity_config, entry)
            elif role == "receiver":
                entity = SeeedHAInfraredReceiver(coordinator, entity_config, entry)
            else:
                _LOGGER.warning("Unknown infrared role: %s", role)
                continue

            known_ids.add(entity_id)
            entities.append(entity)
            new_entities.append(entity)

        if new_entities:
            async_add_entities(new_entities)

    add_discovered_entities()

    @callback
    def handle_discovery(_data: dict[str, Any]) -> None:
        """Handle capabilities reported after platform setup."""
        add_discovered_entities()

    coordinator.device.add_discovery_callback(handle_discovery)


class _SeeedHAInfraredEntity(CoordinatorEntity):
    """Common entity data for an infrared capability."""

    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: SeeedHACoordinator,
        entity_config: dict[str, Any],
        entry: ConfigEntry,
    ) -> None:
        """Initialize common infrared entity data."""
        super().__init__(coordinator)
        self._entry = entry
        self._entity_id = entity_config.get("id", "")
        self._attr_name = entity_config.get("name", self._entity_id)
        self._attr_unique_id = f"{entry.data.get(CONF_DEVICE_ID, '')}_{self._entity_id}"

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


class SeeedHAInfraredEmitter(_SeeedHAInfraredEntity, InfraredEmitterEntity):
    """Infrared emitter backed by the device WebSocket connection."""

    async def async_send_command(self, command: InfraredCommand) -> None:
        """Send a Home Assistant infrared command as raw timings."""
        try:
            await self.coordinator.device.async_transmit_infrared(
                command.modulation,
                command.get_raw_timings(),
                command.repeat_count,
            )
        except (ConnectionError, RuntimeError, TimeoutError, ValueError) as err:
            raise HomeAssistantError(str(err)) from err


class SeeedHAInfraredReceiver(_SeeedHAInfraredEntity, InfraredReceiverEntity):
    """Infrared receiver backed by raw timing events from the device."""

    _remove_receive_callback: CALLBACK_TYPE | None = None

    async def async_added_to_hass(self) -> None:
        """Subscribe to raw infrared receive events."""
        await super().async_added_to_hass()
        self._remove_receive_callback = (
            self.coordinator.device.add_infrared_receive_callback(
                self._handle_device_signal
            )
        )

    async def async_will_remove_from_hass(self) -> None:
        """Remove the raw infrared receive subscription."""
        if self._remove_receive_callback is not None:
            self._remove_receive_callback()
            self._remove_receive_callback = None
        await super().async_will_remove_from_hass()

    @callback
    def _handle_device_signal(self, data: dict[str, Any]) -> None:
        """Forward a matching raw signal to Home Assistant."""
        if data.get("entity_id") != self._entity_id:
            return

        timings = data.get("timings")
        if not isinstance(timings, list) or not all(
            isinstance(value, int) for value in timings
        ):
            _LOGGER.warning("Received invalid infrared timings")
            return

        modulation = data.get("carrier_frequency")
        if not isinstance(modulation, int):
            modulation = None
        self._handle_received_signal(
            InfraredReceivedSignal(timings=timings, modulation=modulation)
        )
