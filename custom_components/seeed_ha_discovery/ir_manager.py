"""IR Mate appliance, learned command, touch binding, and sync management."""

from __future__ import annotations

import asyncio
from collections.abc import Callable
from copy import deepcopy
import json
import logging
from pathlib import Path
import re
from typing import Any
from uuid import uuid4

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers.storage import Store
from homeassistant.util import dt as dt_util

from . import ir_library
from .const import CONF_DEVICE_ID, DEFAULT_IR_CARRIER_FREQUENCY, DOMAIN
from .device import SeeedHADevice

_LOGGER = logging.getLogger(__name__)

STORAGE_VERSION = 1
GESTURES = ("single", "double", "triple", "long")
DEFAULT_APPLIANCE_ID = "factory_gree"
DEFAULT_PROFILE_ID = "gree_yan_yaw1f"

# Appliance that holds signals learned directly into a touch gesture slot.
# 存放直接学习进触摸手势槽位的信号所用的电器。
GESTURE_CAPTURE_APPLIANCE_ID = "touch_captures"

# Prefix marking profiles generated from the bundled climate code library.
# 标记由内置空调码库动态生成的 profile 的前缀。
LIBRARY_CLIMATE_PREFIX = "lib_climate_"


def _default_data() -> dict[str, Any]:
    """Return the factory Gree appliance and touch bindings."""
    commands = {
        "power": _builtin_command("power", "Power", "gree_power"),
        "temperature_up": _builtin_command(
            "temperature_up", "Temperature up", "gree_temp_up"
        ),
        "temperature_down": _builtin_command(
            "temperature_down", "Temperature down", "gree_temp_down"
        ),
        "mode": _builtin_command("mode", "Mode", "gree_mode"),
    }
    return {
        "appliances": {
            DEFAULT_APPLIANCE_ID: {
                "id": DEFAULT_APPLIANCE_ID,
                "name": "Gree Air Conditioner",
                "category": "air_conditioner",
                "profile_id": DEFAULT_PROFILE_ID,
                "brand": "Gree",
                "model": "YAN / YAW1F",
                "source": "builtin",
                "commands": commands,
                "factory": True,
            }
        },
        "bindings": {
            "single": _binding(DEFAULT_APPLIANCE_ID, "power"),
            "double": _binding(DEFAULT_APPLIANCE_ID, "temperature_up"),
            "triple": _binding(DEFAULT_APPLIANCE_ID, "temperature_down"),
            "long": _binding(DEFAULT_APPLIANCE_ID, "mode"),
        },
        "active_appliance": DEFAULT_APPLIANCE_ID,
        "revision": 0,
        "device_revision": 0,
        "sync_error": None,
    }


def _builtin_command(command_id: str, name: str, action: str) -> dict[str, Any]:
    """Build one firmware-backed command record."""
    return {
        "id": command_id,
        "name": name,
        "source": "builtin",
        "builtin_action": action,
        "signals": [],
    }


def _managed_command(command_id: str, name: str, action: str) -> dict[str, Any]:
    """Build one Home-Assistant-computed climate action record.

    A managed command is resolved on the Home Assistant side from the
    appliance HVAC state and the bundled code library, so stateful actions
    (power toggle, temperature step, mode cycle) work while online.
    HA 端根据空调状态与码库解析的动作;开关/温度步进/模式循环等有状态动作在线可用。
    """
    return {
        "id": command_id,
        "name": name,
        "source": "managed",
        "managed_action": action,
        "signals": [],
    }


def _managed_climate_commands(climate: dict[str, Any]) -> dict[str, dict[str, Any]]:
    """Return the stateful action set available for a climate appliance."""
    commands: dict[str, dict[str, Any]] = {
        "power": _managed_command("power", "Power", "power"),
    }
    operation_modes = climate.get("operation_modes") or []
    if operation_modes:
        commands["mode"] = _managed_command("mode", "Mode", "mode")
    try:
        has_range = float(climate.get("max_temp", 30)) > float(
            climate.get("min_temp", 16)
        )
    except (TypeError, ValueError):
        has_range = True
    if has_range:
        commands["temperature_up"] = _managed_command(
            "temperature_up", "Temperature up", "temperature_up"
        )
        commands["temperature_down"] = _managed_command(
            "temperature_down", "Temperature down", "temperature_down"
        )
    if climate.get("fan_modes"):
        commands["fan"] = _managed_command("fan", "Fan speed", "fan")
    return commands


def _is_sendable(command: dict[str, Any]) -> bool:
    """Return whether a command can be transmitted right now."""
    return command.get("source") in ("builtin", "managed") or bool(
        command.get("signals")
    )


def _binding(appliance_id: str, command_id: str) -> dict[str, str]:
    """Build one touch binding reference."""
    return {"appliance_id": appliance_id, "command_id": command_id}


def _library_appliance(
    appliance_id: str, name: str, profile: dict[str, Any]
) -> dict[str, Any]:
    """Build one climate appliance backed by a bundled library code."""
    climate = dict(profile.get("climate", {}))
    return {
        "id": appliance_id,
        "name": name,
        "category": profile["category"],
        "profile_id": profile["id"],
        "brand": profile["brand"],
        "model": profile["model"],
        "source": "library",
        "library_platform": profile.get("library_platform", "climate"),
        "library_code": profile["library_code"],
        "climate": climate,
        "hvac_state": _climate_default_state(climate),
        "commands": _managed_climate_commands(climate),
        "factory": False,
    }


