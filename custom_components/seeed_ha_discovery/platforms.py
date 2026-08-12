"""Platform selection helpers for Wi-Fi devices."""

from __future__ import annotations

from collections.abc import Mapping
from typing import Any

from .const import PLATFORMS


_DIRECT_ENTITY_PLATFORMS = {
    "sensor": "sensor",
    "switch": "switch",
    "camera": "camera",
}
_INFRARED_PLATFORMS = {"sensor", "remote", "select", "climate", "button"}


def wifi_platforms_for_device(
    entities: Mapping[str, Mapping[str, Any]],
    device_info: Mapping[str, Any],
) -> list[str]:
    """Return the Home Assistant platforms required by one Wi-Fi device.

    返回一个 Wi-Fi 设备实际需要加载的 Home Assistant 平台列表。
    """

    selected: set[str] = set()
    infrared_roles: set[str] = set()

    for config in entities.values():
        entity_type = config.get("type")
        platform = _DIRECT_ENTITY_PLATFORMS.get(entity_type)
        if platform is not None:
            selected.add(platform)
        if entity_type == "infrared":
            role = config.get("role")
            if isinstance(role, str):
                infrared_roles.add(role)

    if {"emitter", "receiver"}.issubset(infrared_roles):
        selected.update(_INFRARED_PLATFORMS)

    # Preserve the camera endpoint probe for ESP32-S3 camera examples that
    # expose the stream endpoint without a camera discovery entity.
    # 保留 ESP32-S3 摄像头示例的端点探测能力。
    model = str(device_info.get("model", "")).lower().replace("-", "")
    if "esp32s3" in model:
        selected.add("camera")

    return [platform for platform in PLATFORMS if platform in selected]
