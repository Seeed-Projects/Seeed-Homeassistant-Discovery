"""Climate platform for IR Mate air conditioners backed by the code library."""

from __future__ import annotations

from typing import Any

from homeassistant.components.climate import (
    ClimateEntity,
    ClimateEntityFeature,
    HVACMode,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import ATTR_TEMPERATURE, UnitOfTemperature
from homeassistant.core import CALLBACK_TYPE, HomeAssistant, callback
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

# Map the SmartIR operation-mode strings onto Home Assistant HVAC modes.
# 将 SmartIR 的运行模式字符串映射到 Home Assistant 的 HVAC 模式。
_SMARTIR_TO_HVAC: dict[str, HVACMode] = {
    "cool": HVACMode.COOL,
    "heat": HVACMode.HEAT,
    "auto": HVACMode.AUTO,
    "dry": HVACMode.DRY,
    "heat_cool": HVACMode.HEAT_COOL,
    "fan_only": HVACMode.FAN_ONLY,
    "fan": HVACMode.FAN_ONLY,
}


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddConfigEntryEntitiesCallback,
) -> None:
    """Create a climate entity for each library-backed air conditioner."""
    if (
        entry.data.get(CONF_CONNECTION_TYPE, CONNECTION_TYPE_WIFI)
        != CONNECTION_TYPE_WIFI
    ):
        return

    coordinator: SeeedHACoordinator = hass.data[DOMAIN][entry.entry_id]["coordinator"]
    known: set[str] = set()

    @callback
    def sync_entities() -> None:
        """Add climate entities for any newly created climate appliances."""
        manager: IRMateManager | None = hass.data[DOMAIN][entry.entry_id].get(
            "ir_manager"
        )
        if manager is None:
            return
        new_ids = [
            appliance_id
            for appliance_id in manager.list_climate_appliances()
            if appliance_id not in known
        ]
        if not new_ids:
            return
        known.update(new_ids)
        async_add_entities(
            SeeedIRClimate(coordinator, entry, manager, appliance_id)
            for appliance_id in new_ids
        )

    manager: IRMateManager | None = hass.data[DOMAIN][entry.entry_id].get("ir_manager")
    if manager is not None:
        entry.async_on_unload(manager.add_update_listener(sync_entities))
    sync_entities()

    @callback
    def handle_discovery(_data: dict[str, Any]) -> None:
        """Attach the update listener once IR support appears at runtime."""
        runtime_manager: IRMateManager | None = hass.data[DOMAIN][
            entry.entry_id
        ].get("ir_manager")
        if runtime_manager is not None:
            entry.async_on_unload(runtime_manager.add_update_listener(sync_entities))
        sync_entities()

    coordinator.device.add_discovery_callback(handle_discovery)