def _climate_default_state(climate: dict[str, Any]) -> dict[str, Any]:
    """Return a safe starting HVAC state for a new climate appliance."""
    min_temp = climate.get("min_temp", 16)
    max_temp = climate.get("max_temp", 30)
    fan_modes = climate.get("fan_modes") or []
    swing_modes = climate.get("swing_modes") or []
    return {
        "hvac_mode": "off",
        "temperature": int((float(min_temp) + float(max_temp)) / 2),
        "fan_mode": fan_modes[0] if fan_modes else None,
        "swing_mode": swing_modes[0] if swing_modes else None,
    }


def _slug(value: str) -> str:
    """Convert a user-facing name into a stable command identifier."""
    normalized = re.sub(r"[^a-z0-9]+", "_", value.lower()).strip("_")
    return normalized[:48] or f"command_{uuid4().hex[:8]}"


class IRMateManager:
    """Manage the complete Home Assistant side of one IR Mate."""

    def __init__(
        self,
        hass: HomeAssistant,
        entry: ConfigEntry,
        device: SeeedHADevice,
    ) -> None:
        """Initialize the manager and its persistent stores."""
        self.hass = hass
        self.entry = entry
        self.device = device
        device_id = entry.data.get(CONF_DEVICE_ID, entry.entry_id)
        self._store = Store(
            hass,
            STORAGE_VERSION,
            f"{DOMAIN}_{device_id}_ir_manager",
        )
        self._legacy_store = Store(
            hass,
            1,
            f"{DOMAIN}_{device_id}_ir_codes",
        )
        self._profiles: dict[str, Any] = {"categories": [], "profiles": []}
        self._data: dict[str, Any] = _default_data()
        self._loaded = False
        self._lock = asyncio.Lock()
        self._sync_lock = asyncio.Lock()
        self._alternative_indexes: dict[str, int] = {}
        # Decoded climate code files are cached per device code to avoid
        # re-reading gzip data on every command.
        # 已解压的空调码文件按设备码缓存,避免每次发指令都重新读取 gzip。
        self._climate_cache: dict[str, dict[str, Any]] = {}
        # Listeners notified whenever appliances, commands, or the active
        # appliance change, so select entities can refresh their options.
        # 当电器、指令或当前电器变化时通知的监听器，供下拉框实体刷新选项。
        self._update_listeners: list[Callable[[], None]] = []
        # Most recent learned capture and transmitted frame, exposed as
        # diagnostic entities so the learning result is no longer a black box.
        # 最近一次学习到的波形与最近一次发射的波形,以诊断实体暴露,让学习结果不再是黑盒。
        self._last_learned: dict[str, Any] | None = None
        self._last_sent: dict[str, Any] | None = None

    def add_update_listener(
        self, listener: Callable[[], None]
    ) -> Callable[[], None]:
        """Register a listener invoked after any appliance-facing change."""
        self._update_listeners.append(listener)

        def remove_listener() -> None:
            """Remove the previously registered listener."""
            if listener in self._update_listeners:
                self._update_listeners.remove(listener)

        return remove_listener

    def _notify_update(self) -> None:
        """Notify all listeners that the manager snapshot changed."""
        for listener in list(self._update_listeners):
            listener()

    @property
    def supports_ir(self) -> bool:
        """Return whether this entry represents an infrared device."""
        roles = {
            config.get("role")
            for config in self.device.entities.values()
            if config.get("type") == "infrared"
        }
        model = str(self.device.device_info.get("model", "")).lower()
        name = str(self.device.device_info.get("name", "")).lower()
        return {"emitter", "receiver"}.issubset(roles) or "ir mate" in f"{model} {name}"

    async def async_initialize(self) -> None:
        """Load profiles, stored appliances, and legacy learned commands."""
        if self._loaded:
            return
        self._profiles = await self.hass.async_add_executor_job(_load_profiles)
        stored = await self._store.async_load()
        if isinstance(stored, dict):
            self._data = stored
            self._ensure_schema()
        else:
            self._data = _default_data()
            await self._async_migrate_legacy_codes()
            await self._store.async_save(self._data)
        # Handle physical gestures reported by the device (online stateful path).
        # 处理设备上报的物理手势(在线有状态路径)。
        self.device.add_infrared_gesture_callback(self.async_handle_gesture)
        self._loaded = True

    async def async_snapshot(self, refresh_status: bool = False) -> dict[str, Any]:
        """Return a frontend-safe snapshot without raw timing payloads."""
        await self.async_initialize()
        if refresh_status and self.device.connected:
            await self.async_refresh_device_status()

        appliances: list[dict[str, Any]] = []
        for appliance in self._data["appliances"].values():
            commands = []
            for command in appliance.get("commands", {}).values():
                commands.append(
                    {
                        "id": command["id"],
                        "name": command["name"],
                        "source": command["source"],
                        "learned": _is_sendable(command),
                        "signal_count": len(command.get("signals", [])),
                    }
                )
            appliance_copy = {
                key: value
                for key, value in appliance.items()
                if key != "commands"
            }
            appliance_copy["commands"] = commands
            appliances.append(appliance_copy)

        revision = int(self._data.get("revision", 0))
        device_revision = int(self._data.get("device_revision", 0))
        if not self.device.connected:
            sync_status = "offline"
        elif self._data.get("sync_error"):
            sync_status = "error"
        elif revision == device_revision:
            sync_status = "synced"
        else:
            sync_status = "pending"

        return {
            "entry_id": self.entry.entry_id,
            "name": self.entry.title,
            "connected": self.device.connected,
            "profiles": self._profiles,
            "appliances": appliances,
            "active_appliance": self.get_active_appliance_id(),
            "bindings": deepcopy(self._data["bindings"]),
            "revision": revision,
            "device_revision": device_revision,
            "sync_status": sync_status,
            "sync_error": self._data.get("sync_error"),
        }

    def get_active_appliance_id(self) -> str:
        """Return the currently selected appliance id (never missing)."""
        appliances = self._data.get("appliances", {})
        active = self._data.get("active_appliance", DEFAULT_APPLIANCE_ID)
        if active in appliances:
            return active
        if DEFAULT_APPLIANCE_ID in appliances:
            return DEFAULT_APPLIANCE_ID
        return next(iter(appliances), "")

    def list_appliances(self) -> list[tuple[str, str]]:
        """Return (id, name) pairs for every configured appliance."""
        return [
            (appliance["id"], appliance["name"])
            for appliance in self._data.get("appliances", {}).values()
        ]

    def list_sendable_commands(self, appliance_id: str) -> list[tuple[str, str]]:
        """Return (id, name) pairs for commands that can be transmitted now."""
        appliance = self._data.get("appliances", {}).get(appliance_id)
        if not appliance:
            return []
        commands = []
        for command in appliance.get("commands", {}).values():
            if _is_sendable(command):
                commands.append((command["id"], command["name"]))
        return commands

    def list_bindable_commands(self) -> list[tuple[str, str, str]]:
        """Return (appliance_id, command_id, label) for every gesture target."""
        result = []
        for appliance in self._data.get("appliances", {}).values():
            for command in appliance.get("commands", {}).values():
                if _is_sendable(command):
                    label = f"{appliance['name']} \u00b7 {command['name']}"
                    result.append((appliance["id"], command["id"], label))
        return result

    def get_binding_label(self, gesture: str) -> str | None:
        """Return the display label of the command bound to a gesture."""
        binding = self._data.get("bindings", {}).get(gesture)
        if not binding:
            return None
        appliance_id = binding.get("appliance_id")
        command_id = binding.get("command_id")
        for bound_appliance, bound_command, label in self.list_bindable_commands():
            if bound_appliance == appliance_id and bound_command == command_id:
                return label
        return None

    def get_binding_detail(self, gesture: str) -> dict[str, Any] | None:
        """Return the stored signal details of a gesture's bound command."""
        binding = self._data.get("bindings", {}).get(gesture)
        if not binding:
            return None
        try:
            command = self._get_command(
                binding["appliance_id"], binding["command_id"]
            )
        except HomeAssistantError:
            return None
        detail: dict[str, Any] = {
            "source": command.get("source"),
            "command": command.get("name"),
            "label": self.get_binding_label(gesture),
        }
        signals = command.get("signals", [])
        if signals:
            signal = signals[0]
            detail["carrier_frequency"] = int(signal["carrier_frequency"])
            detail["timings"] = list(signal["timings"])
            detail["pulse_count"] = len(signal["timings"])
        elif command.get("source") == "builtin":
            detail["builtin_action"] = command.get("builtin_action")
        elif command.get("source") == "managed":
            detail["managed_action"] = command.get("managed_action")
            detail["computed_by"] = "home_assistant"
        return detail

    def get_last_learned(self) -> dict[str, Any] | None:
        """Return the most recent learned capture, if any."""
        return deepcopy(self._last_learned)

    def get_last_transmitted(self) -> dict[str, Any] | None:
        """Return the most recent transmitted frame, if any."""
        return deepcopy(self._last_sent)

    def _record_transmission(
        self,
        *,
        label: str,
        carrier: int | None,
        timings: list[int],
        builtin_action: str | None = None,
    ) -> None:
        """Store the latest transmitted frame and notify diagnostic entities."""
        record: dict[str, Any] = {
            "label": label,
            "carrier_frequency": int(carrier) if carrier else None,
            "timings": list(timings),
            "pulse_count": len(timings),
            "sent_at": dt_util.utcnow().isoformat(),
        }
        if builtin_action is not None:
            record["builtin_action"] = builtin_action
        self._last_sent = record
        self._notify_update()

    async def async_set_gesture_binding(
        self,
        gesture: str,
        appliance_id: str | None,
        command_id: str | None,
    ) -> dict[str, Any]:
        """Bind one gesture to a command and sync all four gestures to NVS."""
        await self.async_initialize()
        if gesture not in GESTURES:
            raise HomeAssistantError(f"Unsupported touch gesture: {gesture}")
        new_bindings: dict[str, dict[str, str] | None] = {
            existing_gesture: (dict(binding) if binding else None)
            for existing_gesture, binding in self._data["bindings"].items()
        }
        for known_gesture in GESTURES:
            new_bindings.setdefault(known_gesture, None)
        if appliance_id is None or command_id is None:
            new_bindings[gesture] = None
        else:
            new_bindings[gesture] = {
                "appliance_id": appliance_id,
                "command_id": command_id,
            }
        return await self.async_save_bindings(new_bindings)

    def list_climate_appliances(self) -> list[str]:
        """Return the id of every library-backed climate appliance."""
        return [
            appliance["id"]
            for appliance in self._data.get("appliances", {}).values()
            if appliance.get("library_platform") == "climate"
            and appliance.get("library_code")
        ]

    def get_climate_appliance(self, appliance_id: str) -> dict[str, Any]:
        """Return one climate appliance record."""
        appliance = self._get_appliance(appliance_id)
        if appliance.get("library_platform") != "climate":
            raise HomeAssistantError(f"Appliance {appliance_id} is not a climate device")
        return appliance

    def get_climate_config(self, appliance_id: str) -> dict[str, Any]:
        """Return the temperature and mode metadata of a climate appliance."""
        return dict(self.get_climate_appliance(appliance_id).get("climate", {}))

    def get_hvac_state(self, appliance_id: str) -> dict[str, Any]:
        """Return the last stored HVAC state of a climate appliance."""
        appliance = self.get_climate_appliance(appliance_id)
        state = appliance.get("hvac_state")
        if not isinstance(state, dict):
            state = _climate_default_state(appliance.get("climate", {}))
            appliance["hvac_state"] = state
        return dict(state)

    async def async_set_hvac_state(
        self,
        appliance_id: str,
        *,
        hvac_mode: str | None = None,
        temperature: float | None = None,
        fan_mode: str | None = None,
        swing_mode: str | None = None,
    ) -> None:
        """Update one field of a climate state, transmit it, and persist it."""
        await self.async_initialize()
        appliance = self.get_climate_appliance(appliance_id)
        state = self.get_hvac_state(appliance_id)
        if hvac_mode is not None:
            state["hvac_mode"] = hvac_mode
        if temperature is not None:
            state["temperature"] = temperature
        if fan_mode is not None:
            state["fan_mode"] = fan_mode
        if swing_mode is not None:
            state["swing_mode"] = swing_mode

        # Adjusting temperature or fan while the unit is off only stores the
        # preference; a frame is transmitted when a mode is active or set.
        # 关机状态下改温度/风速只保存偏好;当有运行模式或切换模式时才发射整帧。
        if state.get("hvac_mode") != "off" or hvac_mode is not None:
            await self._async_send_climate(appliance["library_code"], state)
        appliance["hvac_state"] = state
        await self._store.async_save(self._data)
        self._notify_update()

    def _resolve_climate_frame(
        self, library_code: str, state: dict[str, Any]
    ) -> tuple[int, list[int]]:
        """Decode the infrared frame for one climate state (blocking)."""
        device_data = self._climate_cache.get(library_code)
        if device_data is None:
            device_data = ir_library.load_climate_device(library_code)
            self._climate_cache[library_code] = device_data
        return ir_library.resolve_climate_signal(
            device_data,
            state.get("hvac_mode", "off"),
            state.get("fan_mode"),
            state.get("temperature"),
            state.get("swing_mode"),
        )

    async def _async_send_climate(
        self, library_code: str, state: dict[str, Any]
    ) -> None:
        """Resolve a climate state into a signal and transmit it."""
        try:
            carrier, timings = await self.hass.async_add_executor_job(
                self._resolve_climate_frame, library_code, dict(state)
            )
        except ir_library.IRCodeError as err:
            raise HomeAssistantError(
                f"This state is not available for the selected model: {err}"
            ) from err
        await self.device.async_transmit_infrared(carrier, timings)
        self._record_transmission(
            label=f"Air conditioner ({state.get('hvac_mode', 'off')})",
            carrier=carrier,
            timings=timings,
        )

    def _managed_state_change(
        self,
        action: str,
        state: dict[str, Any],
        climate: dict[str, Any],
    ) -> dict[str, Any]:
        """Compute the HVAC state fields changed by one managed action."""
        operation_modes = list(climate.get("operation_modes") or [])
        fan_modes = list(climate.get("fan_modes") or [])
        current_mode = state.get("hvac_mode", "off")

        if action == "power":
            if current_mode == "off":
                return {"hvac_mode": self._default_on_mode(operation_modes)}
            return {"hvac_mode": "off"}

        if action == "mode":
            if not operation_modes:
                return {}
            if current_mode == "off" or current_mode not in operation_modes:
                return {"hvac_mode": operation_modes[0]}
            index = operation_modes.index(current_mode)
            return {"hvac_mode": operation_modes[(index + 1) % len(operation_modes)]}

        if action in ("temperature_up", "temperature_down"):
            try:
                step = float(climate.get("precision", 1.0)) or 1.0
            except (TypeError, ValueError):
                step = 1.0
            min_temp = float(climate.get("min_temp", 16))
            max_temp = float(climate.get("max_temp", 30))
            current = state.get("temperature")
            current = (
                float(current)
                if current is not None
                else (min_temp + max_temp) / 2
            )
            delta = step if action == "temperature_up" else -step
            new_temp = min(max(current + delta, min_temp), max_temp)
            if float(new_temp).is_integer():
                new_temp = int(new_temp)
            return {"temperature": new_temp}

        if action == "fan":
            if not fan_modes:
                return {}
            current_fan = state.get("fan_mode")
            if current_fan in fan_modes:
                index = fan_modes.index(current_fan)
                return {"fan_mode": fan_modes[(index + 1) % len(fan_modes)]}
            return {"fan_mode": fan_modes[0]}

        raise HomeAssistantError(f"Unknown managed action: {action}")

    @staticmethod
    def _default_on_mode(operation_modes: list[str]) -> str:
        """Return a sensible operation mode to use when powering on."""
        for preferred in ("cool", "heat", "auto"):
            if preferred in operation_modes:
                return preferred
        return operation_modes[0] if operation_modes else "cool"

    async def _async_execute_managed(self, appliance_id: str, action: str) -> None:
        """Compute and transmit one Home-Assistant-managed climate action."""
        appliance = self.get_climate_appliance(appliance_id)
        climate = appliance.get("climate", {})
        state = self.get_hvac_state(appliance_id)
        changes = self._managed_state_change(action, state, climate)
        if not changes:
            return
        await self.async_set_hvac_state(appliance_id, **changes)

    def _managed_fallback_signal(
        self, appliance_id: str, action: str
    ) -> dict[str, Any] | None:
        """Resolve a single fixed frame used when the device is offline."""
        try:
            appliance = self.get_climate_appliance(appliance_id)
        except HomeAssistantError:
            return None
        climate = appliance.get("climate", {})
        state = dict(self.get_hvac_state(appliance_id))
        try:
            changes = self._managed_state_change(action, state, climate)
        except HomeAssistantError:
            return None
        state.update(changes)
        powers_off = action == "power" and changes.get("hvac_mode") == "off"
        if state.get("hvac_mode", "off") == "off" and not powers_off:
            return None
        try:
            carrier, timings = self._resolve_climate_frame(
                appliance["library_code"], state
            )
        except ir_library.IRCodeError:
            return None
        return {"carrier_frequency": int(carrier), "timings": list(timings)}

    async def async_handle_gesture(self, gesture: str) -> None:
        """Execute a physical gesture reported by the device while online."""
        await self.async_initialize()
        if gesture not in GESTURES:
            return
        binding = self._data.get("bindings", {}).get(gesture)
        if not binding:
            return
        try:
            command = self._get_command(
                binding["appliance_id"], binding["command_id"]
            )
        except HomeAssistantError:
            return
        # Only managed commands are computed by Home Assistant; the device
        # runs built-in and learned bindings on its own, so nothing else here.
        # 只有 managed 指令由 HA 计算;内置与学习类绑定由设备本地执行,此处不处理。
        if command.get("source") != "managed":
            return
        try:
            await self._async_execute_managed(
                binding["appliance_id"], command["managed_action"]
            )
        except HomeAssistantError as err:
            _LOGGER.warning("Managed gesture '%s' failed: %s", gesture, err)

    async def async_learn_gesture(self, gesture: str, timeout: int = 30) -> dict[str, Any]:
        """Learn one signal and bind it directly to a touch gesture."""
        await self.async_initialize()
        if gesture not in GESTURES:
            raise HomeAssistantError(f"Unsupported touch gesture: {gesture}")
        if not self.device.connected:
            raise HomeAssistantError("The device must be online to learn a signal")
        self._ensure_capture_appliance()
        label = f"{gesture.capitalize()} tap" if gesture != "long" else "Long press"
        await self.async_learn_command(
            GESTURE_CAPTURE_APPLIANCE_ID, gesture, label, timeout
        )
        return await self.async_set_gesture_binding(
            gesture, GESTURE_CAPTURE_APPLIANCE_ID, gesture
        )

    def _ensure_capture_appliance(self) -> dict[str, Any]:
        """Create the hidden appliance that stores learned gesture signals."""
        appliances = self._data["appliances"]
        if GESTURE_CAPTURE_APPLIANCE_ID not in appliances:
            appliances[GESTURE_CAPTURE_APPLIANCE_ID] = {
                "id": GESTURE_CAPTURE_APPLIANCE_ID,
                "name": "Learned signals",
                "category": "custom",
                "profile_id": "custom_learning",
                "brand": "Custom",
                "model": "Touch gesture captures",
                "source": "learning",
                "commands": {},
                "factory": False,
            }
        return appliances[GESTURE_CAPTURE_APPLIANCE_ID]

    async def async_set_active_appliance(self, appliance_id: str) -> None:
        """Select the appliance targeted by the send dropdown and remote."""
        await self.async_initialize()
        self._get_appliance(appliance_id)
        self._data["active_appliance"] = appliance_id
        await self._store.async_save(self._data)
        self._notify_update()

    async def async_create_appliance(
        self,
        profile_id: str,
        name: str,
    ) -> dict[str, Any]:
        """Create one controlled appliance from a catalog profile."""
        await self.async_initialize()
        name = name.strip()
        if not name:
            raise HomeAssistantError("Appliance name cannot be empty")
        profile = self._find_profile(profile_id)
        appliance_id = f"{_slug(name)}_{uuid4().hex[:6]}"

        if profile.get("source") == "library":
            self._data["appliances"][appliance_id] = _library_appliance(
                appliance_id, name, profile
            )
            await self._store.async_save(self._data)
            self._notify_update()
            return await self.async_snapshot()

        commands: dict[str, dict[str, Any]] = {}
        for profile_command in profile.get("commands", []):
            command_id = profile_command["id"]
            if profile.get("source") == "builtin":
                commands[command_id] = _builtin_command(
                    command_id,
                    profile_command["name"],
                    profile_command["builtin_action"],
                )
            else:
                commands[command_id] = {
                    "id": command_id,
                    "name": profile_command["name"],
                    "source": "unlearned",
                    "signals": [],
                }

        self._data["appliances"][appliance_id] = {
            "id": appliance_id,
            "name": name,
            "category": profile["category"],
            "profile_id": profile["id"],
            "brand": profile["brand"],
            "model": profile["model"],
            "source": profile["source"],
            "commands": commands,
            "factory": False,
        }
        await self._store.async_save(self._data)
        self._notify_update()
        return await self.async_snapshot()

    async def async_delete_appliance(self, appliance_id: str) -> dict[str, Any]:
        """Delete one user-created appliance and clear its touch bindings."""
        await self.async_initialize()
        appliance = self._get_appliance(appliance_id)
        if appliance.get("factory"):
            raise HomeAssistantError("The factory Gree appliance cannot be deleted")
        self._data["appliances"].pop(appliance_id)
        changed = False
        for gesture, binding in self._data["bindings"].items():
            if binding and binding.get("appliance_id") == appliance_id:
                self._data["bindings"][gesture] = None
                changed = True
        if self._data.get("active_appliance") == appliance_id:
            self._data["active_appliance"] = DEFAULT_APPLIANCE_ID
        if changed:
            self._mark_pending_sync()
        await self._store.async_save(self._data)
        if changed and self.device.connected:
            await self.async_sync_bindings()
        self._notify_update()
        return await self.async_snapshot()

    async def async_learn_command(
        self,
        appliance_id: str,
        command_id: str | None,
        command_name: str,
        timeout: int = 30,
        alternative: bool = False,
    ) -> dict[str, Any]:
        """Capture and persist one learned infrared command."""
        await self.async_initialize()
        appliance = self._get_appliance(appliance_id)
        target_id = command_id or _slug(command_name)
        command = appliance["commands"].get(target_id)
        if command is not None and command.get("source") == "builtin":
            raise HomeAssistantError("Built-in commands do not require learning")

        async with self._lock:
            try:
                first = await self.device.async_learn_infrared(timeout)
                signals = [self._signal_from_result(first)]
                if alternative:
                    second = await self.device.async_learn_infrared(timeout)
                    signals.append(self._signal_from_result(second))
            except (ConnectionError, RuntimeError, TimeoutError, ValueError) as err:
                raise HomeAssistantError(str(err)) from err

            appliance["commands"][target_id] = {
                "id": target_id,
                "name": command_name.strip(),
                "source": "learned",
                "signals": signals,
            }
            first_signal = signals[0]
            self._last_learned = {
                "appliance": appliance["name"],
                "command": command_name.strip(),
                "carrier_frequency": int(first_signal["carrier_frequency"]),
                "timings": list(first_signal["timings"]),
                "pulse_count": len(first_signal["timings"]),
                "captured_at": dt_util.utcnow().isoformat(),
            }
            if self._command_is_bound(appliance_id, target_id):
                self._mark_pending_sync()
            await self._store.async_save(self._data)

        if self._command_is_bound(appliance_id, target_id) and self.device.connected:
            await self.async_sync_bindings()
        self._notify_update()
        return await self.async_snapshot()

    async def async_delete_command(
        self,
        appliance_id: str,
        command_id: str,
    ) -> dict[str, Any]:
        """Delete one learned command and clear bindings that reference it."""
        await self.async_initialize()
        appliance = self._get_appliance(appliance_id)
        command = self._get_command(appliance_id, command_id)
        if command.get("source") in ("builtin", "managed"):
            raise HomeAssistantError("Built-in commands cannot be deleted")

        self._alternative_indexes.pop(f"{appliance_id}:{command_id}", None)

        if any(
            item.get("id") == command_id
            for item in self._find_profile(appliance["profile_id"]).get("commands", [])
        ):
            appliance["commands"][command_id] = {
                "id": command_id,
                "name": command["name"],
                "source": "unlearned",
                "signals": [],
            }
        else:
            appliance["commands"].pop(command_id)

        changed = self._clear_command_bindings(appliance_id, command_id)
        if changed:
            self._mark_pending_sync()
        await self._store.async_save(self._data)
        if changed and self.device.connected:
            await self.async_sync_bindings()
        self._notify_update()
        return await self.async_snapshot()

    async def async_test_command(
        self,
        appliance_id: str,
        command_id: str,
    ) -> dict[str, Any]:
        """Send one built-in or learned command immediately."""
        await self.async_initialize()
        await self._async_send_command(appliance_id, command_id)
        return await self.async_snapshot()

    async def async_save_bindings(
        self,
        bindings: dict[str, Any],
    ) -> dict[str, Any]:
        """Validate, save, and synchronize all four touch gestures."""
        await self.async_initialize()
        normalized: dict[str, dict[str, str] | None] = {}
        for gesture in GESTURES:
            value = bindings.get(gesture)
            if value is None:
                normalized[gesture] = None
                continue
            if not isinstance(value, dict):
                raise HomeAssistantError(f"Invalid binding for gesture '{gesture}'")
            appliance_id = value.get("appliance_id")
            command_id = value.get("command_id")
            command = self._get_command(appliance_id, command_id)
            if command.get("source") == "unlearned" or not _is_sendable(command):
                raise HomeAssistantError(
                    f"Command '{command_id}' must be learned before binding"
                )
            normalized[gesture] = _binding(appliance_id, command_id)

        self._data["bindings"] = normalized
        self._mark_pending_sync()
        await self._store.async_save(self._data)
        if self.device.connected:
            await self.async_sync_bindings()
        self._notify_update()
        return await self.async_snapshot()

    async def async_sync_bindings(self) -> None:
        """Write all touch bindings to device NVS as one revision."""
        await self.async_initialize()
        if not self.device.connected:
            return
        # Serialize profile writes so one revision always contains four bindings.
        # 串行写入触摸配置，确保同一版本始终包含四个完整绑定。
        async with self._sync_lock:
            revision = int(self._data["revision"])
            try:
                for index, gesture in enumerate(GESTURES):
                    payload = self._binding_payload(
                        self._data["bindings"].get(gesture)
                    )
                    result = await self.device.async_set_ir_touch_binding(
                        gesture=gesture,
                        binding=payload,
                        revision=revision,
                        final=index == len(GESTURES) - 1,
                    )
                    if index == len(GESTURES) - 1:
                        self._data["device_revision"] = int(
                            result.get("revision", revision)
                        )
                self._data["sync_error"] = None
            except (ConnectionError, RuntimeError, TimeoutError, ValueError) as err:
                self._data["sync_error"] = str(err)
                await self._store.async_save(self._data)
                raise HomeAssistantError(str(err)) from err
            await self._store.async_save(self._data)

    async def async_refresh_device_status(self) -> None:
        """Refresh the touch profile revision reported by the device."""
        if not self.device.connected:
            return
        try:
            status = await self.device.async_get_ir_touch_status()
        except (ConnectionError, RuntimeError, TimeoutError, ValueError):
            return
        self._data["device_revision"] = int(status.get("revision", 0))
        if self._data["device_revision"] == int(self._data["revision"]):
            self._data["sync_error"] = None
        await self._store.async_save(self._data)

    async def async_remote_send(
        self,
        appliance_id: str,
        command_ids: list[str],
        repeat_count: int,
        delay: float,
    ) -> None:
        """Send commands for the Home Assistant remote entity."""
        await self.async_initialize()
        transmitted = 0
        for _repeat_index in range(repeat_count):
            for command_id in command_ids:
                if transmitted:
                    await asyncio.sleep(delay)
                await self._async_send_command(appliance_id, command_id)
                transmitted += 1

    async def async_remote_learn(
        self,
        appliance_id: str,
        command_ids: list[str],
        timeout: int,
        alternative: bool,
    ) -> None:
        """Learn commands for the Home Assistant remote entity."""
        for command_id in command_ids:
            await self.async_learn_command(
                appliance_id,
                command_id,
                command_id.replace("_", " ").title(),
                timeout,
                alternative,
            )

    async def async_remote_delete(
        self,
        appliance_id: str,
        command_ids: list[str],
    ) -> None:
        """Delete commands for the Home Assistant remote entity."""
        for command_id in command_ids:
            await self.async_delete_command(appliance_id, command_id)

    async def _async_send_command(
        self,
        appliance_id: str,
        command_id: str,
    ) -> None:
        """Send one command through the matching firmware transport."""
        appliance = self._get_appliance(appliance_id)
        command = self._get_command(appliance_id, command_id)
        if command.get("source") == "managed":
            await self._async_execute_managed(
                appliance_id, command["managed_action"]
            )
            return
        label = f"{appliance['name']} \u00b7 {command['name']}"
        try:
            if command.get("source") == "builtin":
                await self.device.async_execute_ir_builtin(
                    command["builtin_action"]
                )
                self._record_transmission(
                    label=label,
                    carrier=None,
                    timings=[],
                    builtin_action=command["builtin_action"],
                )
                return
            signals = command.get("signals", [])
            if not signals:
                raise HomeAssistantError("This command has not been learned")
            key = f"{appliance_id}:{command_id}"
            alternative_index = self._alternative_indexes.get(key, 0)
            signal = signals[alternative_index % len(signals)]
            await self.device.async_transmit_infrared(
                int(signal["carrier_frequency"]),
                list(signal["timings"]),
            )
            self._record_transmission(
                label=label,
                carrier=int(signal["carrier_frequency"]),
                timings=list(signal["timings"]),
            )
            if len(signals) > 1:
                self._alternative_indexes[key] = alternative_index + 1
        except (ConnectionError, RuntimeError, TimeoutError, ValueError) as err:
            raise HomeAssistantError(str(err)) from err

    def _find_profile(self, profile_id: str) -> dict[str, Any]:
        """Return one catalog profile or raise a user-facing error."""
        for profile in self._profiles.get("profiles", []):
            if profile.get("id") == profile_id:
                return profile
        raise HomeAssistantError(f"Unknown infrared profile: {profile_id}")

    def _get_appliance(self, appliance_id: str) -> dict[str, Any]:
        """Return one configured appliance."""
        if appliance_id == "default":
            appliance_id = DEFAULT_APPLIANCE_ID
        appliance = self._data["appliances"].get(appliance_id)
        if appliance is None:
            raise HomeAssistantError(f"Unknown appliance: {appliance_id}")
        return appliance

    def _get_command(self, appliance_id: str, command_id: str) -> dict[str, Any]:
        """Return one command from a configured appliance."""
        appliance = self._get_appliance(appliance_id)
        command = appliance.get("commands", {}).get(command_id)
        if command is None:
            raise HomeAssistantError(f"Unknown infrared command: {command_id}")
        return command

    def _binding_payload(self, binding: dict[str, str] | None) -> dict[str, Any]:
        """Convert a stored binding reference into a firmware payload."""
        if binding is None:
            return {"source": "none"}
        command = self._get_command(
            binding["appliance_id"],
            binding["command_id"],
        )
        if command.get("source") == "builtin":
            return {
                "source": "builtin",
                "action": command["builtin_action"],
            }
        if command.get("source") == "managed":
            payload: dict[str, Any] = {
                "source": "managed",
                "action": command["managed_action"],
            }
            fallback = self._managed_fallback_signal(
                binding["appliance_id"], command["managed_action"]
            )
            if fallback is not None:
                payload["fallback"] = fallback
            return payload
        signals = command.get("signals", [])
        if not signals:
            raise HomeAssistantError("A bound command has not been learned")
        signal = signals[0]
        return {
            "source": "raw",
            "carrier_frequency": int(signal["carrier_frequency"]),
            "timings": list(signal["timings"]),
        }

    def _mark_pending_sync(self) -> None:
        """Increment the profile revision after a binding-affecting change."""
        self._data["revision"] = int(self._data.get("revision", 0)) + 1
        self._data["sync_error"] = None

    def _command_is_bound(self, appliance_id: str, command_id: str) -> bool:
        """Return whether any local gesture references a command."""
        return any(
            binding
            and binding.get("appliance_id") == appliance_id
            and binding.get("command_id") == command_id
            for binding in self._data["bindings"].values()
        )

    def _clear_command_bindings(self, appliance_id: str, command_id: str) -> bool:
        """Clear every gesture that references a deleted command."""
        changed = False
        for gesture, binding in self._data["bindings"].items():
            if (
                binding
                and binding.get("appliance_id") == appliance_id
                and binding.get("command_id") == command_id
            ):
                self._data["bindings"][gesture] = None
                changed = True
        return changed

    def _signal_from_result(self, result: dict[str, Any]) -> dict[str, Any]:
        """Normalize one firmware learning response for storage."""
        return {
            "carrier_frequency": int(
                result.get("carrier_frequency", DEFAULT_IR_CARRIER_FREQUENCY)
            ),
            "timings": list(result["timings"]),
        }

    async def _async_migrate_legacy_codes(self) -> None:
        """Import commands saved by the previous remote entity implementation."""
        legacy = await self._legacy_store.async_load()
        if not isinstance(legacy, dict):
            return
        for appliance_id, commands in legacy.items():
            if not isinstance(commands, dict):
                continue
            target_id = _slug(appliance_id)
            if target_id == "default":
                target_id = "legacy_default"
            target = {
                "id": target_id,
                "name": str(appliance_id).replace("_", " ").title(),
                "category": "custom",
                "profile_id": "custom_learning",
                "brand": "Custom",
                "model": "Migrated learned commands",
                "source": "learning",
                "commands": {},
                "factory": False,
            }
            for command_id, signals in commands.items():
                if not isinstance(signals, list):
                    continue
                target["commands"][command_id] = {
                    "id": command_id,
                    "name": str(command_id).replace("_", " ").title(),
                    "source": "learned",
                    "signals": signals,
                }
            if target["commands"]:
                self._data["appliances"][target_id] = target

    def _ensure_schema(self) -> None:
        """Fill missing fields when loading an older manager snapshot."""
        defaults = _default_data()
        self._data.setdefault("appliances", defaults["appliances"])
        self._data.setdefault("bindings", defaults["bindings"])
        for gesture in GESTURES:
            self._data["bindings"].setdefault(gesture, None)
        self._data.setdefault("active_appliance", DEFAULT_APPLIANCE_ID)
        self._data.setdefault("revision", 0)
        self._data.setdefault("device_revision", 0)
        self._data.setdefault("sync_error", None)
        self._ensure_managed_commands()

    def _ensure_managed_commands(self) -> None:
        """Add stateful managed actions to library climate appliances."""
        for appliance in self._data.get("appliances", {}).values():
            if appliance.get("library_platform") != "climate":
                continue
            commands = appliance.setdefault("commands", {})
            for command_id, command in _managed_climate_commands(
                appliance.get("climate", {})
            ).items():
                commands.setdefault(command_id, command)


