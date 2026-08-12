from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
from types import ModuleType, SimpleNamespace
import unittest
from unittest.mock import patch


PACKAGE_PATH = (
    Path(__file__).parents[1]
    / "custom_components"
    / "seeed_ha_discovery"
)
PACKAGE_NAME = "seeed_ha_discovery_entry_test"


class ConfigEntryNotReady(Exception):
    pass


def install_homeassistant_stubs() -> None:
    """Install the minimum Home Assistant API used by the integration entry."""

    homeassistant = sys.modules.setdefault(
        "homeassistant", ModuleType("homeassistant")
    )
    config_entries = sys.modules.setdefault(
        "homeassistant.config_entries",
        ModuleType("homeassistant.config_entries"),
    )
    core = sys.modules.setdefault(
        "homeassistant.core", ModuleType("homeassistant.core")
    )
    exceptions = sys.modules.setdefault(
        "homeassistant.exceptions", ModuleType("homeassistant.exceptions")
    )
    config_entries.ConfigEntry = object
    core.HomeAssistant = object
    exceptions.ConfigEntryNotReady = ConfigEntryNotReady
    homeassistant.config_entries = config_entries
    homeassistant.core = core
    homeassistant.exceptions = exceptions


def load_file(module_name: str):
    """Load one module under the isolated integration package."""

    is_integration = module_name == "integration"
    full_name = PACKAGE_NAME if is_integration else f"{PACKAGE_NAME}.{module_name}"
    filename = "__init__.py" if is_integration else f"{module_name}.py"
    spec = importlib.util.spec_from_file_location(
        full_name,
        PACKAGE_PATH / filename,
        submodule_search_locations=[str(PACKAGE_PATH)] if is_integration else None,
    )
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[full_name] = module
    spec.loader.exec_module(module)
    return module


def load_integration_module():
    """Load the integration with isolated coordinator and device classes."""

    install_homeassistant_stubs()
    package = ModuleType(PACKAGE_NAME)
    package.__path__ = [str(PACKAGE_PATH)]
    sys.modules[PACKAGE_NAME] = package
    load_file("const")
    load_file("platforms")

    coordinator = ModuleType(f"{PACKAGE_NAME}.coordinator")
    coordinator.SeeedHACoordinator = object
    sys.modules[coordinator.__name__] = coordinator
    device = ModuleType(f"{PACKAGE_NAME}.device")
    device.SeeedHADevice = object
    sys.modules[device.__name__] = device
    return load_file("integration")


INTEGRATION = load_integration_module()


class FakeCoordinator:
    def __init__(self) -> None:
        self.disconnect_count = 0

    async def async_disconnect(self) -> None:
        self.disconnect_count += 1


class FakeConfigEntries:
    def __init__(self, error: Exception | None = None) -> None:
        self.error = error
        self.unloaded_platforms = []

    async def async_unload_platforms(self, entry, platforms) -> bool:
        self.unloaded_platforms.append(list(platforms))
        if self.error is not None:
            raise self.error
        return True


class FailingSetupDevice:
    def __init__(self, hass, host, port, entry) -> None:
        self.host = host


class FailingSetupCoordinator:
    latest = None

    def __init__(self, hass, device, entry) -> None:
        self.disconnect_count = 0
        FailingSetupCoordinator.latest = self

    async def async_connect(self) -> None:
        raise ConnectionError("device unavailable")

    async def async_disconnect(self) -> None:
        self.disconnect_count += 1


class WiFiEntryLifecycleTest(unittest.IsolatedAsyncioTestCase):
    def make_hass(self, coordinator, error=None):
        config_entries = FakeConfigEntries(error)
        hass = SimpleNamespace(
            data={
                "seeed_ha_discovery": {
                    "entry-1": {
                        "connection_type": "wifi",
                        "loaded_platforms": ["sensor"],
                        "coordinator": coordinator,
                    }
                }
            },
            config_entries=config_entries,
        )
        return hass, config_entries

    async def test_setup_failure_cleans_partial_runtime(self) -> None:
        hass = SimpleNamespace(data={}, config_entries=FakeConfigEntries())
        entry = SimpleNamespace(
            entry_id="entry-1",
            data={"connection_type": "wifi", "host": "192.0.2.10"},
            options={},
        )

        with patch.object(INTEGRATION, "SeeedHADevice", FailingSetupDevice), patch.object(
            INTEGRATION,
            "SeeedHACoordinator",
            FailingSetupCoordinator,
        ):
            with self.assertRaises(ConfigEntryNotReady):
                await INTEGRATION.async_setup_entry(hass, entry)

        self.assertEqual(FailingSetupCoordinator.latest.disconnect_count, 1)
        self.assertNotIn("entry-1", hass.data["seeed_ha_discovery"])

    async def test_unloads_only_recorded_platforms(self) -> None:
        coordinator = FakeCoordinator()
        hass, config_entries = self.make_hass(coordinator)
        entry = SimpleNamespace(entry_id="entry-1", data={})

        result = await INTEGRATION.async_unload_entry(hass, entry)

        self.assertTrue(result)
        self.assertEqual(config_entries.unloaded_platforms, [["sensor"]])
        self.assertEqual(coordinator.disconnect_count, 1)
        self.assertNotIn("entry-1", hass.data["seeed_ha_discovery"])

    async def test_unload_error_still_disconnects_coordinator(self) -> None:
        coordinator = FakeCoordinator()
        hass, _config_entries = self.make_hass(
            coordinator, RuntimeError("platform unload failed")
        )
        entry = SimpleNamespace(entry_id="entry-1", data={})

        result = await INTEGRATION.async_unload_entry(hass, entry)

        self.assertFalse(result)
        self.assertEqual(coordinator.disconnect_count, 1)
        self.assertNotIn("entry-1", hass.data["seeed_ha_discovery"])


if __name__ == "__main__":
    unittest.main()