class SeeedIRClimate(CoordinatorEntity, ClimateEntity):
    """A Home Assistant thermostat that emits full air-conditioner frames."""

    _attr_has_entity_name = True
    _attr_temperature_unit = UnitOfTemperature.CELSIUS
    _enable_turn_on_off_backwards_compatibility = False
    _remove_listener: CALLBACK_TYPE | None = None

    def __init__(
        self,
        coordinator: SeeedHACoordinator,
        entry: ConfigEntry,
        manager: IRMateManager,
        appliance_id: str,
    ) -> None:
        """Build the entity from the appliance's climate metadata."""
        super().__init__(coordinator)
        self._entry = entry
        self._manager = manager
        self._appliance_id = appliance_id

        appliance = manager.get_climate_appliance(appliance_id)
        config = appliance.get("climate", {})
        self._attr_name = appliance.get("name", "Air conditioner")
        device_id = entry.data.get(CONF_DEVICE_ID, "")
        self._attr_unique_id = f"{device_id}_climate_{appliance_id}"

        self._attr_min_temp = float(config.get("min_temp", 16))
        self._attr_max_temp = float(config.get("max_temp", 30))
        self._attr_target_temperature_step = float(config.get("precision", 1.0))

        # Build the mode map so each HVAC mode remembers its library string.
        # 构建模式映射,让每个 HVAC 模式记住它在码库里的字符串。
        self._mode_map: dict[HVACMode, str] = {}
        for library_mode in config.get("operation_modes", []):
            hvac = _SMARTIR_TO_HVAC.get(library_mode)
            if hvac is not None:
                self._mode_map[hvac] = library_mode
        self._attr_hvac_modes = [HVACMode.OFF, *self._mode_map]

        self._attr_fan_modes = list(config.get("fan_modes", []) or [])
        self._attr_swing_modes = list(config.get("swing_modes", []) or [])

        features = (
            ClimateEntityFeature.TARGET_TEMPERATURE
            | ClimateEntityFeature.TURN_ON
            | ClimateEntityFeature.TURN_OFF
        )
        if self._attr_fan_modes:
            features |= ClimateEntityFeature.FAN_MODE
        if self._attr_swing_modes:
            features |= ClimateEntityFeature.SWING_MODE
        self._attr_supported_features = features

    async def async_added_to_hass(self) -> None:
        """Refresh whenever the stored HVAC state changes."""
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
        """Push the latest stored state to Home Assistant."""
        self.async_write_ha_state()

    @property
    def available(self) -> bool:
        """Sending frames requires an active device connection."""
        if self._appliance_id not in self._manager.list_climate_appliances():
            return False
        return self.coordinator.device.connected

    def _state(self) -> dict[str, Any]:
        """Return the current stored HVAC state."""
        return self._manager.get_hvac_state(self._appliance_id)

    @property
    def hvac_mode(self) -> HVACMode:
        """Return the current operation mode."""
        library_mode = self._state().get("hvac_mode", "off")
        if library_mode == "off":
            return HVACMode.OFF
        return _SMARTIR_TO_HVAC.get(library_mode, HVACMode.OFF)

    @property
    def target_temperature(self) -> float | None:
        """Return the target temperature."""
        temperature = self._state().get("temperature")
        return float(temperature) if temperature is not None else None

    @property
    def fan_mode(self) -> str | None:
        """Return the current fan setting."""
        return self._state().get("fan_mode")

    @property
    def swing_mode(self) -> str | None:
        """Return the current swing setting."""
        return self._state().get("swing_mode")

    async def async_set_hvac_mode(self, hvac_mode: HVACMode) -> None:
        """Set the operation mode and transmit the matching frame."""
        if hvac_mode == HVACMode.OFF:
            library_mode = "off"
        else:
            library_mode = self._mode_map.get(hvac_mode)
            if library_mode is None:
                return
        await self._manager.async_set_hvac_state(
            self._appliance_id, hvac_mode=library_mode
        )
        self.async_write_ha_state()

    async def async_set_temperature(self, **kwargs: Any) -> None:
        """Set the target temperature."""
        temperature = kwargs.get(ATTR_TEMPERATURE)
        if temperature is None:
            return
        await self._manager.async_set_hvac_state(
            self._appliance_id, temperature=temperature
        )
        self.async_write_ha_state()

    async def async_set_fan_mode(self, fan_mode: str) -> None:
        """Set the fan speed."""
        await self._manager.async_set_hvac_state(
            self._appliance_id, fan_mode=fan_mode
        )
        self.async_write_ha_state()

    async def async_set_swing_mode(self, swing_mode: str) -> None:
        """Set the swing position."""
        await self._manager.async_set_hvac_state(
            self._appliance_id, swing_mode=swing_mode
        )
        self.async_write_ha_state()

    async def async_turn_on(self) -> None:
        """Turn the unit on using the first available mode."""
        if not self._mode_map:
            return
        first_mode = next(iter(self._mode_map))
        await self.async_set_hvac_mode(first_mode)

    async def async_turn_off(self) -> None:
        """Turn the unit off."""
        await self.async_set_hvac_mode(HVACMode.OFF)

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
