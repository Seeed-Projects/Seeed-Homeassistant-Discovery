"""
Access the vendored SmartIR climate code database.

访问随插件打包的 SmartIR 空调码库。

The catalog is a small index plus one gzip-compressed JSON per device code.
Only the selected device file is decompressed on demand, so memory stays low.

码库由一个小索引 + 每个设备码一个 gzip 压缩的 JSON 组成。
只有被选中的设备文件才会按需解压,内存占用很低。
"""

from __future__ import annotations

import gzip
import json
from pathlib import Path
from typing import Any

from .ir_codes import IRCodeError, decode

_CODES_DIR = Path(__file__).with_name("codes")
_CLIMATE_DIR = _CODES_DIR / "climate"
_CLIMATE_INDEX = _CODES_DIR / "climate_index.json"


def load_climate_index() -> list[dict[str, Any]]:
    """Return the list of available climate device descriptors."""
    with _CLIMATE_INDEX.open(encoding="utf-8") as index_file:
        data = json.load(index_file)
    return data.get("devices", [])


def load_climate_device(code: str) -> dict[str, Any]:
    """Decompress and parse one climate device code file."""
    path = _CLIMATE_DIR / f"{code}.json.gz"
    if not path.is_file():
        raise IRCodeError(f"Unknown climate device code: {code}")
    with gzip.open(path, "rt", encoding="utf-8") as device_file:
        return json.load(device_file)


def resolve_climate_signal(
    device: dict[str, Any],
    hvac_mode: str,
    fan_mode: str | None,
    temperature: float | None,
    swing_mode: str | None = None,
) -> tuple[int, list[int]]:
    """Select the matching code for a climate state and decode it."""
    code = _select_climate_code(
        device, hvac_mode, fan_mode, temperature, swing_mode
    )
    return decode(device.get("commandsEncoding", ""), code)


def _select_climate_code(
    device: dict[str, Any],
    hvac_mode: str,
    fan_mode: str | None,
    temperature: float | None,
    swing_mode: str | None,
) -> str:
    """Walk the nested command tree to the code for the requested state."""
    commands = device.get("commands", {})
    if hvac_mode == "off":
        code = commands.get("off")
        if isinstance(code, str) and code.strip():
            return code
        raise IRCodeError("Device has no 'off' command")

    node = commands.get(hvac_mode)
    if node is None:
        raise IRCodeError(f"Unsupported operation mode: {hvac_mode}")
    if isinstance(node, str):
        return node

    # Fan level.
    # 风速层。
    node = _descend(node, fan_mode)
    if isinstance(node, str):
        return node

    # The next level is either temperatures or swing positions.
    # 下一层要么是温度,要么是扫风位置。
    if not _looks_numeric(node):
        node = _descend(node, swing_mode)
        if isinstance(node, str):
            return node

    return _select_temperature(node, temperature)


def _descend(node: dict[str, Any], preferred: str | None) -> Any:
    """Return the preferred child, falling back to the first available."""
    if not isinstance(node, dict) or not node:
        return node
    if preferred is not None and preferred in node:
        return node[preferred]
    lowered = {str(key).lower(): key for key in node}
    if preferred is not None and str(preferred).lower() in lowered:
        return node[lowered[str(preferred).lower()]]
    return next(iter(node.values()))


def _looks_numeric(node: dict[str, Any]) -> bool:
    """Return whether every key of a mapping is a temperature number."""
    if not isinstance(node, dict) or not node:
        return False
    for key in node:
        try:
            float(key)
        except (TypeError, ValueError):
            return False
    return True


def _select_temperature(node: dict[str, Any], temperature: float | None) -> str:
    """Pick the code for the nearest available temperature key."""
    if isinstance(node, str):
        return node
    if not isinstance(node, dict) or not node:
        raise IRCodeError("Device command tree is malformed")
    if temperature is None:
        return next(iter(node.values()))

    for candidate in (
        str(int(temperature)) if float(temperature).is_integer() else None,
        f"{float(temperature):.1f}",
        str(temperature),
    ):
        if candidate is not None and candidate in node:
            value = node[candidate]
            if isinstance(value, str):
                return value

    # Fall back to the numerically closest temperature key.
    # 回退到数值上最接近的温度键。
    numeric_keys = []
    for key in node:
        try:
            numeric_keys.append((abs(float(key) - float(temperature)), key))
        except (TypeError, ValueError):
            continue
    if not numeric_keys:
        raise IRCodeError("No temperature codes available")
    _, nearest = min(numeric_keys, key=lambda item: item[0])
    value = node[nearest]
    if isinstance(value, str):
        return value
    raise IRCodeError("Temperature code is not a string")
