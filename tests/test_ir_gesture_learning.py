from __future__ import annotations

from copy import deepcopy
from datetime import UTC, datetime
import importlib.util
from pathlib import Path
import sys
from types import ModuleType, SimpleNamespace
import unittest


PACKAGE_PATH = (
    Path(__file__).parents[1]
    / "custom_components"
    / "seeed_ha_discovery"
)
PACKAGE_NAME = "seeed_ha_discovery_ir_gesture_test"


class HomeAssistantError(Exception):
    pass


class FakeStore:
    def __init__(self, _hass, _version, _key) -> None:
        self.saved: list[dict] = []

    async def async_load(self):
        return None

    async def async_save(self, data) -> None:
        self.saved.append(deepcopy(data))


def install_homeassistant_stubs() -> None:
    """Install the Home Assistant API surface imported by the IR manager."""

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
    helpers = sys.modules.setdefault(
        "homeassistant.helpers", ModuleType("homeassistant.helpers")
    )
    storage = sys.modules.setdefault(
        "homeassistant.helpers.storage",
        ModuleType("homeassistant.helpers.storage"),
    )
    util = sys.modules.setdefault(
        "homeassistant.util", ModuleType("homeassistant.util")
    )
    dt = sys.modules.setdefault(
        "homeassistant.util.dt", ModuleType("homeassistant.util.dt")
    )

    config_entries.ConfigEntry = object
    core.HomeAssistant = object
    exceptions.HomeAssistantError = HomeAssistantError
    storage.Store = FakeStore
    dt.utcnow = lambda: datetime(2026, 8, 15, tzinfo=UTC)
    helpers.storage = storage
    util.dt = dt
    homeassistant.config_entries = config_entries
    homeassistant.core = core
    homeassistant.exceptions = exceptions
    homeassistant.helpers = helpers
    homeassistant.util = util


def load_file(module_name: str):
    """Load one integration module inside an isolated package."""

    full_name = f"{PACKAGE_NAME}.{module_name}"
    spec = importlib.util.spec_from_file_location(
        full_name,
        PACKAGE_PATH / f"{module_name}.py",
    )
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[full_name] = module
    spec.loader.exec_module(module)
    return module


def load_ir_manager_module():
    """Load the IR manager with isolated Home Assistant and device classes."""

    install_homeassistant_stubs()
    package = ModuleType(PACKAGE_NAME)
    package.__path__ = [str(PACKAGE_PATH)]
    sys.modules[PACKAGE_NAME] = package
    load_file("const")
    load_file("ir_codes")
    load_file("ir_library")
    device = ModuleType(f"{PACKAGE_NAME}.device")
    device.SeeedHADevice = object
    sys.modules[device.__name__] = device
    return load_file("ir_manager")


IR_MANAGER = load_ir_manager_module()


class FakeDevice:
    def __init__(self) -> None:
        self.connected = True
        self.entities = {}
        self.device_info = {"model": "IR Mate"}
        self.learn_count = 0
        self.binding_calls: list[dict] = []

    async def async_learn_infrared(self, _timeout: int) -> dict:
        self.learn_count += 1
        return {
            "success": True,
            "carrier_frequency": 38000,
            "timings": [1000 + self.learn_count, -500, 600, -1600],
        }

    async def async_set_ir_touch_binding(
        self,
        gesture: str,
        binding: dict,
        revision: int,
        final: bool,
    ) -> dict:
        self.binding_calls.append(
            {
                "gesture": gesture,
                "binding": deepcopy(binding),
                "revision": revision,
                "final": final,
            }
        )
        return {"success": True, "revision": revision}


class IRGestureLearningTest(unittest.IsolatedAsyncioTestCase):
    def make_manager(self):
        device = FakeDevice()
        entry = SimpleNamespace(
            data={"device_id": "ir-mate-test"},
            entry_id="entry-1",
            title="IR Mate",
        )
        manager = IR_MANAGER.IRMateManager(SimpleNamespace(), entry, device)
        manager._loaded = True
        return manager, device

    async def test_first_and_repeated_learning_sync_once_for_every_gesture(
        self,
    ) -> None:
        for gesture in IR_MANAGER.GESTURES:
            with self.subTest(gesture=gesture):
                manager, device = self.make_manager()

                await manager.async_learn_gesture(gesture)
                self.assertEqual(len(device.binding_calls), 4)
                self.assertEqual(manager._data["revision"], 1)

                device.binding_calls.clear()
                await manager.async_learn_gesture(gesture)

                self.assertEqual(len(device.binding_calls), 4)
                self.assertEqual(manager._data["revision"], 2)
                self.assertEqual(
                    [call["gesture"] for call in device.binding_calls],
                    list(IR_MANAGER.GESTURES),
                )
                self.assertTrue(device.binding_calls[-1]["final"])
                target = next(
                    call
                    for call in device.binding_calls
                    if call["gesture"] == gesture
                )
                self.assertEqual(target["binding"]["source"], "raw")
                self.assertEqual(
                    target["binding"]["timings"],
                    [1002, -500, 600, -1600],
                )

    async def test_direct_relearning_still_syncs_bound_gesture(self) -> None:
        manager, device = self.make_manager()
        await manager.async_learn_gesture("single")

        device.binding_calls.clear()
        await manager.async_learn_command(
            IR_MANAGER.GESTURE_CAPTURE_APPLIANCE_ID,
            "single",
            "Single tap",
        )

        self.assertEqual(len(device.binding_calls), 4)
        self.assertEqual(manager._data["revision"], 2)
        target = next(
            call
            for call in device.binding_calls
            if call["gesture"] == "single"
        )
        self.assertEqual(
            target["binding"]["timings"],
            [1002, -500, 600, -1600],
        )


if __name__ == "__main__":
    unittest.main()
