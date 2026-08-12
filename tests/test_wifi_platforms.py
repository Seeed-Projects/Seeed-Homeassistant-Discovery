from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
from types import ModuleType
import unittest


PACKAGE_PATH = (
    Path(__file__).parents[1]
    / "custom_components"
    / "seeed_ha_discovery"
)
PACKAGE_NAME = "seeed_ha_discovery_platform_test"

package = ModuleType(PACKAGE_NAME)
package.__path__ = [str(PACKAGE_PATH)]
sys.modules[PACKAGE_NAME] = package


def load_module(module_name: str):
    """Load one integration module without importing Home Assistant."""

    full_name = f"{PACKAGE_NAME}.{module_name}"
    spec = importlib.util.spec_from_file_location(
        full_name, PACKAGE_PATH / f"{module_name}.py"
    )
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[full_name] = module
    spec.loader.exec_module(module)
    return module


load_module("const")
PLATFORMS = load_module("platforms")


class WiFiPlatformsForDeviceTest(unittest.TestCase):
    def test_scd41_loads_only_sensor_platform(self) -> None:
        entities = {
            "carbon_dioxide": {"type": "sensor"},
            "temperature": {"type": "sensor"},
            "humidity": {"type": "sensor"},
        }

        result = PLATFORMS.wifi_platforms_for_device(
            entities, {"model": "XIAO ESP32-C3"}
        )

        self.assertEqual(result, ["sensor"])

    def test_mixed_device_loads_reported_platforms(self) -> None:
        entities = {
            "temperature": {"type": "sensor"},
            "relay": {"type": "switch"},
        }

        result = PLATFORMS.wifi_platforms_for_device(entities, {})

        self.assertEqual(result, ["sensor", "switch"])

    def test_esp32s3_preserves_camera_endpoint_probe(self) -> None:
        result = PLATFORMS.wifi_platforms_for_device(
            {"temperature": {"type": "sensor"}},
            {"model": "XIAO ESP32-S3"},
        )

        self.assertEqual(result, ["sensor", "camera"])

    def test_full_infrared_device_loads_management_platforms(self) -> None:
        entities = {
            "ir_emitter": {"type": "infrared", "role": "emitter"},
            "ir_receiver": {"type": "infrared", "role": "receiver"},
        }

        result = PLATFORMS.wifi_platforms_for_device(entities, {})

        self.assertEqual(
            result,
            ["sensor", "remote", "select", "climate", "button"],
        )


if __name__ == "__main__":
    unittest.main()
