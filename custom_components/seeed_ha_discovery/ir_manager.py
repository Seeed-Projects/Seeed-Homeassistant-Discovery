"""IR Mate appliance, learned command, touch binding, and sync management."""

from __future__ import annotations

import asyncio
from collections.abc import Callable
from copy import deepcopy
import json
from pathlib import Path
import re
from typing import Any
from uuid import uuid4

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers.storage import Store

from .const import CONF_DEVICE_ID, DEFAULT_IR_CARRIER_FREQUENCY, DOMAIN
from .device import SeeedHADevice

STORAGE_VERSION = 1
GESTURES = ("single", "double", "triple", "long")
DEFAULT_APPLIANCE_ID = "factory_gree"
DEFAULT_PROFILE_ID = "gree_yan_yaw1f"


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


def _binding(appliance_id: str, command_id: str) -> dict[str, str]:
    """Build one touch binding reference."""
    return {"appliance_id": appliance_id, "command_id": command_id}


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
        # Listeners notified whenever appliances, commands, or the active
        # appliance change, so select entities can refresh their options.
        # 当电器、指令或当前电器变化时通知的监听器，供下拉框实体刷新选项。
        self._update_listeners: list[Callable[[], None]] = []

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
                        "learned": bool(command.get("signals"))
                        or command["source"] == "builtin",
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
            sendable = command.get("source") == "builtin" or bool(
                command.get("signals")
            )
            if sendable:
                commands.append((command["id"], command["name"]))
        return commands

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
        if command.get("source") == "builtin":
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
            if command.get("source") == "unlearned" or not (
                command.get("source") == "builtin" or command.get("signals")
            ):
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
        command = self._get_command(appliance_id, command_id)
        try:
            if command.get("source") == "builtin":
                await self.device.async_execute_ir_builtin(
                    command["builtin_action"]
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


def _load_profiles() -> dict[str, Any]:
    """Load the bundled infrared profile catalog from disk."""
    path = Path(__file__).with_name("ir_profiles.json")
    with path.open(encoding="utf-8") as profile_file:
        data = json.load(profile_file)
    if not isinstance(data, dict):
        raise ValueError("Infrared profile catalog must contain an object")
    return data
