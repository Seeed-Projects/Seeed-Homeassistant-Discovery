from __future__ import annotations

import asyncio
import importlib.util
from pathlib import Path
import sys
from types import ModuleType
import unittest
from unittest.mock import patch


PACKAGE_PATH = (
    Path(__file__).parents[1]
    / "custom_components"
    / "seeed_ha_discovery"
)
PACKAGE_NAME = "seeed_ha_discovery_coordinator_test"


def install_homeassistant_stubs() -> None:
    """Install the minimum Home Assistant API used by the coordinator."""

    homeassistant = ModuleType("homeassistant")
    config_entries = ModuleType("homeassistant.config_entries")
    core = ModuleType("homeassistant.core")
    helpers = ModuleType("homeassistant.helpers")
    update_coordinator = ModuleType("homeassistant.helpers.update_coordinator")

    class DataUpdateCoordinator:
        @classmethod
        def __class_getitem__(cls, _item):
            return cls

        def __init__(self, hass, logger, name) -> None:
            self.hass = hass
            self.data = None

        def async_set_updated_data(self, data) -> None:
            self.data = data

    config_entries.ConfigEntry = object
    core.HomeAssistant = object
    core.callback = lambda func: func
    update_coordinator.DataUpdateCoordinator = DataUpdateCoordinator

    sys.modules["homeassistant"] = homeassistant
    sys.modules["homeassistant.config_entries"] = config_entries
    sys.modules["homeassistant.core"] = core
    sys.modules["homeassistant.helpers"] = helpers
    sys.modules["homeassistant.helpers.update_coordinator"] = update_coordinator


def load_coordinator_module():
    """Load the coordinator with an isolated device stub."""

    install_homeassistant_stubs()
    package = ModuleType(PACKAGE_NAME)
    package.__path__ = [str(PACKAGE_PATH)]
    sys.modules[PACKAGE_NAME] = package

    const_spec = importlib.util.spec_from_file_location(
        f"{PACKAGE_NAME}.const", PACKAGE_PATH / "const.py"
    )
    assert const_spec is not None and const_spec.loader is not None
    const_module = importlib.util.module_from_spec(const_spec)
    sys.modules[const_spec.name] = const_module
    const_spec.loader.exec_module(const_module)

    device_module = ModuleType(f"{PACKAGE_NAME}.device")
    device_module.SeeedHADevice = object
    sys.modules[device_module.__name__] = device_module

    spec = importlib.util.spec_from_file_location(
        f"{PACKAGE_NAME}.coordinator", PACKAGE_PATH / "coordinator.py"
    )
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


COORDINATOR = load_coordinator_module()


class FakeDevice:
    def __init__(self, entities, report_discovery=True) -> None:
        self.entities = entities
        self.report_discovery = report_discovery
        self.state_callbacks = []
        self.discovery_callbacks = []
        self.disconnect_count = 0

    def add_state_callback(self, callback):
        self.state_callbacks.append(callback)
        return lambda: self.state_callbacks.remove(callback)

    def add_discovery_callback(self, callback):
        self.discovery_callbacks.append(callback)
        return lambda: self.discovery_callbacks.remove(callback)

    async def async_connect(self) -> bool:
        if self.report_discovery:
            payload = {"type": "discovery", "entities": list(self.entities.values())}
            for callback in tuple(self.discovery_callbacks):
                callback(payload)
        return True

    async def async_disconnect(self) -> None:
        self.disconnect_count += 1


class SeeedHACoordinatorTest(unittest.IsolatedAsyncioTestCase):
    async def test_accepts_non_empty_discovery(self) -> None:
        device = FakeDevice({"temperature": {"id": "temperature", "type": "sensor"}})
        coordinator = COORDINATOR.SeeedHACoordinator(object(), device, object())

        await coordinator.async_connect()

        self.assertEqual(coordinator.data["entities"], device.entities)
        self.assertEqual(len(device.state_callbacks), 1)
        self.assertEqual(len(device.discovery_callbacks), 1)

    async def test_accepts_empty_discovery_from_subscription_device(self) -> None:
        device = FakeDevice({})
        coordinator = COORDINATOR.SeeedHACoordinator(object(), device, object())

        await coordinator.async_connect()

        self.assertEqual(coordinator.data["entities"], {})

    async def test_rejects_discovery_timeout(self) -> None:
        device = FakeDevice({}, report_discovery=False)
        coordinator = COORDINATOR.SeeedHACoordinator(object(), device, object())

        async def raise_timeout(awaitable, timeout):
            awaitable.close()
            raise asyncio.TimeoutError

        with patch.object(
            COORDINATOR.asyncio,
            "wait_for",
            side_effect=raise_timeout,
        ):
            with self.assertRaisesRegex(TimeoutError, "discovery timed out"):
                await coordinator.async_connect()

    async def test_disconnect_removes_callbacks(self) -> None:
        device = FakeDevice({"temperature": {"id": "temperature", "type": "sensor"}})
        coordinator = COORDINATOR.SeeedHACoordinator(object(), device, object())
        await coordinator.async_connect()

        await coordinator.async_disconnect()

        self.assertEqual(device.state_callbacks, [])
        self.assertEqual(device.discovery_callbacks, [])
        self.assertEqual(device.disconnect_count, 1)


if __name__ == "__main__":
    unittest.main()
