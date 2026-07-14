"""Universal remote platform with persistent raw infrared learning."""

from __future__ import annotations

import asyncio
from collections.abc import Iterable
import logging
from typing import Any

from homeassistant.components import persistent_notification
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
from homeassistant.core import CALLBACK_TYPE, HomeAssistant, callback
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddConfigEntryEntitiesCallback
from homeassistant.helpers.storage import Store
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import (
    CONF_CONNECTION_TYPE,
    CONF_DEVICE_ID,
    CONF_MODEL,
    CONNECTION_TYPE_WIFI,
    DEFAULT_IR_CARRIER_FREQUENCY,
    DOMAIN,
    MANUFACTURER,
)
from .coordinator import SeeedHACoordinator

_LOGGER = logging.getLogger(__name__)

STORAGE_VERSION = 1
DEFAULT_SUBDEVICE = "default"
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

        device_id = entry.data.get(CONF_DEVICE_ID, entry.entry_id)
        store = Store(hass, STORAGE_VERSION, f"{DOMAIN}_{device_id}_ir_codes")
        async_add_entities([SeeedHAUniversalRemote(coordinator, entry, store)])
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
        store: Store,
    ) -> None:
        """Initialize the universal remote."""
        super().__init__(coordinator)
        self._entry = entry
        self._store = store
        self._storage_loaded = False
        self._codes: dict[str, dict[str, list[dict[str, Any]]]] = {}
        self._alternative_indexes: dict[str, int] = {}
        self._learning_lock = asyncio.Lock()
        self._learning_future: asyncio.Future[dict[str, Any]] | None = None
        self._remove_receive_callback: CALLBACK_TYPE | None = None

        device_id = entry.data.get(CONF_DEVICE_ID, "")
        self._attr_unique_id = f"{device_id}_universal_remote"
        self._attr_is_on = True
        self._notification_id = f"{DOMAIN}_{device_id}_learn_command"

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

    async def async_added_to_hass(self) -> None:
        """Subscribe to infrared receive events."""
        await super().async_added_to_hass()
        self._remove_receive_callback = (
            self.coordinator.device.add_infrared_receive_callback(
                self._handle_device_signal
            )
        )

    async def async_will_remove_from_hass(self) -> None:
        """Remove the infrared receive subscription."""
        if self._remove_receive_callback is not None:
            self._remove_receive_callback()
            self._remove_receive_callback = None
        if self._learning_future is not None and not self._learning_future.done():
            self._learning_future.cancel()
        await super().async_will_remove_from_hass()

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
        await self._async_load_storage()

        commands = list(command)
        subdevice = kwargs.get(ATTR_DEVICE) or DEFAULT_SUBDEVICE
        repeat_count = kwargs.get(ATTR_NUM_REPEATS, DEFAULT_NUM_REPEATS)
        delay = kwargs.get(ATTR_DELAY_SECS, DEFAULT_DELAY_SECS)
        if not commands:
            raise HomeAssistantError("At least one command is required")
        if repeat_count < 1:
            raise HomeAssistantError("Repeat count must be at least one")

        records: list[tuple[str, list[dict[str, Any]]]] = []
        for command_name in commands:
            learned = self._codes.get(subdevice, {}).get(command_name)
            if not learned:
                raise HomeAssistantError(
                    f"Command '{command_name}' was not learned for device '{subdevice}'"
                )
            records.append((command_name, learned))

        transmitted = 0
        for _repeat_index in range(repeat_count):
            for command_name, alternatives in records:
                if transmitted:
                    await asyncio.sleep(delay)

                key = f"{subdevice}:{command_name}"
                alternative_index = self._alternative_indexes.get(key, 0)
                signal = alternatives[alternative_index % len(alternatives)]
                try:
                    await self.coordinator.device.async_transmit_infrared(
                        int(signal["carrier_frequency"]),
                        list(signal["timings"]),
                    )
                except (
                    ConnectionError,
                    RuntimeError,
                    TimeoutError,
                    ValueError,
                ) as err:
                    raise HomeAssistantError(str(err)) from err

                if len(alternatives) > 1:
                    self._alternative_indexes[key] = alternative_index + 1
                transmitted += 1

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

        await self._async_load_storage()
        subdevice = kwargs.get(ATTR_DEVICE) or DEFAULT_SUBDEVICE
        alternative = bool(kwargs.get(ATTR_ALTERNATIVE, False))
        timeout = int(kwargs.get(ATTR_TIMEOUT, DEFAULT_LEARNING_TIMEOUT))

        async with self._learning_lock:
            for command_name in commands:
                signals = [await self._async_capture_signal(command_name, timeout)]
                if alternative:
                    signals.append(
                        await self._async_capture_signal(command_name, timeout, True)
                    )
                self._codes.setdefault(subdevice, {})[command_name] = signals

            await self._store.async_save(self._codes)

    async def async_delete_command(self, **kwargs: Any) -> None:
        """Delete learned commands from persistent storage."""
        await self._async_load_storage()
        commands = list(kwargs.get(ATTR_COMMAND) or [])
        subdevice = kwargs.get(ATTR_DEVICE) or DEFAULT_SUBDEVICE
        stored_commands = self._codes.get(subdevice, {})

        for command_name in commands:
            stored_commands.pop(command_name, None)
            self._alternative_indexes.pop(f"{subdevice}:{command_name}", None)
        if not stored_commands:
            self._codes.pop(subdevice, None)
        await self._store.async_save(self._codes)

    async def _async_load_storage(self) -> None:
        """Load learned commands once from Home Assistant storage."""
        if self._storage_loaded:
            return
        stored = await self._store.async_load()
        if isinstance(stored, dict):
            self._codes = stored
        self._storage_loaded = True

    async def _async_capture_signal(
        self,
        command_name: str,
        timeout: int,
        alternative: bool = False,
    ) -> dict[str, Any]:
        """Wait for the next raw infrared signal from the device."""
        prompt = f"Press the '{command_name}' button"
        if alternative:
            prompt += " again"
        persistent_notification.async_create(
            self.hass,
            f"{prompt} within {timeout} seconds.",
            title="Learn infrared command",
            notification_id=self._notification_id,
        )

        self._learning_future = asyncio.get_running_loop().create_future()
        try:
            data = await asyncio.wait_for(self._learning_future, timeout=timeout)
        except TimeoutError as err:
            raise HomeAssistantError(
                f"No infrared signal was received within {timeout} seconds"
            ) from err
        finally:
            self._learning_future = None
            persistent_notification.async_dismiss(
                self.hass,
                notification_id=self._notification_id,
            )

        timings = data["timings"]
        carrier_frequency = data.get("carrier_frequency", DEFAULT_IR_CARRIER_FREQUENCY)
        return {
            "carrier_frequency": carrier_frequency,
            "timings": timings,
        }

    @callback
    def _handle_device_signal(self, data: dict[str, Any]) -> None:
        """Complete an active learning request with a valid raw signal."""
        future = self._learning_future
        timings = data.get("timings")
        if (
            future is None
            or future.done()
            or not isinstance(timings, list)
            or not timings
            or not all(isinstance(value, int) for value in timings)
        ):
            return
        future.set_result(data)
