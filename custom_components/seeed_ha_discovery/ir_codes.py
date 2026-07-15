"""
Convert vendored IR code payloads into signed microsecond timings.

将厂商红外码(Broadlink / Pronto / 原始时序)转换为设备可用的带符号微秒时序。

The device transmit protocol expects a flat list of signed durations where an
even index is a mark (positive) and an odd index is a space (negative), matching
the format produced by the firmware learning path.

设备发射协议要求一串带符号的时长:偶数位是脉冲(正数),奇数位是间隔(负数),
与固件学习返回的格式一致。
"""

from __future__ import annotations

import base64

# One Broadlink tick equals 8192/269 microseconds (~30.45 us).
# 一个 Broadlink tick 等于 8192/269 微秒(约 30.45 微秒)。
BROADLINK_TICK_US = 8192 / 269

# Pronto duration unit is derived from its frequency word (see decode_pronto).
# Pronto 的时长单位由其频率字推导(见 decode_pronto)。
PRONTO_FREQ_FACTOR = 0.241246

# Broadlink IR packets carry no carrier, so assume the common 38 kHz.
# Broadlink 红外包不带载波频率,默认使用常见的 38 kHz。
DEFAULT_CARRIER_HZ = 38000

# Drop a trailing inter-frame gap longer than this many microseconds.
# 丢弃长度超过该微秒数的末尾帧间空档。
TRAILING_GAP_TRIM_US = 20000

# Reject decoded signals that are implausibly short to transmit.
# 拒绝解码后短到无法发射的信号。
MIN_PULSE_COUNT = 6


class IRCodeError(ValueError):
    """Raised when an IR code payload cannot be decoded."""


def _b64_decode(payload: str) -> bytes:
    """Base64-decode a payload, tolerating whitespace and missing padding."""
    cleaned = "".join(payload.split())
    padding = (-len(cleaned)) % 4
    try:
        return base64.b64decode(cleaned + "=" * padding)
    except (base64.binascii.Error, ValueError) as err:
        raise IRCodeError("Invalid Base64 IR code") from err


def decode(encoding: str, payload: str) -> tuple[int, list[int]]:
    """Decode one payload into (carrier_frequency_hz, signed_timings)."""
    if not isinstance(payload, str) or not payload.strip():
        raise IRCodeError("IR code payload is empty")
    # A bracketed array is unambiguous raw timing data regardless of the
    # declared encoding, so detect it before trusting the encoding label.
    # 方括号数组本身就是明确的原始时序数据,不论声明的编码是什么都优先按 raw 解。
    if payload.lstrip().startswith("["):
        return DEFAULT_CARRIER_HZ, decode_raw(payload)
    normalized = (encoding or "").strip().lower()
    if normalized in ("base64", "b64"):
        return DEFAULT_CARRIER_HZ, decode_broadlink(_b64_decode(payload))
    if normalized == "hex":
        cleaned = payload.replace(" ", "").replace(",", "").replace("0x", "")
        return DEFAULT_CARRIER_HZ, decode_broadlink(bytes.fromhex(cleaned))
    if normalized == "pronto":
        return decode_pronto(payload)
    if normalized in ("raw", "esphome"):
        return DEFAULT_CARRIER_HZ, decode_raw(payload)
    raise IRCodeError(f"Unsupported IR code encoding: {encoding}")


def decode_broadlink(packet: bytes) -> list[int]:
    """Convert one Broadlink IR packet into signed microsecond timings."""
    if len(packet) < 4:
        raise IRCodeError("Broadlink packet is too short")

    length = packet[2] | (packet[3] << 8)
    data = packet[4 : 4 + length] if length else packet[4:]

    pulses: list[float] = []
    index = 0
    count = len(data)
    while index < count:
        value = data[index]
        if value == 0:
            if index + 2 >= count:
                break
            value = (data[index + 1] << 8) | data[index + 2]
            index += 3
        else:
            index += 1
        pulses.append(value * BROADLINK_TICK_US)

    return _finalize(pulses)


def decode_pronto(payload: str) -> tuple[int, list[int]]:
    """Convert one Pronto hex string into (carrier_frequency_hz, timings)."""
    try:
        words = [int(chunk, 16) for chunk in payload.split()]
    except ValueError as err:
        raise IRCodeError("Pronto code contains non-hex words") from err
    if len(words) < 4 or words[0] != 0:
        raise IRCodeError("Only raw (0000) Pronto codes are supported")

    unit_us = words[1] * PRONTO_FREQ_FACTOR
    if unit_us <= 0:
        raise IRCodeError("Pronto frequency word is invalid")
    carrier = int(round(1000000 / unit_us))

    once_pairs = words[2]
    repeat_pairs = words[3]
    burst = words[4:]
    # Prefer the one-shot sequence, falling back to the repeat sequence.
    # 优先使用一次性序列,没有则回退到重复序列。
    pair_count = once_pairs or repeat_pairs
    burst = burst[: pair_count * 2] if pair_count else burst

    pulses = [value * unit_us for value in burst]
    return carrier, _finalize(pulses)


def decode_raw(payload: str) -> list[int]:
    """Convert a raw microsecond array (ESPHome style) into signed timings."""
    tokens = (
        payload.replace("[", " ")
        .replace("]", " ")
        .replace(",", " ")
        .split()
    )
    try:
        values = [abs(int(token)) for token in tokens]
    except ValueError as err:
        raise IRCodeError("Raw code contains non-integer values") from err
    return _finalize([float(value) for value in values])


def _finalize(pulses: list[float]) -> list[int]:
    """Round, trim a trailing gap, and apply the signed mark/space layout."""
    if len(pulses) < MIN_PULSE_COUNT:
        raise IRCodeError("Decoded IR signal is too short")
    rounded = [max(1, int(round(value))) for value in pulses]
    # A final space longer than a frame gap only delays completion; drop it.
    # 末尾一个超过帧间间隔的空档只会拖延发射结束,去掉它。
    if len(rounded) % 2 == 0 and rounded[-1] > TRAILING_GAP_TRIM_US:
        rounded.pop()
    return [value if index % 2 == 0 else -value for index, value in enumerate(rounded)]
