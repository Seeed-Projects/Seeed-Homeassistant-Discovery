from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


MODULE_PATH = (
    Path(__file__).parents[1]
    / "custom_components"
    / "seeed_ha_discovery"
    / "entity_control.py"
)
SPEC = importlib.util.spec_from_file_location("entity_control", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
ENTITY_CONTROL = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = ENTITY_CONTROL
SPEC.loader.exec_module(ENTITY_CONTROL)


class ParseEntityCommandTest(unittest.TestCase):
    def setUp(self) -> None:
        self.selected = [
            "cover.room_window",
            "switch.room_tv",
            "switch.room_left",
            "light.room_light",
            "media_player.room_speaker",
        ]

    def test_accepts_single_cover_toggle(self) -> None:
        command = ENTITY_CONTROL.parse_entity_command(
            {
                "request_id": 7,
                "action": "toggle",
                "entity_ids": ["cover.room_window"],
            },
            self.selected,
        )

        self.assertEqual(command.request_id, 7)
        self.assertEqual(command.action, "toggle")
        self.assertEqual(command.entity_ids, ("cover.room_window",))

    def test_accepts_mixed_domain_turn_off(self) -> None:
        command = ENTITY_CONTROL.parse_entity_command(
            {
                "request_id": 8,
                "action": "turn_off",
                "entity_ids": [
                    "switch.room_left",
                    "light.room_light",
                    "media_player.room_speaker",
                ],
            },
            self.selected,
        )

        self.assertEqual(len(command.entity_ids), 3)

    def test_rejects_entity_outside_subscription(self) -> None:
        with self.assertRaisesRegex(
            ENTITY_CONTROL.EntityCommandError, "entity_not_subscribed"
        ):
            ENTITY_CONTROL.parse_entity_command(
                {
                    "request_id": 9,
                    "action": "toggle",
                    "entity_ids": ["switch.unselected"],
                },
                self.selected,
            )

    def test_rejects_sensor_control(self) -> None:
        with self.assertRaisesRegex(
            ENTITY_CONTROL.EntityCommandError, "unsupported_entity_domain"
        ):
            ENTITY_CONTROL.parse_entity_command(
                {
                    "request_id": 10,
                    "action": "toggle",
                    "entity_ids": ["sensor.room_temperature"],
                },
                [*self.selected, "sensor.room_temperature"],
            )

    def test_rejects_multi_entity_toggle(self) -> None:
        with self.assertRaisesRegex(
            ENTITY_CONTROL.EntityCommandError, "toggle_requires_one_entity"
        ):
            ENTITY_CONTROL.parse_entity_command(
                {
                    "request_id": 11,
                    "action": "toggle",
                    "entity_ids": ["switch.room_tv", "switch.room_left"],
                },
                self.selected,
            )


if __name__ == "__main__":
    unittest.main()