def _load_profiles() -> dict[str, Any]:
    """Load the profile catalog, merging bundled climate library devices."""
    path = Path(__file__).with_name("ir_profiles.json")
    with path.open(encoding="utf-8") as profile_file:
        data = json.load(profile_file)
    if not isinstance(data, dict):
        raise ValueError("Infrared profile catalog must contain an object")
    data.setdefault("profiles", [])
    data["profiles"].extend(_library_climate_profiles())
    return data


def _library_climate_profiles() -> list[dict[str, Any]]:
    """Turn each bundled climate code file into a selectable profile."""
    profiles: list[dict[str, Any]] = []
    for device in ir_library.load_climate_index():
        models = device.get("models") or ["Unknown"]
        profiles.append(
            {
                "id": f"{LIBRARY_CLIMATE_PREFIX}{device['code']}",
                "category": "air_conditioner",
                "brand": device.get("manufacturer", "Unknown"),
                "model": models[0],
                "source": "library",
                "library_platform": "climate",
                "library_code": device["code"],
                "climate": {
                    "min_temp": device.get("minTemperature", 16),
                    "max_temp": device.get("maxTemperature", 30),
                    "precision": device.get("precision", 1.0),
                    "operation_modes": device.get("operationModes", []),
                    "fan_modes": device.get("fanModes", []),
                    "swing_modes": device.get("swingModes") or [],
                },
                "commands": [],
            }
        )
    return profiles
